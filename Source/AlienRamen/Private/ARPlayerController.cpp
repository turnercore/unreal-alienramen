#include "ARPlayerController.h"
#include "ARDialogueSubsystem.h"
#include "ARHUDBase.h"
#include "ARGameStateBase.h"
#include "ARGameModeBase.h"
#include "ARInvaderGameState.h"
#include "ARInvaderPlayerController.h"
#include "ARLobbyPlayerController.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

AARPlayerController::AARPlayerController()
{
	// Nothing required here yet.
	// Input bindings can stay in BP; this controller just provides the CommonAbilitySet reference.
}

void AARPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefaultInputMappings(true);
	InitializeCustomCursor();
	RequestHUDInitializationInternal(false);

	if (IsLocalController() && !HasAuthority() && !bRequestedInitialCanonicalSaveSync)
	{
		bRequestedInitialCanonicalSaveSync = true;
		ServerRequestCanonicalSaveSync();
	}
}

void AARPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPauseMenuOpenLocal(false);
	ApplyDefaultInputMappings(false);
	StopHUDInitializationRetry();
	Super::EndPlay(EndPlayReason);
}

void AARPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bPauseMenuOpenLocal && IsPauseMenuBlockedLocal())
	{
		RequestClosePauseMenu();
	}
}

void AARPlayerController::InitializeCustomCursor()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!bEnableCustomCursorInit)
	{
		return;
	}

	if (!CursorDefaultWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[UI] Custom cursor init skipped on '%s': CursorDefaultWidgetClass is not set."), *GetNameSafe(this));
		return;
	}

	if (!Cursor)
	{
		Cursor = CreateWidget<UUserWidget>(this, CursorDefaultWidgetClass);
	}

	if (!Cursor)
	{
		UE_LOG(ARLog, Warning, TEXT("[UI] Custom cursor init failed on '%s': could not create cursor widget."), *GetNameSafe(this));
		return;
	}

	SetMouseCursorWidget(EMouseCursor::Default, Cursor);
}

void AARPlayerController::RequestHUDInitialization()
{
	RequestHUDInitializationInternal(true);
}

void AARPlayerController::ClientPersistCanonicalSave_Implementation(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			FARSaveResult Result;
			if (!SaveSubsystem->PersistCanonicalSaveFromBytes(SaveBytes, SlotBaseName, SlotNumber, Result))
			{
				UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] ClientPersistCanonicalSave failed for '%s' rev %d: %s"), *SlotBaseName.ToString(), SlotNumber, *Result.Error);
			}
		}
	}
}

void AARPlayerController::ServerRequestCanonicalSaveSync_Implementation()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			FARSaveResult Result;
			if (!SaveSubsystem->PushCurrentSaveToPlayer(this, Result))
			{
				UE_LOG(ARLog, Verbose, TEXT("[SaveSubsystem] ServerRequestCanonicalSaveSync: no current save pushed (%s)."), *Result.Error);
			}
		}
	}
}

void AARPlayerController::LeaveSession()
{
	if (HasAuthority())
	{
		LeaveSessionInternal();
		return;
	}

	ServerLeaveSession();
}

void AARPlayerController::ServerLeaveSession_Implementation()
{
	LeaveSessionInternal();
}

void AARPlayerController::LeaveSessionInternal()
{
	UE_LOG(ARLog, Log, TEXT("[Session] LeaveSession requested by controller '%s' (Authority=%d)."), *GetNameSafe(this), HasAuthority() ? 1 : 0);

	// Remote client leave requests are executed on the server controller instance for that client.
	// Only return that specific client to menu; do not collapse the host session.
	if (HasAuthority() && !IsLocalController())
	{
		ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Leaving session")));
		return;
	}

	// Host/standalone leave path: best-effort autosave if dirty, except while actively in invader mode.
	if (HasAuthority())
	{
		if (AARGameModeBase* ARGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AARGameModeBase>() : nullptr)
		{
			const FGameplayTag InvaderModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
			const bool bInInvaderMode = InvaderModeTag.IsValid() && ARGameMode->GetModeTag().MatchesTagExact(InvaderModeTag);
			if (!bInInvaderMode)
			{
				if (UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
				{
					FARSaveResult SaveResult;
					const bool bSaved = SaveSubsystem->RequestAutosaveIfDirty(true, SaveResult);
					if (bSaved)
					{
						UE_LOG(ARLog, Log, TEXT("[Session] LeaveSession autosave succeeded (Slot=%s Rev=%d)."), *SaveResult.SlotName.ToString(), SaveResult.SlotNumber);
					}
					else if (!SaveResult.Error.IsEmpty())
					{
						UE_LOG(ARLog, Verbose, TEXT("[Session] LeaveSession autosave skipped/failed: %s"), *SaveResult.Error);
					}
				}
			}
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		GI->ReturnToMainMenu();
		return;
	}

	ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Leaving session")));
}

void AARPlayerController::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE)
{
	if (HasAuthority())
	{
		TryStartTravelInternal(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE);
		return;
	}

	ServerTryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE);
}

