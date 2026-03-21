#include "ARShopStationActor.h"

#include "ARCustomerSettings.h"
#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARRamenBowlActor.h"
#include "ARRamenMeatActor.h"
#include "ARShopCarryComponent.h"
#include "TagKeySubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "StructUtils/InstancedStruct.h"

AARShopStationActor::AARShopStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeatSlotAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("MeatSlotAnchor"));
	MeatSlotAnchor->SetupAttachment(SceneRoot);
}

void AARShopStationActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ApplyConfigFromRowIfAvailable();
		BindAutoSlotContactHandlers();
	}

	AttachSlottedMeatToSlot();
}

void AARShopStationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	if (RuntimeState != EARRamenStationRuntimeState::Processing)
	{
		return;
	}

	// Keep activation refresh and progress advancement in one flow so resumed processing
	// can advance in the same frame controllers become active again.
	const bool bWasProcessingActive = bProcessingActive;
	RefreshProcessingActiveFlag();
	if (bWasProcessingActive != bProcessingActive)
	{
		ForceNetUpdate();
		BroadcastRuntimeChanged();
	}

	if (!bProcessingActive)
	{
		return;
	}

	const float Duration = ResolveEffectiveProcessingDuration();
	ProcessingProgress01 = FMath::Clamp(ProcessingProgress01 + (DeltaSeconds / Duration), 0.0f, 1.0f);
	if (ProcessingProgress01 >= 1.0f)
	{
		CompleteProcessingCycle();
	}
}

bool AARShopStationActor::IsStationUpgraded() const
{
	// Manual/debug authoring mode:
	// When config lookup is disabled and no upgrade tags are authored, treat station as upgraded
	// so station behavior can be tested without unlock dependencies.
	if (!bResolveConfigFromData && RequiredUpgradeTags.IsEmpty())
	{
		return true;
	}

	if (RequiredUpgradeTags.IsEmpty())
	{
		return false;
	}

	const AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	return ARGameState && ARGameState->GetUnlocks().HasAll(RequiredUpgradeTags);
}

bool AARShopStationActor::TryPlaceMeatActor(AARRamenMeatActor* MeatActor)
{
	if (!HasAuthority() || !IsValid(MeatActor) || !IsStationUpgraded())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPlaceMeatActor rejected on '%s': authority=%d validMeat=%d upgraded=%d meat='%s'."),
			*GetNameSafe(this),
			HasAuthority() ? 1 : 0,
			IsValid(MeatActor) ? 1 : 0,
			IsStationUpgraded() ? 1 : 0,
			*GetNameSafe(MeatActor));
		return false;
	}

	if (SlottedMeatActor != nullptr || RuntimeState == EARRamenStationRuntimeState::Processing)
	{
		// Explicitly block slot replacement while a meat object is currently in station slot.
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPlaceMeatActor blocked on '%s': slotted='%s' state='%s'."),
			*GetNameSafe(this),
			*GetNameSafe(SlottedMeatActor),
			*StaticEnum<EARRamenStationRuntimeState>()->GetValueAsString(RuntimeState));
		return false;
	}

	SlottedMeatActor = MeatActor;
	PendingProcessColor = SanitizeColor(MeatActor->GetMeatColor());
	PendingProcessMeatTag = MeatActor->GetMeatTag();
	PendingProcessAmount = FMath::Max(1, MeatActor->GetMeatAmount());
	ProcessingProgress01 = 0.0f;
	SetRuntimeState(EARRamenStationRuntimeState::MeatReady);
	AttachSlottedMeatToSlot();
	ForceNetUpdate();
	BroadcastRuntimeChanged();
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] Meat '%s' slotted into station '%s' (state='%s')."),
		*GetNameSafe(MeatActor),
		*GetNameSafe(this),
		*StaticEnum<EARRamenStationRuntimeState>()->GetValueAsString(RuntimeState));
	return true;
}

