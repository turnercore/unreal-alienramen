/**
 * @file ARTransitionGameMode.h
 * @brief Transition map mode that gates destination travel on player continue readiness.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARTransitionGameMode.generated.h"

class AARTransitionGameState;
class AController;
class APlayerController;
class APawn;
class AActor;

UCLASS()
class ALIENRAMEN_API AARTransitionGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARTransitionGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	UFUNCTION()
	void HandleAllPlayersTravelReadyChanged(bool bNewAllPlayersTravelReady, bool bOldAllPlayersTravelReady);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	bool bAutoAdvanceWhenAllPlayersReady = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	bool bResetPlayerReadyOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	FString FallbackDestinationURL = TEXT("/Game/Maps/Lvl_RamenShop");

private:
	void InitializeTransitionContext();
	void ResetPlayersReadyState() const;
	bool TryAdvanceToDestination();

	bool bTransitionTravelStarted = false;
};