void AARPlayerController::ServerTryStartTravel_Implementation(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE)
{
	TryStartTravelInternal(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE);
}

void AARPlayerController::TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE)
{
	if (AARGameModeBase* ARGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AARGameModeBase>() : nullptr)
	{
		if (!ARGameMode->TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE))
		{
			UE_LOG(ARLog, Warning, TEXT("[Travel] Controller '%s' TryStartTravel failed. URL='%s' Options='%s'"), *GetNameSafe(this), *URL, *Options);
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Travel] Controller '%s' TryStartTravel ignored: no authoritative AARGameModeBase."), *GetNameSafe(this));
}

void AARPlayerController::RequestAddUnlock(const FGameplayTag& UnlockTag)
{
	if (HasAuthority())
	{
		RequestAddUnlockInternal(UnlockTag);
		return;
	}

	ServerRequestAddUnlock(UnlockTag);
}

void AARPlayerController::RequestRemoveUnlock(const FGameplayTag& UnlockTag)
{
	if (HasAuthority())
	{
		RequestRemoveUnlockInternal(UnlockTag);
		return;
	}

	ServerRequestRemoveUnlock(UnlockTag);
}

void AARPlayerController::ServerRequestAddUnlock_Implementation(const FGameplayTag& UnlockTag)
{
	RequestAddUnlockInternal(UnlockTag);
}

void AARPlayerController::ServerRequestRemoveUnlock_Implementation(const FGameplayTag& UnlockTag)
{
	RequestRemoveUnlockInternal(UnlockTag);
}

void AARPlayerController::RequestAddUnlockInternal(const FGameplayTag& UnlockTag)
{
	if (AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr)
	{
		if (!ARGameState->AddUnlockTag(UnlockTag))
		{
			UE_LOG(ARLog, Verbose, TEXT("[Save] RequestAddUnlock ignored for '%s' tag '%s'."), *GetNameSafe(this), *UnlockTag.ToString());
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Save] RequestAddUnlock ignored: no AARGameStateBase for '%s'."), *GetNameSafe(this));
}

void AARPlayerController::RequestRemoveUnlockInternal(const FGameplayTag& UnlockTag)
{
	if (AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr)
	{
		if (!ARGameState->RemoveUnlockTag(UnlockTag))
		{
			UE_LOG(ARLog, Verbose, TEXT("[Save] RequestRemoveUnlock ignored for '%s' tag '%s'."), *GetNameSafe(this), *UnlockTag.ToString());
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Save] RequestRemoveUnlock ignored: no AARGameStateBase for '%s'."), *GetNameSafe(this));
}

void AARPlayerController::RequestStartDialogue(FGameplayTag NpcTag)
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->TryStartDialogueWithNpc(this, NpcTag);
		}
		return;
	}

	ServerRequestStartDialogue(NpcTag);
}

void AARPlayerController::ServerRequestStartDialogue_Implementation(FGameplayTag NpcTag)
{
	RequestStartDialogue(NpcTag);
}

void AARPlayerController::RequestAdvanceDialogue()
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->AdvanceDialogue(this);
		}
		return;
	}

	ServerRequestAdvanceDialogue();
}

void AARPlayerController::ServerRequestAdvanceDialogue_Implementation()
{
	RequestAdvanceDialogue();
}

void AARPlayerController::RequestSubmitDialogueChoice(FGameplayTag ChoiceTag)
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->SubmitDialogueChoice(this, ChoiceTag);
		}
		return;
	}

	ServerRequestSubmitDialogueChoice(ChoiceTag);
}

