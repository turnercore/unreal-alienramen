#include "ARInvaderDropBase.h"

#include "ARAttributeSetCore.h"
#include "ARInvaderCollisionChannels.h"
#include "ARGameStateBase.h"
#include "ARInvaderDirectorSettings.h"
#include "ARLog.h"
#include "ARPlayerCharacterInvader.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AARInvaderDropBase::AARInvaderDropBase()
{
	PhysicsSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PhysicsSphere"));
	SetRootComponent(PhysicsSphere);

	PhysicsSphere->SetSphereRadius(18.0f);
	PhysicsSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsSphere->SetCollisionObjectType(ARInvaderCollisionChannels::Drop);
	PhysicsSphere->SetCollisionResponseToAllChannels(ECR_Block);
	PhysicsSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	PhysicsSphere->SetCollisionResponseToChannel(ARInvaderCollisionChannels::Projectile, ECR_Ignore);
	PhysicsSphere->SetSimulatePhysics(true);
	PhysicsSphere->SetEnableGravity(false);
	PhysicsSphere->BodyInstance.bLockZTranslation = true;
	PhysicsSphere->BodyInstance.bLockXRotation = true;
	PhysicsSphere->BodyInstance.bLockYRotation = true;

	SetReplicateMovement(true);
}

void AARInvaderDropBase::BeginPlay()
{
	Super::BeginPlay();
	ResolveInvaderGravityFrameFromSettings();

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const EARInvaderDropPawnCollisionMode PawnCollisionMode = Settings
		? Settings->DropPawnCollisionMode
		: EARInvaderDropPawnCollisionMode::CollideWithPawns;

	if (PhysicsSphere)
	{
		const ECollisionResponse PawnResponse = (PawnCollisionMode == EARInvaderDropPawnCollisionMode::IgnoreAllPawns)
			? ECR_Ignore
			: ECR_Block;
		PhysicsSphere->SetCollisionResponseToChannel(ARInvaderCollisionChannels::Enemy, PawnResponse);
		PhysicsSphere->SetCollisionResponseToChannel(ARInvaderCollisionChannels::Player, PawnResponse);
		PhysicsSphere->SetCollisionResponseToChannel(ECC_Pawn, PawnResponse);
	}
}

void AARInvaderDropBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bRewardApplied)
	{
		return;
	}

	if (!HasAuthority())
	{
		if (bPredictedLocalCollectionVisual && !bIsCollecting)
		{
			PredictedLocalCollectionElapsed += FMath::Max(0.0f, DeltaSeconds);
			if (PredictedLocalCollectionElapsed >= FMath::Max(0.05f, PredictedCollectionRollbackSeconds))
			{
				RestorePostPredictionVisualState();
			}
		}
		return;
	}

	if (bIsCollecting)
	{
		TickCollection(DeltaSeconds);
		return;
	}

	if (bEarthGravityEnabled)
	{
		ApplyEarthGravityForce(DeltaSeconds);
	}
}

void AARInvaderDropBase::InitializeDrop(const EARInvaderDropType InDropType, const int32 InDropAmount, const EARAffinityColor InDropColor)
{
	if (!HasAuthority())
	{
		return;
	}

	DropType = InDropType;
	DropAmount = FMath::Max(0, InDropAmount);
	DropColor = (InDropColor == EARAffinityColor::Unknown) ? EARAffinityColor::None : InDropColor;
	ForceNetUpdate();
}

bool AARInvaderDropBase::IsAvailableForCollection() const
{
	return !bRewardApplied && !bIsCollecting && DropAmount > 0 && DropType != EARInvaderDropType::None;
}

