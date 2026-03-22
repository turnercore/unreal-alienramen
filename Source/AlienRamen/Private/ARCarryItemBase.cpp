#include "ARCarryItemBase.h"

#include "ARAttributeSetCore.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARShopCarryComponent.h"
#include "ARShopPlayerController.h"
#include "ARScrapyardPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace
{
	static UPrimitiveComponent* ResolveCarryPhysicsPrimitive(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			return RootPrimitive;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				return Primitive;
			}
		}

		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				return Primitive;
			}
		}

		return PrimitiveComponents.Num() > 0 ? PrimitiveComponents[0] : nullptr;
	}

	static float ResolveSecondaryForceForController(const AARPlayerController* Controller)
	{
		float Strength = 10.0f;
		const AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetASC() : nullptr;
		if (ASC)
		{
			Strength = ASC->GetNumericAttribute(UARAttributeSetCore::GetStrengthAttribute());
		}

		return FMath::Max(0.0f, Strength) * 100.0f;
	}

	static AARPlayerController* ResolvePlayerControllerFromActor(AActor* UsingActor)
	{
		AARPlayerController* UsingController = Cast<AARPlayerController>(UsingActor);
		if (!UsingController)
		{
			const APawn* UsingPawn = Cast<APawn>(UsingActor);
			UsingController = UsingPawn ? Cast<AARPlayerController>(UsingPawn->GetController()) : nullptr;
		}

		return UsingController;
	}

	static float ResolveSlapCueMinHeightDeltaCm(const AARPlayerController* Controller)
	{
		const FFloatProperty* SlapCueThresholdProperty = FindFProperty<FFloatProperty>(AARPlayerController::StaticClass(), TEXT("SlapCueMinHeightDeltaCm"));
		return (Controller && SlapCueThresholdProperty)
			? FMath::Max(0.0f, SlapCueThresholdProperty->GetPropertyValue_InContainer(Controller))
			: 80.0f;
	}
}

AARCarryItemBase::AARCarryItemBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void AARCarryItemBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyWeightToPrimitiveComponents();
	RefreshVisualModelActor();
}

void AARCarryItemBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyVisualModelActor();
	Super::EndPlay(EndPlayReason);
}

float AARCarryItemBase::GetResolvedWeightKg() const
{
	return WeightKg > 0.0f ? WeightKg : ResolveDefaultWeightKg();
}

void AARCarryItemBase::SetWeightKg(const float NewWeightKg)
{
	if (!HasAuthority())
	{
		return;
	}

	const float SanitizedWeight = FMath::Max(0.0f, NewWeightKg);
	if (SanitizedWeight > 0.0f && FMath::IsNearlyEqual(WeightKg, SanitizedWeight))
	{
		return;
	}

	WeightKg = SanitizedWeight;
	ApplyWeightToPrimitiveComponents();
	ForceNetUpdate();
}

void AARCarryItemBase::ForwardUseToController(AActor* UsingActor)
{
	AARPlayerController* UsingController = ResolvePlayerControllerFromActor(UsingActor);
	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Carry] '%s' use-forward ignored: could not resolve AARPlayerController from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	if (AARShopPlayerController* ShopController = Cast<AARShopPlayerController>(UsingController))
	{
		ShopController->RequestShopPickupCarryItem(this);
		return;
	}

	if (AARScrapyardPlayerController* ScrapyardController = Cast<AARScrapyardPlayerController>(UsingController))
	{
		ScrapyardController->RequestScrapyardPickupCarryItem(this);
		return;
	}

	UE_LOG(
		ARLog,
		Warning,
		TEXT("[Carry] '%s' use-forward ignored: unsupported controller '%s'."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController));
}