void AARPlayerController::ServerRequestSubmitDialogueChoice_Implementation(FGameplayTag ChoiceTag)
{
	RequestSubmitDialogueChoice(ChoiceTag);
}

void AARPlayerController::RequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot)
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->SetShopEavesdropTarget(this, TargetSlot, bEnable);
		}
		return;
	}

	ServerRequestSetDialogueEavesdrop(bEnable, TargetSlot);
}

void AARPlayerController::ServerRequestSetDialogueEavesdrop_Implementation(bool bEnable, EARPlayerSlot TargetSlot)
{
	RequestSetDialogueEavesdrop(bEnable, TargetSlot);
}

void AARPlayerController::ClientDialogueSessionUpdated_Implementation(const FARDialogueClientView& View)
{
	BP_OnDialogueSessionUpdated(View);
}

void AARPlayerController::ClientDialogueSessionEnded_Implementation(const FString& SessionId)
{
	BP_OnDialogueSessionEnded(SessionId);
}

void AARPlayerController::RequestHUDInitializationInternal(const bool bForceBroadcast)
{
	if (!IsLocalController())
	{
		StopHUDInitializationRetry();
		return;
	}

	AARHUDBase* ARHUD = Cast<AARHUDBase>(GetHUD());
	if (!ARHUD)
	{
		StartHUDInitializationRetry();
		if (!bForceBroadcast)
		{
			return;
		}
	}

	APlayerState* CurrentPlayerState = PlayerState;
	AGameStateBase* CurrentGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const bool bContextChanged = LastHUDInitPlayerState.Get() != CurrentPlayerState
		|| LastHUDInitGameState.Get() != CurrentGameState;
	if (!bForceBroadcast && bHasBroadcastHUDInitialization && !bContextChanged)
	{
		return;
	}

	bHasBroadcastHUDInitialization = true;
	LastHUDInitPlayerState = CurrentPlayerState;
	LastHUDInitGameState = CurrentGameState;
	StopHUDInitializationRetry();

	if (ARHUD)
	{
		ARHUD->RequestHUDInitialization(this, CurrentPlayerState, CurrentGameState);
	}

	BP_OnHUDInitializationRequested(this, CurrentPlayerState, CurrentGameState);
}

void AARPlayerController::StartHUDInitializationRetry()
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(HUDInitializationRetryTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		HUDInitializationRetryTimer,
		this,
		&AARPlayerController::HandleHUDInitializationRetry,
		0.1f,
		true);
}

void AARPlayerController::StopHUDInitializationRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HUDInitializationRetryTimer);
	}
}

void AARPlayerController::HandleHUDInitializationRetry()
{
	RequestHUDInitializationInternal(false);
}

void AARPlayerController::RequestOpenPauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(GetWorld(), LocalControllers);
	if (LocalControllers.IsEmpty())
	{
		return;
	}

	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (!LocalController || LocalController->IsPauseMenuBlockedLocal())
		{
			return;
		}
	}

	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (LocalController)
		{
			LocalController->SetPauseMenuOpenLocal(true);
		}
	}
}

void AARPlayerController::RequestClosePauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(GetWorld(), LocalControllers);
	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (LocalController)
		{
			LocalController->SetPauseMenuOpenLocal(false);
		}
	}
}

void AARPlayerController::ClosePause()
{
	RequestClosePauseMenu();
}

void AARPlayerController::RequestTogglePauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(GetWorld(), LocalControllers);
	if (LocalControllers.IsEmpty())
	{
		return;
	}

	const bool bAnyOpen = LocalControllers.ContainsByPredicate(
		[](const AARPlayerController* LocalController)
		{
			return LocalController && LocalController->IsPauseMenuOpenLocal();
		});

	if (bAnyOpen)
	{
		RequestClosePauseMenu();
		return;
	}

	RequestOpenPauseMenu();
}

bool AARPlayerController::IsPauseMenuBlockedLocal() const
{
	if (!IsLocalPlayerController())
	{
		return true;
	}

	if (!PauseMenuBlockerReasons.IsEmpty())
	{
		return true;
	}

	return IsLobbyControllerMode() || IsDialogueSessionActiveLocal() || IsInvaderFullBlastSessionActiveLocal();
}

