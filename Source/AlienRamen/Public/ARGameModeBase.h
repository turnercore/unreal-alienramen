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
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
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

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerJoined(AARPlayerStateBase* JoinedPlayerState);
	virtual void BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerLeft(AARPlayerStateBase* LeftPlayerState);
	virtual void BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState);

	// Authority pre-travel hook for mode-specific transition logic.
	virtual bool PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks);

private:
	static EARPlayerSlot DetermineNextPlayerSlot(const AARGameStateBase* GameState);
	static EARPlayerSlot FindFirstFreePlayerSlot(const AARGameStateBase* GameState, const AARPlayerStateBase* IgnorePlayerState = nullptr);
	static EARAffinityColor ResolveExpectedInvaderPlayerColor(EARCharacterChoice CharacterChoice, EARPlayerSlot PlayerSlot);
	static EARCharacterChoice GetAlternateCharacterChoice(EARCharacterChoice CurrentChoice);
	static bool IsCharacterChoiceTakenByOther(const AARGameStateBase* InGameState, const AARPlayerStateBase* CurrentPlayerState, EARCharacterChoice CharacterChoice);
	void ResolveCharacterChoiceConflict(const AARGameStateBase* InGameState, AARPlayerStateBase* CurrentPlayerState) const;
	void HandleFirstSessionJoinSetup(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState, UARSaveSubsystem* SaveSubsystem) const;
	void EnsureJoinedPlayerHasUniqueSlot(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState) const;
	void NormalizeConnectedPlayersIdentity(AARGameStateBase* InGameState) const;
};