bool AARShopStationActor::TryPlaceHeldMeatFromController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
	}

	UARShopCarryComponent* CarryComponent = ResolveCarryComponentFromController(Controller);
	AARRamenMeatActor* HeldMeat = CarryComponent ? CarryComponent->GetHeldMeatActor() : nullptr;
	if (!CarryComponent || !HeldMeat)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPlaceHeldMeatFromController rejected on '%s': controller='%s' carry='%s' heldMeat='%s'."),
			*GetNameSafe(this),
			*GetNameSafe(Controller),
			*GetNameSafe(CarryComponent),
			*GetNameSafe(HeldMeat));
		return false;
	}

	if (!TryPlaceMeatActor(HeldMeat))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPlaceHeldMeatFromController failed placement on '%s' for controller '%s' meat '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(Controller),
			*GetNameSafe(HeldMeat));
		return false;
	}

	CarryComponent->ReleaseHeldActorForTransfer();
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] TryPlaceHeldMeatFromController success on '%s' for controller '%s'."),
		*GetNameSafe(this),
		*GetNameSafe(Controller));
	return true;
}

bool AARShopStationActor::TryPickupSlottedMeatToController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller || RuntimeState != EARRamenStationRuntimeState::MeatReady || SlottedMeatActor == nullptr)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPickupSlottedMeatToController rejected on '%s': authority=%d controller='%s' state='%s' slotted='%s'."),
			*GetNameSafe(this),
			HasAuthority() ? 1 : 0,
			*GetNameSafe(Controller),
			*StaticEnum<EARRamenStationRuntimeState>()->GetValueAsString(RuntimeState),
			*GetNameSafe(SlottedMeatActor));
		return false;
	}

	UARShopCarryComponent* CarryComponent = ResolveCarryComponentFromController(Controller);
	if (!CarryComponent || CarryComponent->HasHeldActor())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPickupSlottedMeatToController rejected on '%s': carry='%s' held='%s'."),
			*GetNameSafe(this),
			*GetNameSafe(CarryComponent),
			*GetNameSafe(CarryComponent ? CarryComponent->GetHeldActor() : nullptr));
		return false;
	}

	AARRamenMeatActor* MeatToPickup = SlottedMeatActor;
	if (!CarryComponent->TrySetHeldActor(MeatToPickup))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryPickupSlottedMeatToController failed hold-transfer on '%s' for controller '%s' meat '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(Controller),
			*GetNameSafe(MeatToPickup));
		return false;
	}

	SlottedMeatActor = nullptr;
	PendingProcessColor = EARAffinityColor::None;
	PendingProcessMeatTag = FGameplayTag();
	PendingProcessAmount = 0;

	if (ProcessedStockAmount > 0)
	{
		SetRuntimeState(EARRamenStationRuntimeState::Processed);
	}
	else
	{
		SetRuntimeState(EARRamenStationRuntimeState::Idle);
	}

	ForceNetUpdate();
	BroadcastRuntimeChanged();
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] Controller '%s' picked up slotted meat from station '%s'."),
		*GetNameSafe(Controller),
		*GetNameSafe(this));
	return true;
}

bool AARShopStationActor::StartProcessingByController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
	}

	if (ProcessingInputMode == EARRamenStationProcessingInputMode::Tap)
	{
		for (auto It = ActiveTapPressControllers.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}

		if (ActiveTapPressControllers.Contains(Controller))
		{
			UE_LOG(
				ARLog,
				VeryVerbose,
				TEXT("[Shop|Station] StartProcessing tap ignored on '%s': controller '%s' is still held; release required before next pulse."),
				*GetNameSafe(this),
				*GetNameSafe(Controller));
			return false;
		}

		ActiveTapPressControllers.Add(Controller);
		const bool bTapped = TapProcessByController(Controller);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] StartProcessing tap pulse on '%s': controller '%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(Controller),
			bTapped ? 1 : 0);
		return bTapped;
	}

	if (!IsStationUpgraded())
	{
		// Base station behavior serves None directly and does not use meat/processing.
		return false;
	}

	if (RuntimeState == EARRamenStationRuntimeState::MeatReady)
	{
		if (!ConsumeSlottedMeatAndEnterProcessing())
		{
			return false;
		}
	}
	else if (RuntimeState == EARRamenStationRuntimeState::Idle || RuntimeState == EARRamenStationRuntimeState::Processed)
	{
		if (!BeginProcessingNoneIfAllowed())
		{
			return false;
		}
	}

	if (RuntimeState != EARRamenStationRuntimeState::Processing)
	{
		return false;
	}

	ActiveProcessingControllers.Add(Controller);
	RefreshProcessingActiveFlag();
	ForceNetUpdate();
	BroadcastRuntimeChanged();
	return true;
}

