/**
 * @file ARPlayerController.h
 * @brief ARPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "ARInteractionTypes.h"
#include "ParleyDialogueTypes.h"
#include "ParleyPlayerControllerInterface.h"
#include "ARTransitionTypes.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "ARPlayerController.generated.h"

class UARAbilitySet;
class UInputMappingContext;
class UParleyDialogueWidgetBase;
class UUserWidget;
class AARNPCCharacterBase;
class AARMeatStorageBoxActor;
class AActor;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAROnDialogueViewUpdatedSignature,
	const FDialogueClientView&,
	View);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAROnDialogueSessionEndedSignature,
	const FString&,
	SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAROnDialogueAudioRequestedSignature,
	const FDialogueAudioRequest&,
	Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnDialogueChoiceSelectionChangedSignature,
	int32,
	NewChoiceIndex,
	int32,
	OldChoiceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnInteractionActionCueSignature,
	EARInteractionActionCue,
	ActionCue,
	AActor*,
	ActionTarget);

/** Base player controller: owns save sync RPCs, travel requests, and common ability set handoff. */
UCLASS()
class ALIENRAMEN_API AARPlayerController : public APlayerController, public IParleyPlayerControllerInterface
{
	GENERATED_BODY()

public:
	AARPlayerController();

	virtual FGameplayTag GetPlayerSlotTag() const override;
	virtual bool IsDialogueAutoAdvanceEnabled() const override;
	virtual FGameplayTag GetCharacterTag() const override;
	virtual void NotifyDialogueViewUpdated(const FDialogueClientView& View) override;
	virtual void NotifyDialogueSessionEnded(const FString& SessionId) override;
	virtual void NotifyDialogueAudioRequested(const FDialogueAudioRequest& Request) override;
	virtual void RequestInteractWithActor(AActor* Actor) override;
	virtual void RequestStartDialogueBySpeakerTag(const FGameplayTag& SpeakerTag) override;
	virtual void RequestAdvanceDialogueInput() override;
	virtual void RequestSubmitDialogueChoiceInput(FGuid ChoiceBranchId) override;
	virtual void RequestSetDialogueEavesdropInput(bool bEnable, FGameplayTag TargetSlotTag) override;
	virtual void RequestSetDialogueEavesdropOtherPlayerInput(bool bEnable) override;
	virtual void RequestToggleDialogueAutoAdvanceInput() override;
	virtual void RequestAdvanceOrSubmitDialogueInput() override;
	virtual void RequestDialogueChoiceDeltaInput(int32 Delta) override;

	// Common abilities/effects every pawn gets when possessed (server grants via pawn).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Abilities")
	TObjectPtr<UARAbilitySet> CommonAbilitySet;

	// Convenience accessor
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Abilities")
	const UARAbilitySet* GetCommonAbilitySet() const { return CommonAbilitySet; }

	// Client endpoint for persisting server-canonical save snapshots locally.
	// Call on client after server sends canonical save bytes; writes to local slot.
	UFUNCTION(Client, Reliable)
	void ClientPersistCanonicalSave(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber);

	// Client requests current server-canonical save snapshot on join/connect.
	UFUNCTION(Server, Reliable)
	void ServerRequestCanonicalSaveSync();

	// Session leave entrypoint for UI/BP. Routes to server when called by clients.
	// Safe to call from pause/menus; controller will clean up and return to frontend map.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Session")
	void LeaveSession();

	// Server-side leave request handler.
	UFUNCTION(Server, Reliable)
	void ServerLeaveSession();

