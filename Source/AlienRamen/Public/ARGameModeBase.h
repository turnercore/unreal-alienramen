/**
 * @file ARGameModeBase.h
 * @brief ARGameModeBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "ARPlayerTypes.h"
#include "ARTransitionTypes.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ARGameModeBase.generated.h"

class AARGameStateBase;
class AARPlayerStateBase;
class AGameSession;
class UARSaveSubsystem;

/** Shared authoritative GameMode: join/setup flow, travel gating, and mode identity tag. */
UCLASS()
class ALIENRAMEN_API AARGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AARGameModeBase();
	virtual TSubclassOf<AGameSession> GetGameSessionClass() const override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual TSubclassOf<APlayerController> GetPlayerControllerClassToSpawnForSeamlessTravel(APlayerController* PreviousPlayerController) override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void Logout(AController* Exiting) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Mode identity gameplay tag (used by transition context and runtime checks). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Mode")
	FGameplayTag GetModeTag() const { return ModeTag; }

	// Returns the final travel URL for mode-driven travel, optionally routing through transition map context.
	FString BuildModeTravelURL(const FString& DestinationURL, EARTravelRoutePolicy RoutePolicy = EARTravelRoutePolicy::ModeDefault) const;

	// Authority helper: readiness + optional save + travel in one call (C++ entrypoint; Blueprint should use AARPlayerController::TryStartTravel).
	bool TryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false, EARTravelRoutePolicy RoutePolicy = EARTravelRoutePolicy::ModeDefault);

	// Convenience helper for ending the current mode and routing through transition map regardless of mode default.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel")
	bool EndModeAndTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

	// Convenience helper for map-to-map travel while staying in the same mode (bypasses transition map regardless of mode default).
	// Designer note: set URL to destination map; Options should include ?listen in listen-host cases.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel")
	bool TravelDirectInMode(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

	/**
	 * Starts a specific Parley conversation by tag for explicit requester/owner character tags.
	 * Intended for scripted mode flows that need deterministic conversation selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue", meta = (BlueprintAuthorityOnly, ToolTip = "Starts a specific Parley conversation by tag for explicit requester and owner character tags on authoritative runtime state."))
	bool StartParleyConversationByTagForCharacters(FGameplayTag RequesterCharacterTag, FGameplayTag OwnerCharacterTag, FGameplayTag ConversationTag);

	/**
	 * Authoritative hold-style character-switch request endpoint.
	 * bIsRequesting=true is "holding switch"; false releases and re-arms the request latch.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Players|Character Switch", meta = (BlueprintAuthorityOnly))
	bool SubmitCharacterSwitchHoldRequest(APlayerController* RequestingController, bool bIsRequesting);

protected:
	// Authoritative mode identity tag for this GameMode class/instance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Game Mode")
	FGameplayTag ModeTag;

	// When true, mode exits via TryStartTravel persist a disk save before travel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Save")
	bool bSaveOnModeExit = true;

	// When true, authority performs an autosave-if-dirty on quit (EndPlay reason = Quit).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Save")
	bool bAutosaveOnQuit = true;

	// When true, mode allows manual save actions (for example pause-menu save).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Save")
	bool bAllowManualSaveInMode = true;

	// When true, local pause open/close requests fan out across all local controllers on the same machine.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Pause")
	bool bShareLocalPauseAcrossControllersInMode = false;

	// When true, mode travel to destination URLs is routed through TransitionTravelMapURL with FARTransitionContext payload.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	bool bRouteModeTravelThroughTransitionMap = false;

	// Transition map URL used when bRouteModeTravelThroughTransitionMap is enabled.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition", meta = (EditCondition = "bRouteModeTravelThroughTransitionMap"))
	FString TransitionTravelMapURL = TEXT("/Game/Maps/Lvl_Loading");

	// Transition context source mode emitted when routing through transition map.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition", meta = (EditCondition = "bRouteModeTravelThroughTransitionMap"))
	EARTransitionSourceMode TransitionSourceMode = EARTransitionSourceMode::Unknown;

	// Transition context reason emitted when routing through transition map.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition", meta = (EditCondition = "bRouteModeTravelThroughTransitionMap"))
	EARTransitionReason TransitionReason = EARTransitionReason::GenericContinue;

	/** Ordered canonical character tags used by switch-character request cycling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Players|Character Switch", meta = (ToolTip = "Ordered canonical character tags used when cycling switch-character requests."))
	TArray<FGameplayTag> PlayableCharacterSwitchOrder;

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerJoined(AARPlayerStateBase* JoinedPlayerState);
	virtual void BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerLeft(AARPlayerStateBase* LeftPlayerState);
	virtual void BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState);

	/** Returns the character tag cached by ChoosePlayerStart for this controller's current spawn attempt. */
	FGameplayTag GetPendingSpawnCharacterTagForController(const AController* Controller) const;

	// Authority pre-travel hook for mode-specific transition logic.
	virtual bool PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks);

private:
	void CachePendingSpawnCharacterTagForController(const AController* Controller, const FGameplayTag& CharacterTag);
	static int32 FindFirstFreePlayerSlotId(const AARGameStateBase* GameState, const AARPlayerStateBase* IgnorePlayerState = nullptr);
	static EARAffinityColor ResolveExpectedInvaderPlayerColor(EARCharacterChoice CharacterChoice);
	static EARCharacterChoice GetAlternateCharacterChoice(EARCharacterChoice CurrentChoice);
	static bool IsCharacterChoiceTakenByOther(const AARGameStateBase* InGameState, const AARPlayerStateBase* CurrentPlayerState, EARCharacterChoice CharacterChoice);
	void ResolveCharacterChoiceConflict(const AARGameStateBase* InGameState, AARPlayerStateBase* CurrentPlayerState) const;
	void HandleFirstSessionJoinSetup(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState, UARSaveSubsystem* SaveSubsystem) const;
	void EnsureJoinedPlayerHasUniqueIdentity(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState) const;
	void NormalizeConnectedPlayersIdentity(AARGameStateBase* InGameState) const;
	void PreparePlayerSpawnIdentity(AController* Player, AARPlayerStateBase* PlayerState) const;
	void CleanupCharacterSwitchRequests();
	bool CollectSwitchEligibleControllers(TArray<APlayerController*>& OutEligibleControllers) const;
	void BuildPlayableCharacterSwitchList(const TArray<APlayerController*>& EligibleControllers, TArray<FGameplayTag>& OutPlayableCharacterTags) const;
	bool TryFindNextFreeSwitchTargetTag(
		const FGameplayTag& CurrentCharacterTag,
		const TArray<FGameplayTag>& OrderedCharacterTags,
		const TMap<FGameplayTag, TWeakObjectPtr<APlayerController>>& OccupancyByCharacterTag,
		const APlayerController* RequestingController,
		FGameplayTag& OutTargetCharacterTag) const;
	bool TryResolveQueuedCharacterSwitches();
	bool ApplyCharacterSwitchAssignments(const TMap<TWeakObjectPtr<APlayerController>, FGameplayTag>& AssignmentByController);

	/** Per-controller character-tag cache captured in ChoosePlayerStart to keep pawn class/start identity aligned. */
	TMap<TWeakObjectPtr<const AController>, FGameplayTag> PendingSpawnCharacterTagsByController;

	/** Controllers currently holding switch-character input. */
	TSet<TWeakObjectPtr<APlayerController>> ActiveCharacterSwitchRequests;

	/** Post-switch latch; controller must release before another switch request is accepted. */
	TSet<TWeakObjectPtr<APlayerController>> CharacterSwitchRequestLatchUntilRelease;
};