bool AARShopStationActor::TapProcessByController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
	}

	if (!IsStationUpgraded())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TapProcess rejected on '%s': station not upgraded."),
			*GetNameSafe(this));
		return false;
	}

	if (RuntimeState == EARRamenStationRuntimeState::MeatReady)
	{
		if (!ConsumeSlottedMeatAndEnterProcessing())
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Station] TapProcess failed on '%s': could not consume slotted meat."),
				*GetNameSafe(this));
			return false;
		}
	}
	else if (RuntimeState == EARRamenStationRuntimeState::Idle || RuntimeState == EARRamenStationRuntimeState::Processed)
	{
		if (!BeginProcessingNoneIfAllowed())
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Station] TapProcess rejected on '%s': could not begin processing from state '%s'."),
				*GetNameSafe(this),
				*StaticEnum<EARRamenStationRuntimeState>()->GetValueAsString(RuntimeState));
			return false;
		}
	}

	if (RuntimeState != EARRamenStationRuntimeState::Processing)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TapProcess rejected on '%s': station state is '%s', not Processing."),
			*GetNameSafe(this),
			*StaticEnum<EARRamenStationRuntimeState>()->GetValueAsString(RuntimeState));
		return false;
	}

	const float Duration = ResolveEffectiveProcessingDuration();
	const float TapSeconds = FMath::Max(0.0f, TapProcessingSecondsPerPress);
	const float AddedProgress = Duration > KINDA_SMALL_NUMBER ? (TapSeconds / Duration) : 0.0f;
	ProcessingProgress01 = FMath::Clamp(ProcessingProgress01 + AddedProgress, 0.0f, 1.0f);

	if (ProcessingProgress01 >= 1.0f)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TapProcess complete on '%s': controller '%s' advanced to completion."),
			*GetNameSafe(this),
			*GetNameSafe(Controller));
		CompleteProcessingCycle();
		return true;
	}

	ForceNetUpdate();
	BroadcastRuntimeChanged();
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] TapProcess progress on '%s': controller '%s' +%.3f -> %.3f."),
		*GetNameSafe(this),
		*GetNameSafe(Controller),
		AddedProgress,
		ProcessingProgress01);
	return true;
}

bool AARShopStationActor::StopProcessingByController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
	}

	if (ProcessingInputMode == EARRamenStationProcessingInputMode::Tap)
	{
		const bool bReleased = ActiveTapPressControllers.Remove(Controller) > 0;
		UE_LOG(
			ARLog,
			VeryVerbose,
			TEXT("[Shop|Station] StopProcessing tap release on '%s': controller '%s' released=%d."),
			*GetNameSafe(this),
			*GetNameSafe(Controller),
			bReleased ? 1 : 0);
		return bReleased;
	}

	if (ActiveProcessingControllers.Remove(Controller) <= 0)
	{
		return false;
	}

	RefreshProcessingActiveFlag();
	ForceNetUpdate();
	BroadcastRuntimeChanged();
	return true;
}

void AARShopStationActor::StopAllProcessingControllers()
{
	if (!HasAuthority())
	{
		return;
	}

	ActiveProcessingControllers.Reset();
	ActiveTapPressControllers.Reset();
	RefreshProcessingActiveFlag();
	ForceNetUpdate();
	BroadcastRuntimeChanged();
}