bool AARInvaderDropBase::IsPlayerWithinPickupRange2D(const AARPlayerCharacterInvader* Player) const
{
	if (!Player || !IsAvailableForCollection())
	{
		return false;
	}

	const float PickupRadius = ResolvePickupRadiusForPlayer(Player);
	if (PickupRadius <= 0.0f)
	{
		return false;
	}

	return FVector::DistSquared2D(GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(PickupRadius);
}

bool AARInvaderDropBase::TryCollectByPlayer(AARPlayerCharacterInvader* InCollectingPlayer)
{
	if (!HasAuthority() || !InCollectingPlayer)
	{
		return false;
	}

	if (!IsPlayerWithinPickupRange2D(InCollectingPlayer))
	{
		return false;
	}

	BeginCollection(InCollectingPlayer);
	return true;
}

void AARInvaderDropBase::BeginPredictedCollectionLocal(AARPlayerCharacterInvader* InCollectingPlayer)
{
	if (HasAuthority() || !InCollectingPlayer || !InCollectingPlayer->IsLocallyControlled() || bRewardApplied || bIsCollecting || bPredictedLocalCollectionVisual)
	{
		return;
	}

	bPredictedLocalCollectionVisual = true;
	PredictedLocalCollectionElapsed = 0.0f;
	DisablePhysicsAndCollisionForCollection();
	SetActorHiddenInGame(true);
	ApplyCollectionGameplayCue(InCollectingPlayer);
	BP_OnCollectionStarted(InCollectingPlayer);
}

void AARInvaderDropBase::SetEarthGravityEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bEarthGravityEnabled = bEnabled;
}

float AARInvaderDropBase::ResolvePickupRadiusForPlayer(const AARPlayerCharacterInvader* Player) const
{
	if (!Player)
	{
		return 0.0f;
	}

	const UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!ASC)
	{
		return FMath::Max(0.0f, FallbackPickupRadius);
	}

	const float AttrRadius = FMath::Max(0.0f, ASC->GetNumericAttribute(UARAttributeSetCore::GetPickupRadiusAttribute()));
	return AttrRadius > 0.0f ? AttrRadius : FMath::Max(0.0f, FallbackPickupRadius);
}

void AARInvaderDropBase::BeginCollection(AARPlayerCharacterInvader* InCollectingPlayer)
{
	if (!HasAuthority() || bIsCollecting || !InCollectingPlayer)
	{
		return;
	}

	bIsCollecting = true;
	CollectionElapsed = 0.0f;
	CollectionStartLocation = GetActorLocation();
	CollectingPlayerPtr = InCollectingPlayer;
	DisablePhysicsAndCollisionForCollection();
	ApplyCollectionGameplayCue(InCollectingPlayer);
	BP_OnCollectionStarted(InCollectingPlayer);
	ForceNetUpdate();
}

void AARInvaderDropBase::TickCollection(const float DeltaSeconds)
{
	AARPlayerCharacterInvader* Player = CollectingPlayerPtr.Get();
	if (!Player)
	{
		ReleasePickup();
		return;
	}

	CollectionElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float Duration = FMath::Max(0.01f, CollectionDuration);
	const float Alpha = FMath::Clamp(CollectionElapsed / Duration, 0.0f, 1.0f);

	FVector TargetLocation = Player->GetActorLocation();
	TargetLocation.Z += CollectionTargetZOffset;

	const FVector NewLocation = FMath::Lerp(CollectionStartLocation, TargetLocation, Alpha);
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
	{
		FinalizeCollection();
	}
}

void AARInvaderDropBase::FinalizeCollection()
{
	if (!HasAuthority() || bRewardApplied)
	{
		return;
	}

	bRewardApplied = true;
	UE_LOG(
		ARLog,
		Log,
		TEXT("[InvaderDrop|Collect] Finalize drop='%s' type=%d amount=%d color=%d collector='%s'"),
		*GetNameSafe(this),
		static_cast<int32>(DropType),
		DropAmount,
		static_cast<int32>(DropColor),
		*GetNameSafe(CollectingPlayerPtr.Get()));
	ApplyDropReward();
	BP_OnRewardApplied(CollectingPlayerPtr.Get());
	ReleasePickup();
}

void AARInvaderDropBase::OnRep_IsCollecting()
{
	if (bIsCollecting)
	{
		DisablePhysicsAndCollisionForCollection();
	}
}