void AARCarryItemBase::ForwardSecondaryUseToController(AActor* UsingActor)
{
	AARPlayerController* UsingController = ResolvePlayerControllerFromActor(UsingActor);
	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Carry|Secondary] '%s' secondary-forward ignored: could not resolve AARPlayerController from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	APawn* ControllerPawn = UsingController->GetPawn();
	const UARShopCarryComponent* CarryComponent = ControllerPawn ? ControllerPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	const bool bHeldByController = CarryComponent && CarryComponent->GetHeldActor() == this;
	if (!bHeldByController)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Carry|Secondary] '%s' secondary-forward ignored: not held by controller '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingController));
		return;
	}

	const bool bHandled = UseSecondaryByController(UsingController);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Carry|Secondary] ForwardSecondaryUse actor='%s' controller='%s' handled=%d."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController),
		bHandled ? 1 : 0);
}

void AARCarryItemBase::ForwardKickToController(AActor* UsingActor)
{
	AARPlayerController* UsingController = ResolvePlayerControllerFromActor(UsingActor);
	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Carry|Kick] '%s' kick-forward ignored: could not resolve AARPlayerController from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	UsingController->RequestKickActor(this);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Carry|Kick] ForwardKick routed actor='%s' controller='%s'."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController));
}

void AARCarryItemBase::ReleaseCarryItem_Implementation()
{
	Destroy();
}

bool AARCarryItemBase::UseSecondaryByController_Implementation(AARPlayerController* UsingController)
{
	if (!UsingController)
	{
		return false;
	}

	if (AARShopPlayerController* ShopController = Cast<AARShopPlayerController>(UsingController))
	{
		ShopController->RequestShopThrowHeldCarryItem();
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Carry|Secondary] '%s' routed held-secondary to shop throw via '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(ShopController));
		return true;
	}

	if (AARScrapyardPlayerController* ScrapyardController = Cast<AARScrapyardPlayerController>(UsingController))
	{
		ScrapyardController->RequestScrapyardThrowHeldCarryItem();
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Carry|Secondary] '%s' routed held-secondary to scrapyard throw via '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(ScrapyardController));
		return true;
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Carry|Secondary] '%s' ignored secondary use: unsupported controller '%s'."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController));
	return false;
}

bool AARCarryItemBase::UseSecondaryInWorldByController_Implementation(AARPlayerController* UsingController)
{
	if (!HasAuthority() || !UsingController)
	{
		return false;
	}

	const USceneComponent* Root = GetRootComponent();
	const AActor* AttachParentActor = Root && Root->GetAttachParent() ? Root->GetAttachParent()->GetOwner() : nullptr;
	if (AttachParentActor)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Carry|Secondary] '%s' world-secondary ignored: attached to '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(AttachParentActor));
		return false;
	}

	UPrimitiveComponent* PhysicsPrimitive = ResolveCarryPhysicsPrimitive(this);
	if (!PhysicsPrimitive)
	{
		return false;
	}

	if (!PhysicsPrimitive->IsSimulatingPhysics())
	{
		PhysicsPrimitive->SetSimulatePhysics(true);
	}

	PhysicsPrimitive->SetEnableGravity(true);
	PhysicsPrimitive->WakeAllRigidBodies();

	const float KickStrength = FMath::Max(50.0f, ResolveSecondaryForceForController(UsingController));
	const FVector KickDirection = UsingController->GetControlRotation().Vector().GetSafeNormal();
	PhysicsPrimitive->AddImpulse(KickDirection * KickStrength, NAME_None, true);

	const APawn* ControlledPawn = UsingController->GetPawn();
	const float PawnZ = ControlledPawn ? ControlledPawn->GetActorLocation().Z : 0.0f;
	FVector TargetOrigin = GetActorLocation();
	FVector TargetExtent = FVector::ZeroVector;
	GetActorBounds(true, TargetOrigin, TargetExtent);
	const float HeightDelta = TargetOrigin.Z - PawnZ;
	const float SlapCueMinHeightDeltaCm = ResolveSlapCueMinHeightDeltaCm(UsingController);
	const EARInteractionActionCue KickOrSlapCue =
		HeightDelta >= SlapCueMinHeightDeltaCm
		? EARInteractionActionCue::Slap
		: EARInteractionActionCue::Kick;

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Carry|Secondary] Applied world-secondary impulse item='%s' controller='%s' strength=%.1f heightDelta=%.1f cue=%s."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController),
		KickStrength,
		HeightDelta,
		*StaticEnum<EARInteractionActionCue>()->GetValueAsString(KickOrSlapCue));
	UsingController->NotifyInteractionActionCue(KickOrSlapCue, this);
	return true;
}

