#include "ARInvaderPlayerController.h"

#include "ARInvaderFullBlastMenuWidget.h"
#include "ARInvaderGameState.h"
#include "ARInvaderSpicyTrackSettings.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ContentLookupSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

AARInvaderPlayerController::AARInvaderPlayerController()
{
}

void AARInvaderPlayerController::BeginPlay()
{
	Super::BeginPlay();
	TryBindInvaderGameState();
}

void AARInvaderPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBindInvaderGameStateRetry();

	if (AARInvaderGameState* BoundGameState = BoundInvaderGameState.Get())
	{
		BoundGameState->OnInvaderFullBlastSessionChanged.RemoveDynamic(this, &AARInvaderPlayerController::HandleInvaderFullBlastSessionChanged);
	}
	BoundInvaderGameState.Reset();

	CloseFullBlastMenu();
	Super::EndPlay(EndPlayReason);
}

AARPlayerStateBase* AARInvaderPlayerController::GetInvaderPlayerState() const
{
	return GetPlayerState<AARPlayerStateBase>();
}

void AARInvaderPlayerController::HandleInvaderFullBlastSessionChanged(const bool /*bIsActive*/)
{
	SyncFullBlastMenuFromGameState();
}

void AARInvaderPlayerController::TryBindInvaderGameState()
{
	AARInvaderGameState* CurrentGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr;
	if (!CurrentGameState)
	{
		if (!BindInvaderGameStateRetryTimer.IsValid())
		{
			GetWorldTimerManager().SetTimer(
				BindInvaderGameStateRetryTimer,
				this,
				&AARInvaderPlayerController::TryBindInvaderGameState,
				0.25f,
				true);
		}
		return;
	}

	if (BoundInvaderGameState.Get() == CurrentGameState)
	{
		StopBindInvaderGameStateRetry();
		SyncFullBlastMenuFromGameState();
		return;
	}

	if (AARInvaderGameState* PreviousGameState = BoundInvaderGameState.Get())
	{
		PreviousGameState->OnInvaderFullBlastSessionChanged.RemoveDynamic(this, &AARInvaderPlayerController::HandleInvaderFullBlastSessionChanged);
	}

	BoundInvaderGameState = CurrentGameState;
	CurrentGameState->OnInvaderFullBlastSessionChanged.AddDynamic(this, &AARInvaderPlayerController::HandleInvaderFullBlastSessionChanged);
	StopBindInvaderGameStateRetry();
	SyncFullBlastMenuFromGameState();
}

void AARInvaderPlayerController::StopBindInvaderGameStateRetry()
{
	if (BindInvaderGameStateRetryTimer.IsValid())
	{
		GetWorldTimerManager().ClearTimer(BindInvaderGameStateRetryTimer);
	}
}

void AARInvaderPlayerController::SyncFullBlastMenuFromGameState()
{
	AARInvaderGameState* InvaderGameState = BoundInvaderGameState.Get();
	if (!InvaderGameState)
	{
		CloseFullBlastMenu();
		return;
	}

	const FARInvaderFullBlastSessionState& Session = InvaderGameState->GetFullBlastSession();
	TArray<FARInvaderUpgradeDefRow> OfferDefinitions;
	BuildOfferDefinitionsForSession(Session, OfferDefinitions);
	OnInvaderFullBlastMenuSessionUpdated.Broadcast(Session.bIsActive, Session, OfferDefinitions);

	if (!Session.bIsActive || !ShouldDisplayFullBlastMenuForSession(Session))
	{
		CloseFullBlastMenu();
		return;
	}

	ShowOrUpdateFullBlastMenu(Session, OfferDefinitions);
}

void AARInvaderPlayerController::BuildOfferDefinitionsForSession(
	const FARInvaderFullBlastSessionState& Session,
	TArray<FARInvaderUpgradeDefRow>& OutDefinitions) const
{
	OutDefinitions.Reset();
	if (!Session.bIsActive || Session.Offers.IsEmpty())
	{
		return;
	}

	const UARInvaderSpicyTrackSettings* Settings = GetDefault<UARInvaderSpicyTrackSettings>();
	if (!Settings || !Settings->UpgradeDefinitionRootTag.IsValid())
	{
		return;
	}

	UContentLookupSubsystem* ContentLookup = GetGameInstance() ? GetGameInstance()->GetSubsystem<UContentLookupSubsystem>() : nullptr;
	if (!ContentLookup)
	{
		return;
	}

	UDataTable* UpgradeTable = nullptr;
	FString LookupError;
	if (!ContentLookup->GetDataTableForRootTag(Settings->UpgradeDefinitionRootTag, UpgradeTable, LookupError) || !UpgradeTable)
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|UI] Could not resolve upgrade table for full-blast menu: %s"), *LookupError);
		return;
	}

	TMap<FGameplayTag, FARInvaderUpgradeDefRow> DefinitionMap;
	TArray<FARInvaderUpgradeDefRow*> Rows;
	UpgradeTable->GetAllRows(TEXT("AARInvaderPlayerController::BuildOfferDefinitionsForSession"), Rows);
	for (const FARInvaderUpgradeDefRow* Row : Rows)
	{
		if (Row && Row->UpgradeTag.IsValid())
		{
			DefinitionMap.Add(Row->UpgradeTag, *Row);
		}
	}

	OutDefinitions.Reserve(Session.Offers.Num());
	for (const FARInvaderUpgradeOffer& Offer : Session.Offers)
	{
		if (const FARInvaderUpgradeDefRow* Def = DefinitionMap.Find(Offer.UpgradeTag))
		{
			OutDefinitions.Add(*Def);
		}
		else
		{
			FARInvaderUpgradeDefRow FallbackDef;
			FallbackDef.UpgradeTag = Offer.UpgradeTag;
			FallbackDef.DisplayName = FText::FromString(Offer.UpgradeTag.ToString());
			OutDefinitions.Add(MoveTemp(FallbackDef));
		}
	}
}

