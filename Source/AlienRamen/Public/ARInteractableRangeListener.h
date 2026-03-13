/**
 * @file ARInteractableRangeListener.h
 * @brief Optional interaction lifecycle callback for out-of-range interruption.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ARInteractableRangeListener.generated.h"

class AARPlayerController;

UINTERFACE(BlueprintType)
class ALIENRAMEN_API UARInteractableRangeListener : public UInterface
{
	GENERATED_BODY()
};

class ALIENRAMEN_API IARInteractableRangeListener
{
	GENERATED_BODY()

public:
	/**
	 * Called by controller-side range validation when an active interaction target goes out of range.
	 * Implement in BP/C++ interactables to stop hold/looped interaction safely.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Alien Ramen|Interaction")
	void OnPlayerOutOfRange(AARPlayerController* PlayerInteracting, bool bWasSecondaryInteraction);
};

