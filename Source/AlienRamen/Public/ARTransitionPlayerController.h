/**
 * @file ARTransitionPlayerController.h
 * @brief Transition-map controller entrypoint for continue-ready voting.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARTransitionPlayerController.generated.h"

UCLASS()
class ALIENRAMEN_API AARTransitionPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARTransitionPlayerController();

	/** Toggle this player's ready state in the transition map. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Transition")
	void RequestTransitionContinue(bool bReady = true);

	UFUNCTION(Server, Reliable)
	void ServerRequestTransitionContinue(bool bReady = true);
};

