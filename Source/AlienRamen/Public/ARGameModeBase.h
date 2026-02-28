#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ARPlayerStateBase.h"
#include "ARGameModeBase.generated.h"

class AARGameStateBase;
class UARSaveSubsystem;

UCLASS()
class ALIENRAMEN_API AARGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AARGameModeBase();
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Game Mode")
	FGameplayTag GetModeTag() const { return ModeTag; }

	// Authority helper: readiness + save + travel in one call.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Travel", meta = (BlueprintAuthorityOnly))
	bool TryStartTravel(const FString& URL, const FString& Options = FString(), bool bSkipReadyChecks = false, bool bAbsolute = false, bool bSkipGameNotify = false);

protected:
	// Authoritative mode identity tag for this GameMode class/instance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Game Mode")
	FGameplayTag ModeTag;

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerJoined(AARPlayerStateBase* JoinedPlayerState);
	virtual void BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Alien Ramen|Players")
	void BP_OnPlayerLeft(AARPlayerStateBase* LeftPlayerState);
	virtual void BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState);

private:
	static EARPlayerSlot DetermineNextPlayerSlot(const AARGameStateBase* GameState);
	static EARCharacterChoice GetAlternateCharacterChoice(EARCharacterChoice CurrentChoice);
	static bool IsCharacterChoiceTakenByOther(const AARGameStateBase* InGameState, const AARPlayerStateBase* CurrentPlayerState, EARCharacterChoice CharacterChoice);
	void ResolveCharacterChoiceConflict(const AARGameStateBase* InGameState, AARPlayerStateBase* CurrentPlayerState) const;
	void HandleFirstSessionJoinSetup(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState, UARSaveSubsystem* SaveSubsystem) const;
};
