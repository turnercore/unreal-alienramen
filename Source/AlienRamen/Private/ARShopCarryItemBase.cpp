#include "ARShopCarryItemBase.h"

#include "ARAttributeSetCore.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARScrapyardPlayerController.h"
#include "ARShopCarryComponent.h"
#include "ARShopPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

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
}

AARShopCarryItemBase::AARShopCarryItemBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void AARShopCarryItemBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyWeightToPrimitiveComponents();
}

float AARShopCarryItemBase::GetResolvedWeightKg() const
{
	return WeightKg > 0.0f ? WeightKg : ResolveDefaultWeightKg();
}

void AARShopCarryItemBase::SetWeightKg(const float NewWeightKg)
{
	if (!HasAuthority())
	{
		return;
	}

	const float SanitizedWeight = FMath::Max(0.0f, NewWeightKg);
	if (FMath::IsNearlyEqual(WeightKg, SanitizedWeight))
	{
		return;
	}

	WeightKg = SanitizedWeight;
	ApplyWeightToPrimitiveComponents();
	ForceNetUpdate();
}

void AARShopCarryItemBase::ForwardUseToController(AActor* UsingActor)
{
	AARShopPlayerController* UsingController = Cast<AARShopPlayerController>(ResolvePlayerControllerFromActor(UsingActor));

	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Shop|Carry] '%s' use-forward ignored: could not resolve AARShopPlayerController from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	UsingController->RequestShopPickupCarryItem(this);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Carry] ForwardUse routed actor='%s' controller='%s'."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController));
}

void AARShopCarryItemBase::ForwardSecondaryUseToController(AActor* UsingActor)
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

void AARShopCarryItemBase::ForwardKickToController(AActor* UsingActor)
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

void AARShopCarryItemBase::ReleaseCarryItem_Implementation()
{
	Destroy();
}

bool AARShopCarryItemBase::UseSecondaryByController_Implementation(AARPlayerController* UsingController)
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

bool AARShopCarryItemBase::UseSecondaryInWorldByController_Implementation(AARPlayerController* UsingController)
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

	const FVector KickDirection = UsingController->GetControlRotation().Vector().GetSafeNormal();
	const float KickStrength = FMath::Max(50.0f, ResolveSecondaryForceForController(UsingController));
	PhysicsPrimitive->AddImpulse(KickDirection * KickStrength, NAME_None, true);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Carry|Secondary] '%s' world-secondary kick by '%s' strength=%.1f."),
		*GetNameSafe(this),
		*GetNameSafe(UsingController),
		KickStrength);
	UsingController->NotifyInteractionActionCue(EARInteractionActionCue::Kick, this);
	return true;
}

void AARShopCarryItemBase::OnRep_WeightKg()
{
	ApplyWeightToPrimitiveComponents();
}

void AARShopCarryItemBase::ApplyWeightToPrimitiveComponents() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		if (WeightKg > 0.0f)
		{
			Primitive->SetMassOverrideInKg(NAME_None, WeightKg, true);
		}
		else
		{
			Primitive->SetMassOverrideInKg(NAME_None, 0.0f, false);
		}
	}
}

float AARShopCarryItemBase::ResolveDefaultWeightKg() const
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

void AARShopCarryItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARShopCarryItemBase, WeightKg);
}