void AARCarryItemBase::SetScrapyardItemTag(const FGameplayTag NewItemTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ScrapyardItemTag == NewItemTag)
	{
		return;
	}

	ScrapyardItemTag = NewItemTag;
	ForceNetUpdate();
}

void AARCarryItemBase::SetFallbackScrapCost(const int32 NewFallbackCost)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 SanitizedCost = FMath::Max(0, NewFallbackCost);
	if (FallbackScrapCost == SanitizedCost)
	{
		return;
	}

	FallbackScrapCost = SanitizedCost;
	ForceNetUpdate();
}

void AARCarryItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARCarryItemBase, WeightKg);
	DOREPLIFETIME(AARCarryItemBase, ScrapyardItemTag);
	DOREPLIFETIME(AARCarryItemBase, FallbackScrapCost);
	DOREPLIFETIME(AARCarryItemBase, VisualModelClass);
}

void AARCarryItemBase::SetVisualModelClass(TSoftClassPtr<AActor> NewVisualModelClass)
{
	if (!HasAuthority())
	{
		return;
	}

	if (VisualModelClass == NewVisualModelClass)
	{
		return;
	}

	VisualModelClass = NewVisualModelClass;
	RefreshVisualModelActor();
	ForceNetUpdate();
}

void AARCarryItemBase::OnRep_WeightKg()
{
	ApplyWeightToPrimitiveComponents();
}

void AARCarryItemBase::OnRep_VisualModelClass()
{
	RefreshVisualModelActor();
}

void AARCarryItemBase::ApplyWeightToPrimitiveComponents() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return;
	}

	UPrimitiveComponent* TargetPrimitive = Cast<UPrimitiveComponent>(GetRootComponent());
	if (!TargetPrimitive)
	{
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				TargetPrimitive = Primitive;
				break;
			}
		}
	}

	if (!TargetPrimitive)
	{
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				TargetPrimitive = Primitive;
				break;
			}
		}
	}

	if (!TargetPrimitive)
	{
		TargetPrimitive = PrimitiveComponents[0];
	}

	const bool bUseExplicitMassOverride = WeightKg > 0.0f;
	TargetPrimitive->SetMassOverrideInKg(NAME_None, bUseExplicitMassOverride ? WeightKg : 0.0f, bUseExplicitMassOverride);
	if (TargetPrimitive->IsSimulatingPhysics())
	{
		TargetPrimitive->WakeAllRigidBodies();
	}
}

float AARCarryItemBase::ResolveDefaultWeightKg() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (const UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		const float PrimitiveMass = Primitive->GetMass();
		if (PrimitiveMass > KINDA_SMALL_NUMBER)
		{
			return PrimitiveMass;
		}
	}

	return 0.0f;
}

void AARCarryItemBase::RefreshVisualModelActor()
{
	DestroyVisualModelActor();

	if (VisualModelClass.IsNull())
	{
		return;
	}

	UClass* LoadedClass = VisualModelClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Carry] '%s' RefreshVisualModelActor: failed to load VisualModelClass '%s'."),
			*GetNameSafe(this),
			*VisualModelClass.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedVisualModelActor = World->SpawnActor<AActor>(LoadedClass, GetActorTransform(), SpawnParams);
	if (SpawnedVisualModelActor)
	{
		SpawnedVisualModelActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

void AARCarryItemBase::DestroyVisualModelActor()
{
	if (SpawnedVisualModelActor)
	{
		SpawnedVisualModelActor->Destroy();
		SpawnedVisualModelActor = nullptr;
	}
}
