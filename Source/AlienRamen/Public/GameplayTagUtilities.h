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
	// -----------------------
	// Tag path helpers
	// -----------------------
	// Returns ONLY the direct children of ParentTag (one level deeper), not grandchildren.
	// Example: Parent=Unlocks.Ships -> [Unlocks.Ships.Ship1, Unlocks.Ships.Ship2, ...]
	UFUNCTION(BlueprintCallable, Category = "AlienRamen|Tags")
	static bool GetDirectChildrenOfTag(
		FGameplayTag ParentTag,
		TArray<FGameplayTag>& OutDirectChildren,
		bool& bFoundAny,
		FGameplayTag& OutFirstChild
	);

	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static int32 GetTagDepth(FGameplayTag Tag);

	// Direct parent: Unlocks.Ships.Ship1 -> Unlocks.Ships
	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static bool TryGetParentTag(FGameplayTag Tag, FGameplayTag& OutParent);

	// N levels up: UpLevels=2: Unlocks.Ships.Ship1 -> Unlocks
	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static bool TryGetAncestorTag(FGameplayTag Tag, int32 UpLevels, FGameplayTag& OutAncestor);

	// Top-level: Unlocks.Ships.Ship1 -> Unlocks
	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static bool TryGetTopLevelTag(FGameplayTag Tag, FGameplayTag& OutTopLevel);

	// Get tag at depth: Depth=2 => Unlocks.Ships.Ship1 -> Unlocks.Ships
	// Depth is 1-based: 1="Unlocks", 2="Unlocks.Ships", etc.
	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static bool TryGetTagAtDepth(FGameplayTag Tag, int32 Depth, FGameplayTag& OutTagAtDepth);

	// Returns all prefixes up to depth (optional convenience):
	// Unlocks.Ships.Ship1 -> [Unlocks, Unlocks.Ships, Unlocks.Ships.Ship1]
	UFUNCTION(BlueprintPure, Category = "AlienRamen|Tags")
	static bool TryGetAllPrefixTags(FGameplayTag Tag, FGameplayTagContainer& OutPrefixes);

	// -----------------------
	// Slot replacement helper
	// -----------------------

	// Replaces everything under "slot" (slot is 1 level up from NewTag),
	// but does not remove the slot tag itself.
	//
	// Examples:
	// - NewTag = Unlocks.Ships.Ship2
	//   Slot = Unlocks.Ships
	//   Removes Unlocks.Ships.* (except Unlocks.Ships), then adds Ship2
	//
	// - NewTag = Unlocks.Ships.Ship1.Laser
	//   Slot = Unlocks.Ships.Ship1
	//   Removes Unlocks.Ships.Ship1.* (except Unlocks.Ships.Ship1), then adds Laser
	UFUNCTION(BlueprintCallable, Category = "AlienRamen|Tags")
	static bool ReplaceTagInSlot(UPARAM(ref) FGameplayTagContainer& InOutContainer, FGameplayTag NewTag);

private:
	static bool SplitTagToParts(FGameplayTag Tag, TArray<FString>& OutParts);
	static bool TryMakeTagFromParts(const TArray<FString>& Parts, int32 NumParts, FGameplayTag& OutTag);
};
