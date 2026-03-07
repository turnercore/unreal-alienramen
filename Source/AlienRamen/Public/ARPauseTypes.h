/**
 * @file ARPauseTypes.h
 * @brief Shared pause-system enums and contracts for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPauseTypes.generated.h"

UENUM(BlueprintType)
enum class EARPauseExternalReason : uint8
{
	None = 0,
	DialogueShared UMETA(DisplayName = "Dialogue Shared"),
	InvaderFullBlast UMETA(DisplayName = "Invader Full Blast")
};