bool AARShopStationActor::TryFillHeldBowlFromController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
	}

	UARShopCarryComponent* CarryComponent = ResolveCarryComponentFromController(Controller);
	AARRamenBowlActor* HeldBowl = CarryComponent ? CarryComponent->GetHeldBowlActor() : nullptr;
	if (!HeldBowl)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryFillHeldBowlFromController rejected on '%s': controller '%s' has no held bowl."),
			*GetNameSafe(this),
			*GetNameSafe(Controller));
		return false;
	}

	if (HeldBowl->GetNextRequiredStationType() != StationType)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryFillHeldBowlFromController rejected on '%s': bowl next='%s' station='%s'."),
			*GetNameSafe(this),
			*StaticEnum<EARRamenStationType>()->GetValueAsString(HeldBowl->GetNextRequiredStationType()),
			*StaticEnum<EARRamenStationType>()->GetValueAsString(StationType));
		return false;
	}

	EARAffinityColor ColorToApply = EARAffinityColor::None;
	FGameplayTag MeatTagToApply;
	EARVendingQualityTier QualityToApply = EARVendingQualityTier::Standard;
	if (!TryConsumeForBowl(StationType, ColorToApply, MeatTagToApply, QualityToApply))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryFillHeldBowlFromController consume failed on '%s': state='%s' stock=%d upgraded=%d."),
			*GetNameSafe(this),
			*StaticEnum<EARRamenStationRuntimeState>()->GetValueAsString(RuntimeState),
			ProcessedStockAmount,
			IsStationUpgraded() ? 1 : 0);
		return false;
	}

	if (!HeldBowl->TryApplyFillFromStation(StationType, ColorToApply, MeatTagToApply, QualityToApply))
	{
		if (ProcessedStockAmount <= 0)
		{
			ProcessedStockColor = ColorToApply;
			ProcessedStockMeatTag = MeatTagToApply;
			ProcessedStockQualityTier = QualityToApply;
		}
		ProcessedStockAmount = FMath::Clamp(ProcessedStockAmount + 1, 0, ResolveEffectiveMaxStock());
		if (RuntimeState == EARRamenStationRuntimeState::Idle)
		{
			SetRuntimeState(EARRamenStationRuntimeState::Processed);
		}
		ForceNetUpdate();
		BroadcastRuntimeChanged();
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TryFillHeldBowlFromController apply failed on '%s': returned stock to station."),
			*GetNameSafe(this));
		return false;
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] TryFillHeldBowlFromController success on '%s' for controller '%s'."),
		*GetNameSafe(this),
		*GetNameSafe(Controller));
	return true;
}

bool AARShopStationActor::TryConsumeForBowl(
	const EARRamenStationType RequestedStationType,
	EARAffinityColor& OutColor,
	FGameplayTag& OutMeatTag,
	EARVendingQualityTier& OutQualityTier)
{
	OutColor = EARAffinityColor::None;
	OutMeatTag = FGameplayTag();
	OutQualityTier = EARVendingQualityTier::Standard;
	if (!HasAuthority() || RequestedStationType != StationType)
	{
		return false;
	}

	if (!IsStationUpgraded())
	{
		// Base station behavior can bypass stock, but still respects empty-processing policy.
		return bAllowProcessingWithoutMeat;
	}

	if (ProcessedStockAmount <= 0)
	{
		return false;
	}

	OutColor = SanitizeColor(ProcessedStockColor);
	OutMeatTag = ProcessedStockMeatTag;
	OutQualityTier = ProcessedStockQualityTier;
	ProcessedStockAmount = FMath::Max(0, ProcessedStockAmount - 1);
	if (ProcessedStockAmount <= 0)
	{
		ProcessedStockColor = EARAffinityColor::None;
		ProcessedStockMeatTag = FGameplayTag();
		ProcessedStockQualityTier = EARVendingQualityTier::Standard;
		if (RuntimeState == EARRamenStationRuntimeState::Processed)
		{
			SetRuntimeState(SlottedMeatActor ? EARRamenStationRuntimeState::MeatReady : EARRamenStationRuntimeState::Idle);
		}
	}

	ForceNetUpdate();
	BroadcastRuntimeChanged();
	return true;
}

void AARShopStationActor::OnRep_RuntimeState(const EARRamenStationRuntimeState OldState)
{
	if (RuntimeState != OldState)
	{
		BroadcastRuntimeChanged();
	}
}

void AARShopStationActor::OnRep_SlottedMeatActor(AARRamenMeatActor* OldSlottedMeatActor)
{
	(void)OldSlottedMeatActor;
	AttachSlottedMeatToSlot();
	BroadcastRuntimeChanged();
}

void AARShopStationActor::OnRep_ProcessedStockAmount(int32 OldProcessedStockAmount)
{
	if (ProcessedStockAmount != OldProcessedStockAmount)
	{
		BroadcastRuntimeChanged();
	}
}

void AARShopStationActor::OnRep_ProcessingState()
{
	BroadcastRuntimeChanged();
}

EARAffinityColor AARShopStationActor::SanitizeColor(const EARAffinityColor InColor)
{
	return InColor == EARAffinityColor::Unknown ? EARAffinityColor::None : InColor;
}

