#include "ARShopStationActor.h"

#include "ARCustomerSettings.h"
#include "ARGameStateBase.h"
#include "ARPlayerController.h"
#include "ARRamenBowlActor.h"
#include "ARRamenMeatActor.h"
#include "ARShopCarryComponent.h"
#include "TagContentResolverSubsystem.h"
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

	if (RuntimeState != EARRamenStationRuntimeState::Processing || !bProcessingActive)
	{
		if (RuntimeState == EARRamenStationRuntimeState::Processing)
		{
			const bool bWasProcessingActive = bProcessingActive;
			RefreshProcessingActiveFlag();
			if (bWasProcessingActive != bProcessingActive)
			{
				ForceNetUpdate();
				BroadcastRuntimeChanged();
			}
		}

		return;
	}

	const bool bWasProcessingActive = bProcessingActive;
	RefreshProcessingActiveFlag();
	if (!bProcessingActive)
	{
		if (bWasProcessingActive != bProcessingActive)
		{
			ForceNetUpdate();
			BroadcastRuntimeChanged();
		}
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
		return false;
	}

	if (SlottedMeatActor != nullptr || RuntimeState == EARRamenStationRuntimeState::Processing)
	{
		// Explicitly block slot replacement while a meat object is currently in station slot.
		return false;
	}

	SlottedMeatActor = MeatActor;
	PendingProcessColor = SanitizeColor(MeatActor->GetMeatColor());
	PendingProcessAmount = FMath::Max(1, MeatActor->GetMeatAmount());
	ProcessingProgress01 = 0.0f;
	SetRuntimeState(EARRamenStationRuntimeState::MeatReady);
	AttachSlottedMeatToSlot();
	ForceNetUpdate();
	BroadcastRuntimeChanged();
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
		return false;
	}

	if (!TryPlaceMeatActor(HeldMeat))
	{
		return false;
	}

	CarryComponent->ReleaseHeldActorForTransfer();
	return true;
}

bool AARShopStationActor::TryPickupSlottedMeatToController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller || RuntimeState != EARRamenStationRuntimeState::MeatReady || SlottedMeatActor == nullptr)
	{
		return false;
	}

	UARShopCarryComponent* CarryComponent = ResolveCarryComponentFromController(Controller);
	if (!CarryComponent || CarryComponent->HasHeldActor())
	{
		return false;
	}

	AARRamenMeatActor* MeatToPickup = SlottedMeatActor;
	if (!CarryComponent->TrySetHeldActor(MeatToPickup))
	{
		return false;
	}

	SlottedMeatActor = nullptr;
	PendingProcessColor = EARAffinityColor::None;
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
	return true;
}

bool AARShopStationActor::StartProcessingByController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
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

bool AARShopStationActor::StopProcessingByController(AARPlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
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
		return false;
	}

	if (HeldBowl->GetNextRequiredStationType() != StationType)
	{
		return false;
	}

	EARAffinityColor ColorToApply = EARAffinityColor::None;
	if (!TryConsumeForBowl(StationType, ColorToApply))
	{
		return false;
	}

	if (!HeldBowl->TryApplyFillFromStation(StationType, ColorToApply))
	{
		if (ProcessedStockAmount <= 0)
		{
			ProcessedStockColor = ColorToApply;
		}
		ProcessedStockAmount = FMath::Clamp(ProcessedStockAmount + 1, 0, ResolveEffectiveMaxStock());
		if (RuntimeState == EARRamenStationRuntimeState::Idle)
		{
			SetRuntimeState(EARRamenStationRuntimeState::Processed);
		}
		ForceNetUpdate();
		BroadcastRuntimeChanged();
		return false;
	}

	return true;
}

bool AARShopStationActor::TryConsumeForBowl(const EARRamenStationType RequestedStationType, EARAffinityColor& OutColor)
{
	OutColor = EARAffinityColor::None;
	if (!HasAuthority() || RequestedStationType != StationType)
	{
		return false;
	}

	if (!IsStationUpgraded())
	{
		// Base station behavior: always provide None output (no meat/stock required).
		return true;
	}

	if (ProcessedStockAmount <= 0)
	{
		return false;
	}

	OutColor = SanitizeColor(ProcessedStockColor);
	ProcessedStockAmount = FMath::Max(0, ProcessedStockAmount - 1);
	if (ProcessedStockAmount <= 0)
	{
		ProcessedStockColor = EARAffinityColor::None;
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
	UTagContentResolverSubsystem* Lookup = GI ? GI->GetSubsystem<UTagContentResolverSubsystem>() : nullptr;
	if (!Lookup)
	{
		return;
	}

	FInstancedStruct RowData;
	FString Error;
	if (!Lookup->TryResolveRowForTag(StationConfigTag, RowData, Error))
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
}

bool AARShopStationActor::ConsumeSlottedMeatAndEnterProcessing()
{
	if (!SlottedMeatActor)
	{
		return false;
	}

	PendingProcessColor = SanitizeColor(SlottedMeatActor->GetMeatColor());
	PendingProcessAmount = FMath::Max(1, SlottedMeatActor->GetMeatAmount());

	SlottedMeatActor->ReleasePickup();
	SlottedMeatActor = nullptr;

	ProcessingProgress01 = 0.0f;
	SetRuntimeState(EARRamenStationRuntimeState::Processing);
	return true;
}

bool AARShopStationActor::BeginProcessingNoneIfAllowed()
{
	if (HasColoredProcessedStock())
	{
		return false;
	}

	const int32 EffectiveMaxStock = ResolveEffectiveMaxStock();
	if (ProcessedStockColor == EARAffinityColor::None && ProcessedStockAmount >= EffectiveMaxStock)
	{
		return false;
	}

	PendingProcessColor = EARAffinityColor::None;
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
		ProcessedStockAmount = FMath::Clamp(AddedUnits, 0, EffectiveMaxStock);
	}
	else if (ProcessedStockColor == FinalColor)
	{
		ProcessedStockAmount = FMath::Clamp(ProcessedStockAmount + AddedUnits, 0, EffectiveMaxStock);
	}
	else
	{
		ProcessedStockColor = FinalColor;
		ProcessedStockAmount = FMath::Clamp(AddedUnits, 0, EffectiveMaxStock);
	}

	PendingProcessColor = EARAffinityColor::None;
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

	SlottedMeatActor->GetRootComponent()->AttachToComponent(MeatSlotAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
	DOREPLIFETIME(AARShopStationActor, PendingProcessAmount);
	DOREPLIFETIME(AARShopStationActor, ProcessedStockColor);
	DOREPLIFETIME(AARShopStationActor, ProcessedStockAmount);
	DOREPLIFETIME(AARShopStationActor, ProcessingProgress01);
	DOREPLIFETIME(AARShopStationActor, bProcessingActive);
}
