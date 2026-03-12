/**
 * @file ARInvaderGameMode.h
 * @brief ARInvaderGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARInvaderGameMode.generated.h"

class AController;
class APlayerController;

UCLASS()
class ALIENRAMEN_API AARInvaderGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARInvaderGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
};
