#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ParleyPlayerSlotHelpers.generated.h"

UENUM(BlueprintType)
enum class EParleyPlayerSlot : uint8
{
	Unknown = 0 UMETA(ToolTip = "No valid player slot."),
	P1 UMETA(ToolTip = "First player slot."),
	P2 UMETA(ToolTip = "Second player slot.")
};

namespace ParleyPlayerSlot
{
	PARLEY_API FGameplayTag SlotToTag(EParleyPlayerSlot Slot);
	PARLEY_API EParleyPlayerSlot TagToSlot(const FGameplayTag& SlotTag);
	PARLEY_API FGameplayTag GetP1Tag();
	PARLEY_API FGameplayTag GetP2Tag();
	PARLEY_API FGameplayTag GetTagByIndex(int32 SlotIndex);
	PARLEY_API int32 GetIndexForTag(const FGameplayTag& SlotTag);
	PARLEY_API FGameplayTag NormalizeSlotTag(const FGameplayTag& SlotTag, const FGameplayTag& FallbackSlotTag = FGameplayTag());
	PARLEY_API bool IsValidSlotTag(const FGameplayTag& SlotTag);
}