bool AARInvaderPlayerController::ShouldDisplayFullBlastMenuForSession(const FARInvaderFullBlastSessionState& Session) const
{
	if (!IsLocalPlayerController() || !Session.bIsActive)
	{
		return false;
	}

	AARPlayerStateBase* InvaderPlayerState = GetInvaderPlayerState();
	if (!InvaderPlayerState)
	{
		return false;
	}

	const EARPlayerSlot LocalSlot = InvaderPlayerState->GetPlayerSlot();
	if (Session.RequestingPlayerSlot == EARPlayerSlot::Unknown)
	{
		return true;
	}

	return LocalSlot == Session.RequestingPlayerSlot;
}

void AARInvaderPlayerController::ShowOrUpdateFullBlastMenu(
	const FARInvaderFullBlastSessionState& Session,
	const TArray<FARInvaderUpgradeDefRow>& OfferDefinitions)
{
	const UARInvaderSpicyTrackSettings* Settings = GetDefault<UARInvaderSpicyTrackSettings>();
	if (!Settings || Settings->FullBlastMenuWidgetClass.IsNull())
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|UI] Full blast menu class not configured; using delegate-only flow."));
		return;
	}

	if (!FullBlastMenuWidget)
	{
		TSubclassOf<UARInvaderFullBlastMenuWidget> MenuClass = Settings->FullBlastMenuWidgetClass.LoadSynchronous();
		if (!MenuClass)
		{
			UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|UI] Failed to load FullBlastMenuWidgetClass from settings."));
			return;
		}

		FullBlastMenuWidget = CreateWidget<UARInvaderFullBlastMenuWidget>(this, MenuClass);
		if (!FullBlastMenuWidget)
		{
			UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|UI] Failed to create full blast menu widget."));
			return;
		}
	}

	if (!FullBlastMenuWidget->IsInViewport())
	{
		FullBlastMenuWidget->AddToViewport(100);
		bCachedShowMouseCursorForFullBlast = bShowMouseCursor;
		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(FullBlastMenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}

	FullBlastMenuWidget->InitializeFullBlastMenu(this, Session, OfferDefinitions, ShouldDisplayFullBlastMenuForSession(Session));
}

void AARInvaderPlayerController::CloseFullBlastMenu()
{
	if (FullBlastMenuWidget && FullBlastMenuWidget->IsInViewport())
	{
		FullBlastMenuWidget->NotifyMenuClosed();
		FullBlastMenuWidget->RemoveFromParent();
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = bCachedShowMouseCursorForFullBlast;
	}
}

void AARInvaderPlayerController::RequestActivateFullBlast()
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] RequestActivateFullBlast controller='%s' authority=%d"),
		*GetNameSafe(this), HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		ServerRequestActivateFullBlast_Implementation();
		return;
	}

	ServerRequestActivateFullBlast();
}

void AARInvaderPlayerController::ServerRequestActivateFullBlast_Implementation()
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] ServerRequestActivateFullBlast controller='%s' playerState='%s'"),
		*GetNameSafe(this), *GetNameSafe(GetInvaderPlayerState()));

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->RequestActivateFullBlast(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestResolveFullBlastSelection(const FGameplayTag SelectedUpgradeTag, const int32 DesiredDestinationSlot)
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] RequestResolveFullBlastSelection controller='%s' tag='%s' slot=%d authority=%d"),
		*GetNameSafe(this), *SelectedUpgradeTag.ToString(), DesiredDestinationSlot, HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		ServerRequestResolveFullBlastSelection_Implementation(SelectedUpgradeTag, DesiredDestinationSlot);
		return;
	}

	ServerRequestResolveFullBlastSelection(SelectedUpgradeTag, DesiredDestinationSlot);
}

