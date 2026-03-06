/**
 * @file ARGameModeBase.h
 * @brief ARGameModeBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARPlayerTypes.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ARGameModeBase.generated.h"

class AARGameStateBase;
class AARPlayerStateBase;
class UARSaveSubsystem;

/** Shared authoritative GameMode: join/setup flow, travel gating, and mode identity tag. */
UCLASS()
class ALIENRAMEN_API AARGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AARGameModeBase();
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Mode")
	FGameplayTag GetModeTag() const { return ModeTag; }

	// Authority helper: readiness + optional save + travel in one call (C++ entrypoint; Blueprint should use AARPlayerController::TryStartTravel).
	bool TryStartTravel(const FString& URL, const FString& Options = "", bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false, bool bUseOpenLevelInPIE = false);

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