UARShopCarryComponent* AARShopStationActor::ResolveCarryComponentFromController(AARPlayerController* Controller)
{
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
}

void AARShopStationActor::BindAutoSlotContactHandlers()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	int32 BoundCount = 0;
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive || Primitive->GetOwner() != this)
		{
			continue;
		}

		Primitive->SetGenerateOverlapEvents(true);
		Primitive->SetNotifyRigidBodyCollision(true);
		Primitive->OnComponentBeginOverlap.AddDynamic(this, &AARShopStationActor::HandleStationPrimitiveBeginOverlap);
		Primitive->OnComponentHit.AddDynamic(this, &AARShopStationActor::HandleStationPrimitiveHit);
		++BoundCount;
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] Auto-slot contact handlers bound on '%s': primitives=%d."),
		*GetNameSafe(this),
		BoundCount);
}

bool AARShopStationActor::TryAutoSlotLooseMeatActor(AActor* CandidateActor)
{
	AARRamenMeatActor* LooseMeat = Cast<AARRamenMeatActor>(CandidateActor);
	if (!HasAuthority() || !LooseMeat)
	{
		return false;
	}

	const USceneComponent* MeatRoot = LooseMeat->GetRootComponent();
	const USceneComponent* AttachParent = MeatRoot ? MeatRoot->GetAttachParent() : nullptr;
	if (AttachParent && AttachParent->GetOwner() != this)
	{
		// Ignore held/attached meat from non-station owners.
		UE_LOG(
			ARLog,
			VeryVerbose,
			TEXT("[Shop|Station] Auto-slot ignored on '%s': meat '%s' attached to '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(LooseMeat),
			*GetNameSafe(AttachParent->GetOwner()));
		return false;
	}

	const bool bPlaced = TryPlaceMeatActor(LooseMeat);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Station] Auto-slot contact on '%s' with meat '%s': success=%d."),
		*GetNameSafe(this),
		*GetNameSafe(LooseMeat),
		bPlaced ? 1 : 0);
	return bPlaced;
}

void AARShopStationActor::HandleStationPrimitiveBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	UE_LOG(
		ARLog,
		VeryVerbose,
		TEXT("[Shop|Station] BeginOverlap station='%s' overlappedComp='%s' otherActor='%s' otherComp='%s'."),
		*GetNameSafe(this),
		*GetNameSafe(OverlappedComponent),
		*GetNameSafe(OtherActor),
		*GetNameSafe(OtherComp));
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;
	TryAutoSlotLooseMeatActor(OtherActor);
}

void AARShopStationActor::HandleStationPrimitiveHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	UE_LOG(
		ARLog,
		VeryVerbose,
		TEXT("[Shop|Station] Hit station='%s' hitComp='%s' otherActor='%s' otherComp='%s'."),
		*GetNameSafe(this),
		*GetNameSafe(HitComponent),
		*GetNameSafe(OtherActor),
		*GetNameSafe(OtherComp));
	(void)NormalImpulse;
	(void)Hit;
	TryAutoSlotLooseMeatActor(OtherActor);
}

void AARShopStationActor::BroadcastRuntimeChanged()
{
	OnRuntimeStateChanged.Broadcast();
}

void AARShopStationActor::ApplyConfigFromRowIfAvailable()
{
	if (!bResolveConfigFromData || !StationConfigTag.IsValid())
	{
		return;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UTagKeySubsystem* Lookup = GI ? GI->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Lookup)
	{
		return;
	}

	FInstancedStruct RowData;
	FString Error;
	if (!Lookup->TryResolveRowStructForTag(StationConfigTag, RowData, Error))
	{
		return;
	}

	const FARShopStationConfigRow* Row = RowData.GetPtr<FARShopStationConfigRow>();
	if (!Row)
	{
		return;
	}

	StationType = Row->StationType;
	RequiredUpgradeTags = Row->RequiredUpgradeTags;
	MaxStock = FMath::Max(1, Row->MaxStock);
	ProcessingDurationSeconds = FMath::Max(0.05f, Row->ProcessingDurationSeconds);
	ProcessingInputMode = Row->ProcessingInputMode;
	TapProcessingSecondsPerPress = FMath::Max(0.0f, Row->TapProcessingSecondsPerPress);
	bAllowProcessingWithoutMeat = Row->bAllowProcessingWithoutMeat;
}