void AARInvaderPlayerController::ServerRequestResolveFullBlastSelection_Implementation(const FGameplayTag SelectedUpgradeTag, const int32 DesiredDestinationSlot)
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] ServerRequestResolveFullBlastSelection controller='%s' playerState='%s' tag='%s' slot=%d"),
		*GetNameSafe(this), *GetNameSafe(GetInvaderPlayerState()), *SelectedUpgradeTag.ToString(), DesiredDestinationSlot);

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ResolveFullBlastSelection(GetInvaderPlayerState(), SelectedUpgradeTag, DesiredDestinationSlot);
	}
}

void AARInvaderPlayerController::RequestResolveFullBlastSkip()
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] RequestResolveFullBlastSkip controller='%s' authority=%d"),
		*GetNameSafe(this), HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		ServerRequestResolveFullBlastSkip_Implementation();
		return;
	}

	ServerRequestResolveFullBlastSkip();
}

void AARInvaderPlayerController::ServerRequestResolveFullBlastSkip_Implementation()
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] ServerRequestResolveFullBlastSkip controller='%s' playerState='%s'"),
		*GetNameSafe(this), *GetNameSafe(GetInvaderPlayerState()));

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ResolveFullBlastSkip(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestActivateTrackUpgrade(const int32 SlotIndex)
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] RequestActivateTrackUpgrade controller='%s' slot=%d authority=%d"),
		*GetNameSafe(this), SlotIndex, HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		ServerRequestActivateTrackUpgrade_Implementation(SlotIndex);
		return;
	}

	ServerRequestActivateTrackUpgrade(SlotIndex);
}

void AARInvaderPlayerController::ServerRequestActivateTrackUpgrade_Implementation(const int32 SlotIndex)
{
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] ServerRequestActivateTrackUpgrade controller='%s' playerState='%s' slot=%d"),
		*GetNameSafe(this), *GetNameSafe(GetInvaderPlayerState()), SlotIndex);

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ActivateTrackUpgrade(GetInvaderPlayerState(), SlotIndex);
	}
}

void AARInvaderPlayerController::HandleSpiceTrackDeltaInput(const float AxisValue)
{
	if (AARPlayerStateBase* InvaderPlayerState = GetInvaderPlayerState())
	{
		int32 DeltaTier = 0;
		if (AxisValue > 0.5f)
		{
			DeltaTier = 1;
		}
		else if (AxisValue < -0.5f)
		{
			DeltaTier = -1;
		}

		if (DeltaTier != 0)
		{
			InvaderPlayerState->AdjustSpicyTrackCursorTier(DeltaTier);
		}
	}
}

void AARInvaderPlayerController::HandleSpiceTrackActivateFromCursor()
{
	AARPlayerStateBase* InvaderPlayerState = GetInvaderPlayerState();
	if (!InvaderPlayerState)
	{
		return;
	}

	const int32 CursorTier = InvaderPlayerState->GetEffectiveSpicyTrackCursorTier();
	if (CursorTier <= 0)
	{
		// Compatibility fallback: if UI/input hasn't moved cursor onto full-blast tier yet,
		// still allow direct full-blast activation attempt from neutral cursor state.
		RequestActivateFullBlast();
		return;
	}

	if (const AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		const int32 FullBlastTier = FMath::Max(1, InvaderGameState->GetSharedFullBlastTier());
		if (CursorTier >= FullBlastTier)
		{
			RequestActivateFullBlast();
			return;
		}
	}

	RequestActivateTrackUpgrade(CursorTier);
}

void AARInvaderPlayerController::RequestStartSharingSpice()
{
	if (HasAuthority())
	{
		ServerRequestStartSharingSpice_Implementation();
		return;
	}

	ServerRequestStartSharingSpice();
}

void AARInvaderPlayerController::ServerRequestStartSharingSpice_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->StartSharingSpice(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestStopSharingSpice()
{
	if (HasAuthority())
	{
		ServerRequestStopSharingSpice_Implementation();
		return;
	}

	ServerRequestStopSharingSpice();
}

void AARInvaderPlayerController::ServerRequestStopSharingSpice_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->StopSharingSpice(GetInvaderPlayerState());
	}
}

void AARInvaderPlayerController::RequestSetOfferPresence(
	const FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	const FVector2D CursorNormalized,
	const bool bHasCursor)
{
	if (HasAuthority())
	{
		ServerRequestSetOfferPresence_Implementation(HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
		return;
	}

	ServerRequestSetOfferPresence(HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
}

void AARInvaderPlayerController::ServerRequestSetOfferPresence_Implementation(
	const FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	const FVector2D CursorNormalized,
	const bool bHasCursor)
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->SetOfferPresence(GetInvaderPlayerState(), HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
	}
}

void AARInvaderPlayerController::RequestClearOfferPresence()
{
	if (HasAuthority())
	{
		ServerRequestClearOfferPresence_Implementation();
		return;
	}

	ServerRequestClearOfferPresence();
}

void AARInvaderPlayerController::ServerRequestClearOfferPresence_Implementation()
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->ClearOfferPresence(GetInvaderPlayerState());
	}
}
