/**
 * @file ARSaveTypesLibrary.h
 * @brief Blueprint helpers for save-related shared structs.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSaveTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARSaveTypesLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API UARSaveTypesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the total meat amount across all FARMeatState buckets. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meat")
	static int32 GetTotalMeatAmount(const FARMeatState& MeatState);
};