void AARPlayerController::SetPauseMenuBlocked(const bool bBlocked, FName Reason)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (Reason.IsNone())
	{
		Reason = TEXT("Generic");
	}

	if (bBlocked)
	{
		PauseMenuBlockerReasons.Add(Reason);
		if (bPauseMenuOpenLocal)
		{
			RequestClosePauseMenu();
		}
		return;
	}

	PauseMenuBlockerReasons.Remove(Reason);
}

void AARPlayerController::ServerSetPauseMenuVote_Implementation(const bool bPaused)
{
	SubmitPauseMenuVote(bPaused);
}

void AARPlayerController::SetPauseMenuOpenLocal(const bool bOpen)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bOpen && IsPauseMenuBlockedLocal())
	{
		return;
	}

	if (bPauseMenuOpenLocal == bOpen)
	{
		return;
	}

	bPauseMenuOpenLocal = bOpen;

	if (bOpen)
	{
		ApplyPauseInputContexts(true);
		ApplyPauseInputMode(true);
		SubmitPauseMenuVote(true);

		const bool bShouldDisplayOverlay = ShouldDisplayPauseOverlayForLocalController();
		bPauseMenuOverlayVisibleLocal = bShouldDisplayOverlay ? ShowPauseOverlayWidget() : false;
		if (!bShouldDisplayOverlay)
		{
			HidePauseOverlayWidget();
		}
		OnPauseMenuOverlayVisibilityChanged.Broadcast(bPauseMenuOverlayVisibleLocal);
		OnPauseMenuStateChanged.Broadcast(true);
		BP_OnPauseMenuOpened(bPauseMenuOverlayVisibleLocal);
		return;
	}

	SubmitPauseMenuVote(false);
	HidePauseOverlayWidget();

	const bool bWasShowingOverlay = bPauseMenuOverlayVisibleLocal;
	bPauseMenuOverlayVisibleLocal = false;
	if (bWasShowingOverlay)
	{
		OnPauseMenuOverlayVisibilityChanged.Broadcast(false);
	}

	ApplyPauseInputContexts(false);
	ApplyPauseInputMode(false);
	OnPauseMenuStateChanged.Broadcast(false);
	BP_OnPauseMenuClosed();
}

