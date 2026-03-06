/**
 * @file ARColorTypes.h
 * @brief Shared gameplay color types used across enemies/players/rewards.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.generated.h"

UENUM(BlueprintType)
enum class EARAffinityColor : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Unknown = None UMETA(Hidden),
	Red,
	White,
	Blue
};
