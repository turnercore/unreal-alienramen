/**
 * @file GameplayTagUtilities.h
 * @brief GameplayTagUtilities header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "GameplayTagUtilities.generated.h"

UCLASS()
class ALIENRAMEN_API UGameplayTagUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Get tag at depth: Depth=2 => Unlocks.Ships.Ship1 -> Unlocks.Ships (Depth is 1-based).
	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static bool TryGetTagAtDepth(FGameplayTag Tag, int32 Depth, FGameplayTag& OutTagAtDepth);

	// Replaces everything under "slot" (slot is the direct parent of NewTag),
	// but does not remove the slot tag itself.
	UFUNCTION(BlueprintCallable, Category = "AlienRamen|Tags")
	static bool ReplaceTagInSlot(UPARAM(ref) FGameplayTagContainer& InOutContainer, FGameplayTag NewTag);

private:
	static bool TryGetParentTag(FGameplayTag Tag, FGameplayTag& OutParent);
	static bool SplitTagToParts(FGameplayTag Tag, TArray<FString>& OutParts);
	static bool TryMakeTagFromParts(const TArray<FString>& Parts, int32 NumParts, FGameplayTag& OutTag);
};
