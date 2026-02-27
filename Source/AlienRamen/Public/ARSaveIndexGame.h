#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARSaveTypes.h"
#include "ARSaveIndexGame.generated.h"

UCLASS(BlueprintType)
class ALIENRAMEN_API UARSaveIndexGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Keep name aligned with previous BP save-index contract to simplify transition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	TArray<FARSaveSlotDescriptor> SlotNames;
};

