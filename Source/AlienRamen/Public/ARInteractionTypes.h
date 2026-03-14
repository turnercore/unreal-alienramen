/**
 * @file ARInteractionTypes.h
 * @brief Shared interaction action cue types for animation/UI reaction routing.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARInteractionTypes.generated.h"

UENUM(BlueprintType)
enum class EARInteractionActionCue : uint8
{
	None UMETA(DisplayName = "None"),
	Throw UMETA(DisplayName = "Throw"),
	Consume UMETA(DisplayName = "Consume"),
	Kick UMETA(DisplayName = "Kick"),
	Slap UMETA(DisplayName = "Slap"),
	Custom UMETA(DisplayName = "Custom")
};

