/**
 * @file ARPlayerController.h
 * @brief ARPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "ARDialogueTypes.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "ARPlayerController.generated.h"

class UARAbilitySet;
class UInputMappingContext;
class UUserWidget;

USTRUCT(BlueprintType)
struct FARControllerInputMapping
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Input")
	int32 Priority = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAROnPauseMenuStateChangedSignature,
	bool,
	bIsOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAROnPauseMenuOverlayVisibilityChangedSignature,
	bool,
	bShouldDisplayOverlay);

/** Base player controller: owns save sync RPCs, travel requests, and common ability set handoff. */
UCLASS()
class ALIENRAMEN_API AARPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AARPlayerController();

	// Common abilities/effects every pawn gets when possessed (server grants via pawn).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Abilities")
	TObjectPtr<UARAbilitySet> CommonAbilitySet;

	// Convenience accessor
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Abilities")
	const UARAbilitySet* GetCommonAbilitySet() const { return CommonAbilitySet; }

	// Client endpoint for persisting server-canonical save snapshots locally.
	UFUNCTION(Client, Reliable)
	void ClientPersistCanonicalSave(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber);

	// Client requests current server-canonical save snapshot on join/connect.
	UFUNCTION(Server, Reliable)
	void ServerRequestCanonicalSaveSync();

	// Session leave entrypoint for UI/BP. Routes to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	void LeaveSession();

	// Server-side leave request handler.
	UFUNCTION(Server, Reliable)
	void ServerLeaveSession();

	// Controller travel entrypoint for UI/BP. Routes to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel")
	void TryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

	// Server-side travel request handler.
	UFUNCTION(Server, Reliable)
	void ServerTryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

	// Unlock mutation entrypoints for UI/BP. Route to server when called by clients.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestAddUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestRemoveUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestAddUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestRemoveUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestStartDialogue(FGameplayTag NpcTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartDialogue(FGameplayTag NpcTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestAdvanceDialogue();

	UFUNCTION(Server, Reliable)
	void ServerRequestAdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestSubmitDialogueChoice(FGameplayTag ChoiceTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestSubmitDialogueChoice(FGameplayTag ChoiceTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot);

	UFUNCTION(Client, Reliable)
	void ClientDialogueSessionUpdated(const FARDialogueClientView& View);

	UFUNCTION(Client, Reliable)
	void ClientDialogueSessionEnded(const FString& SessionId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue")
	void BP_OnDialogueSessionUpdated(const FARDialogueClientView& View);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue")
	void BP_OnDialogueSessionEnded(const FString& SessionId);

	// Initializes a custom default cursor widget on local controllers only.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Cursor")
	void InitializeCustomCursor();

	// Requests HUD initialization/rebind for the local controller context.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD")
	void RequestHUDInitialization();

	// BP hook to create/rebind HUD widgets when local controller context is ready or refreshed.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|HUD")
	void BP_OnHUDInitializationRequested(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	/** Opens pause menu for all local AR controllers on this machine (if not blocked). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Pause")
	void RequestOpenPauseMenu();

	/** Closes pause menu for all local AR controllers on this machine (C++ path). */
	void RequestClosePauseMenu();

	/** Alias for RequestClosePauseMenu for UI/input bindings. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Pause")
	void ClosePause();

	/** Toggles pause menu for all local AR controllers on this machine. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Pause")
	void RequestTogglePauseMenu();

	/** Returns true if this local controller currently has pause-menu state active. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Pause")
	bool IsPauseMenuOpenLocal() const { return bPauseMenuOpenLocal; }

	/** Returns true if this local controller should currently display pause overlay content. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Pause")
	bool IsPauseMenuOverlayVisibleLocal() const { return bPauseMenuOverlayVisibleLocal; }

	/** Returns true when this local controller cannot open pause menu due to policy/system blockers. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|Pause")
	bool IsPauseMenuBlockedLocal() const;

	/** Adds/removes local pause blocker reason (terminal/menu/etc). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|Pause")
	void SetPauseMenuBlocked(bool bBlocked, FName Reason = NAME_None);

	/** Server mutation endpoint for this controller's pause-menu vote. */
	UFUNCTION(Server, Reliable)
	void ServerSetPauseMenuVote(bool bPaused);

	/** Broadcast when this controller's local pause-menu state flips. */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|Pause")
	FAROnPauseMenuStateChangedSignature OnPauseMenuStateChanged;

	/** Broadcast when this controller should show/hide pause overlay content. */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|Pause")
	FAROnPauseMenuOverlayVisibilityChangedSignature OnPauseMenuOverlayVisibilityChanged;

	/** BP hook fired when this controller enters local pause-menu state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|Pause")
	void BP_OnPauseMenuOpened(bool bShouldDisplayOverlay);

	/** BP hook fired when this controller exits local pause-menu state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|Pause")
	void BP_OnPauseMenuClosed();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Automatically applies/removes DefaultInputMappings for local controllers at lifecycle boundaries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Input")
	bool bAutoApplyDefaultInputMappings = true;

	/** Base gameplay mappings for this controller; pause flow removes/restores these automatically. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Input")
	TArray<FARControllerInputMapping> DefaultInputMappings;

	// Enables local-only custom cursor initialization from BeginPlay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Cursor")
	bool bEnableCustomCursorInit = false;

	// Widget class to create and assign as the default mouse cursor when enabled.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Cursor", meta = (EditCondition = "bEnableCustomCursorInit"))
	TSubclassOf<UUserWidget> CursorDefaultWidgetClass;

	// Runtime cursor widget instance used for EMouseCursor::Default.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Cursor")
	TObjectPtr<UUserWidget> Cursor = nullptr;

	/** Automatically swaps Enhanced Input mapping contexts when pause menu opens/closes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause|Input")
	bool bAutoManagePauseInputContexts = true;

	/** Pause menu mapping context applied while pause menu is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause|Input")
	TObjectPtr<UInputMappingContext> PauseMenuInputMappingContext = nullptr;

	/** Priority used when adding PauseMenuInputMappingContext. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause|Input")
	int32 PauseMenuInputPriority = 1000;

	/** Overlay widget class shown while pause menu is open on overlay-owning local controllers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause")
	TSubclassOf<UUserWidget> PauseOverlayWidgetClass;

	/** Viewport z-order for PauseOverlayWidgetClass instances. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause")
	int32 PauseOverlayWidgetZOrder = 2000;

	/** Automatically switches controller input mode and cursor visibility when paused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause|Input")
	bool bAutoManagePauseInputMode = true;

private:
	void LeaveSessionInternal();
	void TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE);
	void RequestAddUnlockInternal(const FGameplayTag& UnlockTag);
	void RequestRemoveUnlockInternal(const FGameplayTag& UnlockTag);
	void RequestHUDInitializationInternal(bool bForceBroadcast);
	void StartHUDInitializationRetry();
	void StopHUDInitializationRetry();
	void HandleHUDInitializationRetry();
	void SetPauseMenuOpenLocal(bool bOpen);
	void ApplyDefaultInputMappings(bool bEnable);
	void ApplyPauseInputContexts(bool bEnable);
	void ApplyPauseInputMode(bool bEnable);
	bool ShowPauseOverlayWidget();
	void HidePauseOverlayWidget();
	void SubmitPauseMenuVote(bool bPaused);
	bool ShouldDisplayPauseOverlayForLocalController() const;
	bool ShouldShareLocalPauseAcrossControllers() const;
	bool IsLobbyControllerMode() const;
	bool IsDialogueSessionActiveLocal() const;
	bool IsInvaderFullBlastSessionActiveLocal() const;
	static void GatherLocalARPlayerControllers(UWorld* World, TArray<AARPlayerController*>& OutControllers);
	static AARPlayerController* ResolveSharedPauseOverlayOwner(UWorld* World);

	UPROPERTY(Transient)
	bool bRequestedInitialCanonicalSaveSync = false;

	UPROPERTY(Transient)
	bool bHasBroadcastHUDInitialization = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerState> LastHUDInitPlayerState;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameStateBase> LastHUDInitGameState;

	FTimerHandle HUDInitializationRetryTimer;

	UPROPERTY(Transient)
	bool bPauseMenuOpenLocal = false;

	UPROPERTY(Transient)
	bool bPauseMenuOverlayVisibleLocal = false;

	UPROPERTY(Transient)
	bool bPauseInputContextsApplied = false;

	UPROPERTY(Transient)
	bool bDefaultInputMappingsApplied = false;

	UPROPERTY(Transient)
	bool bPauseInputModeApplied = false;

	UPROPERTY(Transient)
	bool bCachedShowMouseCursorForPause = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> PauseOverlayWidget = nullptr;

	TSet<FName> PauseMenuBlockerReasons;
};