bool AARShopStationActor::ConsumeSlottedMeatAndEnterProcessing()
{
	if (!SlottedMeatActor)
	{
		return false;
	}

	EARAffinityColor NextColor = SanitizeColor(SlottedMeatActor->GetMeatColor());
	FGameplayTag NextMeatTag = SlottedMeatActor->GetMeatTag();
	EARVendingQualityTier NextQualityTier = SlottedMeatActor->GetMeatQualityTier();
	int32 NextProcessAmount = FMath::Max(1, SlottedMeatActor->GetMeatAmount());
	if (NextMeatTag.IsValid())
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
		FARMeatDefinitionRow MeatDef;
		if (ItemDefinitions && ItemDefinitions->ResolveMeatDefinition(NextMeatTag, MeatDef))
		{
			NextProcessAmount = FMath::Max(1, MeatDef.StationFillAmount);
			if (MeatDef.MeatTag.IsValid())
			{
				NextMeatTag = MeatDef.MeatTag;
			}
		}
	}

	const EARAffinityColor ExistingColor = SanitizeColor(ProcessedStockColor);
	const bool bSameMeatType =
		(ProcessedStockMeatTag.IsValid() && NextMeatTag.IsValid() && ProcessedStockMeatTag.MatchesTagExact(NextMeatTag))
		|| (!ProcessedStockMeatTag.IsValid() && !NextMeatTag.IsValid());
	const bool bSameQualityTier = ProcessedStockQualityTier == NextQualityTier;

	// If stock is already buffered, only allow processing when explicitly swapping
	// to a materially different output type (color/meat tag pair).
	if (ProcessedStockAmount > 0)
	{
		if (NextColor == ExistingColor && bSameMeatType && bSameQualityTier)
		{
			return false;
		}
	}

	PendingProcessColor = NextColor;
	PendingProcessMeatTag = NextMeatTag;
	PendingProcessQualityTier = NextQualityTier;
	PendingProcessAmount = NextProcessAmount;

	SlottedMeatActor->ReleaseCarryItem();
	SlottedMeatActor = nullptr;

	ProcessingProgress01 = 0.0f;
	SetRuntimeState(EARRamenStationRuntimeState::Processing);
	return true;
}

bool AARShopStationActor::BeginProcessingNoneIfAllowed()
{
	if (!bAllowProcessingWithoutMeat)
	{
		return false;
	}

	// None-processing is only valid from an empty stock state.
	// If any stock is already buffered (colored or None), require bowl consumption first.
	if (ProcessedStockAmount > 0 || HasColoredProcessedStock())
	{
		return false;
	}

	PendingProcessColor = EARAffinityColor::None;
	PendingProcessMeatTag = FGameplayTag();
	PendingProcessQualityTier = EARVendingQualityTier::Standard;
	PendingProcessAmount = 1;
	if (RuntimeState != EARRamenStationRuntimeState::Processing)
	{
		ProcessingProgress01 = 0.0f;
	}
	SetRuntimeState(EARRamenStationRuntimeState::Processing);
	return true;
}

