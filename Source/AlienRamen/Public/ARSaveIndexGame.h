#pragma once
/**
 * @file ARSaveIndexGame.h
 * @brief ARSaveIndexGame header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARSaveTypes.h"
#include "ARSaveIndexGame.generated.h"

/** SaveIndex record (canonical + debug namespaces) that lists known slots. */
UCLASS(BlueprintType)
class ALIENRAMEN_API UARSaveIndexGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Keep name aligned with previous BP save-index contract to simplify transition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	TArray<FARSaveSlotDescriptor> SlotNames;
};