void AARInvaderDropBase::ApplyCollectionGameplayCue(AARPlayerCharacterInvader* InCollectingPlayer) const
{
	if (!InCollectingPlayer || !CollectionGameplayCueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = InCollectingPlayer->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = GetActorLocation();
	ASC->ExecuteGameplayCue(CollectionGameplayCueTag, CueParams);
}

void AARInvaderDropBase::ApplyDropReward_Implementation()
{
	if (!HasAuthority() || DropAmount <= 0 || DropType == EARInvaderDropType::None)
	{
		return;
	}

	AARGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState)
	{
		return;
	}

	if (DropType == EARInvaderDropType::Scrap)
	{
		const int32 OldScrap = GameState->GetScrap();
		const int32 NewScrap = OldScrap + DropAmount;
		GameState->SetScrapFromSave(NewScrap);
		UE_LOG(
			ARLog,
			Log,
			TEXT("[InvaderDrop|Reward] Scrap +%d old=%d new=%d drop='%s'"),
			DropAmount,
			OldScrap,
			GameState->GetScrap(),
			*GetNameSafe(this));
		return;
	}

	FARMeatState MeatState = GameState->GetMeat();
	const int32 OldTotalMeat = MeatState.GetTotalAmount();
	switch (DropColor)
	{
	case EARAffinityColor::Red:
		MeatState.RedAmount += DropAmount;
		break;
	case EARAffinityColor::Blue:
		MeatState.BlueAmount += DropAmount;
		break;
	case EARAffinityColor::White:
		MeatState.WhiteAmount += DropAmount;
		break;
	default:
		MeatState.UnspecifiedAmount += DropAmount;
		break;
	}

	GameState->SetMeatFromSave(MeatState);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[InvaderDrop|Reward] Meat +%d color=%d oldTotal=%d newTotal=%d drop='%s'"),
		DropAmount,
		static_cast<int32>(DropColor),
		OldTotalMeat,
		GameState->GetMeat().GetTotalAmount(),
		*GetNameSafe(this));
}

void AARInvaderDropBase::DisablePhysicsAndCollisionForCollection()
{
	if (PhysicsSphere)
	{
		PhysicsSphere->SetSimulatePhysics(false);
		PhysicsSphere->SetEnableGravity(false);
		PhysicsSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetActorEnableCollision(false);
}

void AARInvaderDropBase::RestorePostPredictionVisualState()
{
	if (HasAuthority() || !bPredictedLocalCollectionVisual || bIsCollecting || bRewardApplied)
	{
		return;
	}

	bPredictedLocalCollectionVisual = false;
	PredictedLocalCollectionElapsed = 0.0f;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (PhysicsSphere)
	{
		PhysicsSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void AARInvaderDropBase::ResolveInvaderGravityFrameFromSettings()
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const FVector DesiredUp = Settings ? Settings->InvaderDesiredUpDirection : FVector(1.0f, 0.0f, 0.0f);
	const FVector Up = DesiredUp.GetSafeNormal();
	const FVector SafeUp = Up.IsNearlyZero() ? FVector(1.0f, 0.0f, 0.0f) : Up;
	EarthGravityDirection = -SafeUp;
	EarthGravityAcceleration = Settings ? FMath::Max(0.0f, Settings->DropEarthGravityAcceleration) : 980.0f;
}

void AARInvaderDropBase::ApplyEarthGravityForce(const float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!PhysicsSphere || !PhysicsSphere->IsSimulatingPhysics())
	{
		return;
	}

	const float Mass = FMath::Max(1.0f, PhysicsSphere->GetMass());
	const FVector Force = EarthGravityDirection * (EarthGravityAcceleration * Mass);
	PhysicsSphere->AddForce(Force, NAME_None, true);
}

void AARInvaderDropBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARInvaderDropBase, DropType);
	DOREPLIFETIME(AARInvaderDropBase, DropAmount);
	DOREPLIFETIME(AARInvaderDropBase, DropColor);
	DOREPLIFETIME(AARInvaderDropBase, bIsCollecting);
}