void AARPlayerController::ApplyDefaultInputMappings(const bool bEnable)
{
	if (!bAutoApplyDefaultInputMappings || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	if (bEnable)
	{
		if (bDefaultInputMappingsApplied)
		{
			return;
		}

		for (const FARControllerInputMapping& Mapping : DefaultInputMappings)
		{
			if (Mapping.MappingContext)
			{
				InputSubsystem->AddMappingContext(Mapping.MappingContext, Mapping.Priority);
			}
		}

		bDefaultInputMappingsApplied = true;
		return;
	}

	if (!bDefaultInputMappingsApplied)
	{
		return;
	}

	for (const FARControllerInputMapping& Mapping : DefaultInputMappings)
	{
		if (Mapping.MappingContext)
		{
			InputSubsystem->RemoveMappingContext(Mapping.MappingContext);
		}
	}

	bDefaultInputMappingsApplied = false;
}

void AARPlayerController::ApplyPauseInputContexts(const bool bEnable)
{
	if (!bAutoManagePauseInputContexts || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	if (bEnable)
	{
		if (bPauseInputContextsApplied)
		{
			return;
		}
		ApplyDefaultInputMappings(false);

		if (PauseMenuInputMappingContext)
		{
			InputSubsystem->AddMappingContext(PauseMenuInputMappingContext, PauseMenuInputPriority);
		}

		bPauseInputContextsApplied = true;
		return;
	}

	if (!bPauseInputContextsApplied)
	{
		return;
	}

	if (PauseMenuInputMappingContext)
	{
		InputSubsystem->RemoveMappingContext(PauseMenuInputMappingContext);
	}
	ApplyDefaultInputMappings(true);

	bPauseInputContextsApplied = false;
}

void AARPlayerController::ApplyPauseInputMode(const bool bEnable)
{
	if (!bAutoManagePauseInputMode || !IsLocalPlayerController())
	{
		return;
	}

	if (bEnable)
	{
		bCachedShowMouseCursorForPause = bShowMouseCursor;
		bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;
		if (PauseOverlayWidget)
		{
			InputMode.SetWidgetToFocus(PauseOverlayWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = bCachedShowMouseCursorForPause;
}

bool AARPlayerController::ShowPauseOverlayWidget()
{
	if (!PauseOverlayWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[UI|Pause] PauseOverlayWidgetClass is not configured for '%s'."), *GetNameSafe(this));
		return false;
	}

	if (!PauseOverlayWidget || PauseOverlayWidget->GetClass() != PauseOverlayWidgetClass)
	{
		HidePauseOverlayWidget();
		PauseOverlayWidget = CreateWidget<UUserWidget>(this, PauseOverlayWidgetClass);
	}

	if (!PauseOverlayWidget)
	{
		UE_LOG(ARLog, Warning, TEXT("[UI|Pause] Failed to create pause overlay widget for '%s'."), *GetNameSafe(this));
		return false;
	}

	if (!PauseOverlayWidget->IsInViewport())
	{
		PauseOverlayWidget->AddToViewport(PauseOverlayWidgetZOrder);
	}

	return PauseOverlayWidget->IsInViewport();
}

void AARPlayerController::HidePauseOverlayWidget()
{
	if (PauseOverlayWidget && PauseOverlayWidget->IsInViewport())
	{
		PauseOverlayWidget->RemoveFromParent();
	}
}

void AARPlayerController::SubmitPauseMenuVote(const bool bPaused)
{
	AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	AARPlayerStateBase* ARPlayerState = GetPlayerState<AARPlayerStateBase>();
	if (!ARGameState || !ARPlayerState)
	{
		return;
	}

	const EARPlayerSlot PlayerSlot = ARPlayerState->GetPlayerSlot();
	if (PlayerSlot == EARPlayerSlot::Unknown)
	{
		return;
	}

	if (HasAuthority())
	{
		ARGameState->SetPlayerPauseMenuVote(PlayerSlot, bPaused);
		return;
	}

	ServerSetPauseMenuVote(bPaused);
}

bool AARPlayerController::ShouldDisplayPauseOverlayForLocalController() const
{
	if (!IsLocalPlayerController())
	{
		return false;
	}

	if (!IsA<AARInvaderPlayerController>())
	{
		return true;
	}

	return ResolveInvaderSharedOverlayOwner(GetWorld()) == this;
}

bool AARPlayerController::IsLobbyControllerMode() const
{
	return IsA<AARLobbyPlayerController>();
}

bool AARPlayerController::IsDialogueSessionActiveLocal() const
{
	if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
	{
		FARDialogueClientView View;
		return DialogueSubsystem->GetLocalViewForController(this, View);
	}

	return false;
}

bool AARPlayerController::IsInvaderFullBlastSessionActiveLocal() const
{
	if (const AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		return InvaderGameState->GetFullBlastSession().bIsActive;
	}

	return false;
}

void AARPlayerController::GatherLocalARPlayerControllers(UWorld* World, TArray<AARPlayerController*>& OutControllers)
{
	OutControllers.Reset();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AARPlayerController* ARController = Cast<AARPlayerController>(It->Get()))
		{
			if (ARController->IsLocalPlayerController())
			{
				OutControllers.Add(ARController);
			}
		}
	}
}

AARPlayerController* AARPlayerController::ResolveInvaderSharedOverlayOwner(UWorld* World)
{
	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(World, LocalControllers);

	TArray<AARPlayerController*> LocalInvaderControllers;
	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (LocalController && LocalController->IsA<AARInvaderPlayerController>())
		{
			LocalInvaderControllers.Add(LocalController);
		}
	}

	if (LocalInvaderControllers.IsEmpty())
	{
		return nullptr;
	}

	for (AARPlayerController* LocalInvaderController : LocalInvaderControllers)
	{
		if (const AARPlayerStateBase* ARPlayerState = LocalInvaderController->GetPlayerState<AARPlayerStateBase>())
		{
			if (ARPlayerState->GetPlayerSlot() == EARPlayerSlot::P1)
			{
				return LocalInvaderController;
			}
		}
	}

	return LocalInvaderControllers[0];
}
