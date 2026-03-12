#include "ARInvaderPlayerController.h"

#include "ARInvaderFullBlastMenuWidget.h"
#include "ARInvaderGameState.h"
#include "ARPlayerCharacterInvader.h"
#include "ARInvaderSpicyTrackSettings.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "TagContentResolverSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

AARInvaderPlayerController::AARInvaderPlayerController()
{
}

void AARInvaderPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SyncLegacyShipReferenceFromPawn(GetPawn());
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

void AARInvaderPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SyncFullBlastMenuFromGameState();
}

void AARInvaderPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	SyncLegacyShipReferenceFromPawn(InPawn);
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
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AARInvaderGameState* CurrentGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr;
	if (!CurrentGameState)
	{
		if (!BindInvaderGameStateRetryTimer.IsValid())
		{
			World->GetTimerManager().SetTimer(
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
	if (!BindInvaderGameStateRetryTimer.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindInvaderGameStateRetryTimer);
	}

	BindInvaderGameStateRetryTimer.Invalidate();
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

	UTagContentResolverSubsystem* TagContentResolver = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTagContentResolverSubsystem>() : nullptr;
	if (!TagContentResolver)
	{
		return;
	}

	UDataTable* UpgradeTable = nullptr;
	FString LookupError;
	if (!TagContentResolver->TryResolveDataTableForRootTag(Settings->UpgradeDefinitionRootTag, UpgradeTable, LookupError) || !UpgradeTable)
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

	return true;
}

bool AARInvaderPlayerController::IsChooserForSession(const FARInvaderFullBlastSessionState& Session) const
{
	if (!IsLocalPlayerController() || !Session.bIsActive)
	{
		return false;
	}

	const AARPlayerStateBase* InvaderPlayerState = GetInvaderPlayerState();
	if (!InvaderPlayerState)
	{
		return false;
	}

	if (Session.RequestingPlayerSlot == EARPlayerSlot::Unknown)
	{
		return true;
	}

	return InvaderPlayerState->GetPlayerSlot() == Session.RequestingPlayerSlot;
}

void AARInvaderPlayerController::SyncLegacyShipReferenceFromPawn(APawn* InPawn)
{
	FProperty* ShipProperty = GetClass()->FindPropertyByName(TEXT("Ship"));
	FObjectProperty* ShipObjectProperty = CastField<FObjectProperty>(ShipProperty);
	if (!ShipObjectProperty)
	{
		return;
	}

	UObject* ShipObject = Cast<AARPlayerCharacterInvader>(InPawn);
	if (ShipObject && !ShipObject->IsA(ShipObjectProperty->PropertyClass))
	{
		ShipObject = nullptr;
	}

	if (InPawn && !ShipObject)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[InvaderController] Could not bind legacy Ship property on '%s' from pawn '%s' (PawnClass=%s ShipPropertyClass=%s)."),
			*GetNameSafe(this),
			*GetNameSafe(InPawn),
			*GetNameSafe(InPawn->GetClass()),
			*GetNameSafe(ShipObjectProperty->PropertyClass));
	}

	if (ShipObjectProperty->GetObjectPropertyValue_InContainer(this) != ShipObject)
	{
		ShipObjectProperty->SetObjectPropertyValue_InContainer(this, ShipObject);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[InvaderController] Legacy Ship binding updated on '%s': %s"),
			*GetNameSafe(this),
			*GetNameSafe(ShipObject));
	}
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
		// In couch co-op, attach per local player first; fallback to global viewport.
		if (!FullBlastMenuWidget->AddToPlayerScreen(100))
		{
			FullBlastMenuWidget->AddToViewport(100);
		}
	}

	const bool bIsChooser = IsChooserForSession(Session);
	FullBlastMenuWidget->InitializeFullBlastMenu(this, Session, OfferDefinitions, bIsChooser);

	// Only chooser needs UI input capture; observers should still see menu without forced input mode changes.
	if (bIsChooser && !bCapturedInputForFullBlast)
	{
		bCachedShowMouseCursorForFullBlast = bShowMouseCursor;
		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		if (FullBlastMenuWidget->IsFocusable())
		{
			InputMode.SetWidgetToFocus(FullBlastMenuWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		bCapturedInputForFullBlast = true;
	}
	else if (!bIsChooser && bCapturedInputForFullBlast)
	{
		// Chooser role can change during session updates; release capture immediately.
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = bCachedShowMouseCursorForFullBlast;
		bCapturedInputForFullBlast = false;
	}
}

void AARInvaderPlayerController::CloseFullBlastMenu()
{
	if (FullBlastMenuWidget && FullBlastMenuWidget->IsInViewport())
	{
		FullBlastMenuWidget->NotifyMenuClosed();
		FullBlastMenuWidget->RemoveFromParent();
	}

	if (bCapturedInputForFullBlast)
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = bCachedShowMouseCursorForFullBlast;
		bCapturedInputForFullBlast = false;
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
	AARPlayerStateBase* RequestingPlayerState = GetInvaderPlayerState();
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] ServerRequestActivateFullBlast controller='%s' playerState='%s'"),
		*GetNameSafe(this), *GetNameSafe(RequestingPlayerState));

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		const bool bActivated = InvaderGameState->RequestActivateFullBlast(RequestingPlayerState);
		if (!bActivated)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[InvaderSpice|Input] Full Blast activation failed for '%s'. Check prior [InvaderSpice] error log for rejection details."),
				*GetNameSafe(RequestingPlayerState));
			UE_LOG(ARLog, Error, TEXT("%s"), *FailureMessage);
			ClientMessage(FailureMessage);
		}
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
	AARPlayerStateBase* RequestingPlayerState = GetInvaderPlayerState();
	const int32 RequestingSlot = RequestingPlayerState ? static_cast<int32>(RequestingPlayerState->GetPlayerSlot()) : static_cast<int32>(EARPlayerSlot::Unknown);
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice|Input] ServerRequestActivateTrackUpgrade controller='%s' playerState='%s' playerSlot=%d slot=%d"),
		*GetNameSafe(this), *GetNameSafe(RequestingPlayerState), RequestingSlot, SlotIndex);

	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		const bool bActivated = InvaderGameState->ActivateTrackUpgrade(RequestingPlayerState, SlotIndex);
		if (!bActivated)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[InvaderSpice|Input] Track upgrade activation failed for '%s' (PlayerSlot=%d) on slot %d. Check prior [InvaderSpice|Action] logs for rejection details."),
				*GetNameSafe(RequestingPlayerState),
				RequestingSlot,
				SlotIndex);
			UE_LOG(ARLog, Error, TEXT("%s"), *FailureMessage);
			ClientMessage(FailureMessage);
		}
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
	const FGameplayTag SelectedUpgradeTag,
	const int32 SelectedDestinationSlot,
	const bool bHasSelection)
{
	if (HasAuthority())
	{
		ServerRequestSetOfferPresence_Implementation(
			HoveredUpgradeTag,
			HoveredDestinationSlot,
			SelectedUpgradeTag,
			SelectedDestinationSlot,
			bHasSelection);
		return;
	}

	ServerRequestSetOfferPresence(
		HoveredUpgradeTag,
		HoveredDestinationSlot,
		SelectedUpgradeTag,
		SelectedDestinationSlot,
		bHasSelection);
}

void AARInvaderPlayerController::ServerRequestSetOfferPresence_Implementation(
	const FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	const FGameplayTag SelectedUpgradeTag,
	const int32 SelectedDestinationSlot,
	const bool bHasSelection)
{
	if (AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		InvaderGameState->SetOfferPresence(
			GetInvaderPlayerState(),
			HoveredUpgradeTag,
			HoveredDestinationSlot,
			SelectedUpgradeTag,
			SelectedDestinationSlot,
			bHasSelection);
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
