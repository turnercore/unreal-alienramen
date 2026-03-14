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

	/** When true, transition map will auto-trigger travel once every connected player is ready. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	bool bAutoAdvanceWhenAllPlayersReady = true;

	/** When true, resets player ready flags on enter so players must actively continue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	bool bResetPlayerReadyOnBeginPlay = true;

	/** Destination to fall back to when transition context does not provide a URL. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Transition")
	FString FallbackDestinationURL = TEXT("/Game/Maps/Lvl_RamenShop");

	/** When true, finalizes faction election once on transition init for Shop->Invader flows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "When enabled, transition mode finalizes faction election once for Shop->Invader transitions."))
	bool bFinalizeFactionElectionOnShopToInvaderTransition = true;

private:
	void InitializeTransitionContext();
	void ResetPlayersReadyState() const;
	bool TryAdvanceToDestination();
	void TryFinalizeFactionElectionFromTransitionContext(const FARTransitionContext& TransitionContext);

	bool bTransitionTravelStarted = false;
	bool bFactionElectionFinalizedForThisTransition = false;
};
