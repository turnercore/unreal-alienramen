#pragma once
/**
 * @file ARPlayerTypes.h
 * @brief Shared player-facing enum types for Alien Ramen gameplay/save/runtime APIs.
 */

#include "CoreMinimal.h"
#include "ARPlayerTypes.generated.h"

UENUM(BlueprintType)
enum class EARPlayerSlot : uint8
{
	Unknown = 0,
	P1,
	P2
};

UENUM(BlueprintType)
enum class EARCharacterChoice : uint8
{
	None = 0,
	Brother,
	Sister
};

UENUM(BlueprintType)
enum class EARCoreAttributeType : uint8
{
	Health,
	MaxHealth,
	Spice,
	MaxSpice,
	MoveSpeed
};