	// Controller travel entrypoint for UI/BP. Routes to server when called by clients.
	// Use for menu-driven travel; respects travel readiness unless bSkipReadyChecks is true.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel")
	void TryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false, EARTravelRoutePolicy RoutePolicy = EARTravelRoutePolicy::ModeDefault);

	// Server-side travel request handler.
	UFUNCTION(Server, Reliable)
	void ServerTryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false, EARTravelRoutePolicy RoutePolicy = EARTravelRoutePolicy::ModeDefault);

	// Unlock mutation entrypoints for UI/BP. Route to server when called by clients.
	// Adds a progression tag to shared unlocks (authority validated).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestAddUnlock(const FGameplayTag& UnlockTag);

	// Removes a progression tag from shared unlocks (authority validated).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void RequestRemoveUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestAddUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestRemoveUnlock(const FGameplayTag& UnlockTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestStartDialogue(FGameplayTag SpeakerTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartDialogue(FGameplayTag SpeakerTag);

	// Convenience interaction path for world NPC characters. Safe to call from client/UI/BP.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void RequestInteractWithCharacter(AARNPCCharacterBase* CharacterActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestInteractWithCharacter(AARNPCCharacterBase* CharacterActor);

	// Generic dialogue interaction path for any actor that owns a UParleySpeakerComponent.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void RequestInteractWithParleySpeaker(AActor* SpeakerActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestInteractWithParleySpeaker(AActor* SpeakerActor);

	/** Requests a strength-scaled kick impulse on a target actor in interaction range. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void RequestKickActor(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestKickActor(AActor* TargetActor);

	/** Sets the current primary interactable being actively interacted with (hold/ongoing flows). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void SetActiveInteractable(AActor* InteractableActor);

	/** Sets the current secondary interactable being actively interacted with (hold/ongoing flows). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void SetActiveSecondaryInteractable(AActor* InteractableActor);

	/** Clears active primary interactable and optionally notifies target as out-of-range interrupted. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void ClearActiveInteractable(bool bNotifyOutOfRange = false);

	/** Clears active secondary interactable and optionally notifies target as out-of-range interrupted. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void ClearActiveSecondaryInteractable(bool bNotifyOutOfRange = false);

	/** Shared interaction latch for input handlers to bail when already in an interaction flow. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction")
	void SetIsInteracting(bool bInIsInteracting);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Interaction")
	bool GetIsInteracting() const { return bIsInteracting; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Interaction")
	AActor* GetActiveInteractable() const { return ActiveInteractable; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Interaction")
	AActor* GetActiveSecondaryInteractable() const { return ActiveSecondaryInteractable; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopDispenseMeat(AARMeatStorageBoxActor* StorageActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopDispenseMeat(AARMeatStorageBoxActor* StorageActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestAdvanceDialogue();

	UFUNCTION(Server, Reliable)
	void ServerRequestAdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestSubmitDialogueChoice(FGuid ChoiceBranchId);

	UFUNCTION(Server, Reliable)
	void ServerRequestSubmitDialogueChoice(FGuid ChoiceBranchId);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot);

	// Convenience wrapper that targets the opposite slotted player when possible.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void RequestSetDialogueEavesdropOtherPlayer(bool bEnable);

	UFUNCTION(Client, Reliable)
	void ClientDialogueSessionUpdated(const FDialogueClientView& View);

	UFUNCTION(Client, Reliable)
	void ClientDialogueSessionEnded(const FString& SessionId);

	UFUNCTION(Client, Reliable)
	void ClientDialogueAudioRequested(const FDialogueAudioRequest& Request);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue")
	void BP_OnDialogueSessionUpdated(const FDialogueClientView& View);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Dialogue")
	void BP_OnDialogueSessionEnded(const FString& SessionId);

	// Multicast mirrors for dialogue session updates/end (used by reusable UI widget bases).
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueViewUpdatedSignature OnDialogueViewUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionEndedSignature OnDialogueSessionEndedSignal;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Audio")
	FAROnDialogueAudioRequestedSignature OnDialogueAudioRequested;

	// Runtime dialogue-view cache for late-bound widgets/UI.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool GetCachedDialogueView(FDialogueClientView& OutView) const override;

	// Queries current local dialogue view directly from subsystem.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool QueryLocalDialogueView(FDialogueClientView& OutView) const override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasCachedDialogueView() const { return bHasCachedDialogueView; }

	// Toggles this player's dialogue auto-advance preference.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Input")
	void RequestToggleDialogueAutoAdvance();

	// If waiting for a choice, submits the currently selected choice; otherwise advances dialogue.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Input")
	void RequestAdvanceOrSubmitDialogue();

	// Moves local selected choice index by Delta (wrap-around). No-op when not waiting for choices.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Input")
	void RequestDialogueChoiceDelta(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Input")
	int32 GetSelectedDialogueChoiceIndex() const { return SelectedDialogueChoiceIndex; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Input")
	bool GetSelectedDialogueChoiceBranchId(FGuid& OutChoiceBranchId) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Input")
	FAROnDialogueChoiceSelectionChangedSignature OnDialogueChoiceSelectionChanged;

	// Animation/UI cue stream for performed interaction actions (throw/consume/kick/slap/etc).
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Interaction|Animation")
	FAROnInteractionActionCueSignature OnInteractionActionCue;

	// Emits an interaction action cue for local animation/UI listeners.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction|Animation")
	void NotifyInteractionActionCue(EARInteractionActionCue ActionCue, AActor* ActionTarget = nullptr);

	// Optional auto-created dialogue widget for local controllers.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void EnsureDialogueWidget();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|UI")
	void RemoveDialogueWidget();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|UI")
	UParleyDialogueWidgetBase* GetDialogueWidget() const { return DialogueWidget; }

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

	// Automatically creates a dialogue widget on local controller begin play.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI")
	bool bAutoCreateDialogueWidget = false;

	// Widget class for dialogue presentation/input (typically deriving from UParleyDialogueWidgetBase).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (EditCondition = "bAutoCreateDialogueWidget"))
	TSubclassOf<UParleyDialogueWidgetBase> DialogueWidgetClass;

	// Viewport z-order for auto-created dialogue widget.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (EditCondition = "bAutoCreateDialogueWidget"))
	int32 DialogueWidgetZOrder = 1800;

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

	/** Automatically swaps Enhanced Input mapping contexts when a local dialogue session starts/ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Input")
	bool bAutoManageDialogueInputContexts = true;

	/** Dialogue mapping context applied while a local dialogue session is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Input")
	TObjectPtr<UInputMappingContext> DialogueInputMappingContext = nullptr;

	/** Priority used when adding DialogueInputMappingContext. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Input")
	int32 DialogueInputPriority = 1100;

	/** Automatically switches input mode and cursor visibility while local dialogue is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Input")
	bool bAutoManageDialogueInputMode = true;

	/** Max server-side distance allowed for direct interaction requests (NPC/storage/etc). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Interaction")
	float ServerInteractionMaxDistance = 300.0f;

	/** Tick rate used to validate active hold interactions and interrupt out-of-range interactables. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Interaction", meta = (ClampMin = "0.02", UIMin = "0.02"))
	float ActiveInteractionRangeCheckInterval = 0.10f;

	/** Target height delta above pawn origin (cm) at/above which kick-style actions emit Slap cue instead of Kick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Interaction|Animation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SlapCueMinHeightDeltaCm = 80.0f;

	// Shared server-side validation helper for controller interaction RPCs.
	bool IsServerInteractionTargetReachable(const AActor* TargetActor, const TCHAR* ContextLabel) const;

private:
	bool IsServerInteractionTargetReachableInternal(const AActor* TargetActor, const TCHAR* ContextLabel, bool bLogFailures) const;
	void TickActiveInteractionRangeValidation(float DeltaTime);
	void NotifyInteractableOutOfRange(AActor* InteractableActor, bool bWasSecondaryInteraction);
	void RefreshInteractionGateFromActiveTargets();
	void LeaveSessionInternal();
	void TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE, EARTravelRoutePolicy RoutePolicy);
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
	void ApplyDialogueInputContexts(bool bEnable);
	void ApplyDialogueInputMode(bool bEnable);
	void RefreshDialogueInputStateFromSession();
	void SetSelectedDialogueChoiceIndex(int32 NewIndex);
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

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ActiveInteractable = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ActiveSecondaryInteractable = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Interaction", meta = (AllowPrivateAccess = "true"))
	bool bIsInteracting = false;

	UPROPERTY(Transient)
	float ActiveInteractionRangeCheckAccumulator = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue", meta = (AllowPrivateAccess = "true"))
	FDialogueClientView CachedDialogueView;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue", meta = (AllowPrivateAccess = "true"))
	bool bHasCachedDialogueView = false;

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

	UPROPERTY(Transient)
	bool bDialogueInputContextsApplied = false;

	UPROPERTY(Transient)
	bool bDialogueInputModeApplied = false;

	UPROPERTY(Transient)
	bool bCachedShowMouseCursorForDialogue = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|UI|Pause", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> PauseOverlayWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UParleyDialogueWidgetBase> DialogueWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Input", meta = (AllowPrivateAccess = "true"))
	int32 SelectedDialogueChoiceIndex = INDEX_NONE;

	TSet<FName> PauseMenuBlockerReasons;
};