void AARShopStationActor::CompleteProcessingCycle()
{
	if (!HasAuthority() || RuntimeState != EARRamenStationRuntimeState::Processing)
	{
		return;
	}

	const EARAffinityColor FinalColor = SanitizeColor(PendingProcessColor);
	const int32 AddedUnits = FMath::Max(1, PendingProcessAmount);
	const int32 EffectiveMaxStock = ResolveEffectiveMaxStock();

	if (ProcessedStockAmount <= 0)
	{
		ProcessedStockColor = FinalColor;
		ProcessedStockMeatTag = PendingProcessMeatTag;
		ProcessedStockQualityTier = PendingProcessQualityTier;
		ProcessedStockAmount = FMath::Clamp(AddedUnits, 0, EffectiveMaxStock);
	}
	else if (
		ProcessedStockColor == FinalColor
		&& (
			(ProcessedStockMeatTag.IsValid() && PendingProcessMeatTag.IsValid() && ProcessedStockMeatTag.MatchesTagExact(PendingProcessMeatTag))
			|| (!ProcessedStockMeatTag.IsValid() && !PendingProcessMeatTag.IsValid()))
		&& ProcessedStockQualityTier == PendingProcessQualityTier)
	{
		ProcessedStockAmount = FMath::Clamp(ProcessedStockAmount + AddedUnits, 0, EffectiveMaxStock);
	}
	else
	{
		ProcessedStockColor = FinalColor;
		ProcessedStockMeatTag = PendingProcessMeatTag;
		ProcessedStockQualityTier = PendingProcessQualityTier;
		ProcessedStockAmount = FMath::Clamp(AddedUnits, 0, EffectiveMaxStock);
	}

	PendingProcessColor = EARAffinityColor::None;
	PendingProcessMeatTag = FGameplayTag();
	PendingProcessQualityTier = EARVendingQualityTier::Standard;
	PendingProcessAmount = 0;
	ProcessingProgress01 = 0.0f;
	ActiveProcessingControllers.Reset();
	bProcessingActive = false;

	if (SlottedMeatActor)
	{
		SetRuntimeState(EARRamenStationRuntimeState::MeatReady);
	}
	else if (ProcessedStockAmount > 0)
	{
		SetRuntimeState(EARRamenStationRuntimeState::Processed);
	}
	else
	{
		SetRuntimeState(EARRamenStationRuntimeState::Idle);
	}

	ForceNetUpdate();
	BroadcastRuntimeChanged();
}

void AARShopStationActor::RefreshProcessingActiveFlag()
{
	for (auto It = ActiveProcessingControllers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	bProcessingActive = ActiveProcessingControllers.Num() > 0;
}

void AARShopStationActor::AttachSlottedMeatToSlot() const
{
	if (!SlottedMeatActor || !MeatSlotAnchor || !SlottedMeatActor->GetRootComponent())
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	SlottedMeatActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		if (Primitive->IsSimulatingPhysics())
		{
			Primitive->SetSimulatePhysics(false);
		}
		Primitive->SetEnableGravity(false);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USceneComponent* MeatRoot = SlottedMeatActor->GetRootComponent();
	MeatRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MeatRoot->AttachToComponent(MeatSlotAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SlottedMeatActor->SetActorEnableCollision(false);
}

bool AARShopStationActor::HasColoredProcessedStock() const
{
	return ProcessedStockAmount > 0
		&& ProcessedStockColor != EARAffinityColor::None
		&& ProcessedStockColor != EARAffinityColor::Unknown;
}

void AARShopStationActor::SetRuntimeState(const EARRamenStationRuntimeState NewState)
{
	if (RuntimeState == NewState)
	{
		return;
	}

	const EARRamenStationRuntimeState OldState = RuntimeState;
	RuntimeState = NewState;
	OnRep_RuntimeState(OldState);
}

int32 AARShopStationActor::ResolveEffectiveMaxStock() const
{
	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();
	const int32 Fallback = Settings ? Settings->DefaultStationMaxStock : 5;
	return FMath::Max(1, MaxStock > 0 ? MaxStock : Fallback);
}

float AARShopStationActor::ResolveEffectiveProcessingDuration() const
{
	const UARCustomerSettings* Settings = GetDefault<UARCustomerSettings>();
	const float Fallback = Settings ? Settings->DefaultStationProcessingDurationSeconds : 1.5f;
	const float Candidate = ProcessingDurationSeconds > 0.0f ? ProcessingDurationSeconds : Fallback;
	return FMath::Max(0.05f, Candidate);
}

void AARShopStationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARShopStationActor, RuntimeState);
	DOREPLIFETIME(AARShopStationActor, SlottedMeatActor);
	DOREPLIFETIME(AARShopStationActor, PendingProcessColor);
	DOREPLIFETIME(AARShopStationActor, PendingProcessMeatTag);
	DOREPLIFETIME(AARShopStationActor, PendingProcessQualityTier);
	DOREPLIFETIME(AARShopStationActor, PendingProcessAmount);
	DOREPLIFETIME(AARShopStationActor, ProcessedStockColor);
	DOREPLIFETIME(AARShopStationActor, ProcessedStockMeatTag);
	DOREPLIFETIME(AARShopStationActor, ProcessedStockQualityTier);
	DOREPLIFETIME(AARShopStationActor, ProcessedStockAmount);
	DOREPLIFETIME(AARShopStationActor, ProcessingProgress01);
	DOREPLIFETIME(AARShopStationActor, bProcessingActive);
}
