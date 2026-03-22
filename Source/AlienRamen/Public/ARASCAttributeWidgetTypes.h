/**
 * @file ARASCAttributeWidgetTypes.h
 * @brief Shared enum types for ASC attribute-driven widgets.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARASCAttributeWidgetTypes.generated.h"

/**
 * Player-primary combat attributes exposed as explicit widget events.
 */
UENUM(BlueprintType)
enum class EARPlayerPrimaryCombatAttributeType : uint8
{
	Damage,
	FireRate,
	Ammo,
	MaxAmmo
};

/**
 * Player-hat attributes exposed as explicit widget events.
 */
UENUM(BlueprintType)
enum class EARPlayerHatAttributeType : uint8
{
	HatEnergy,
	MaxHatEnergy,
	HatEnergyRegenRate,
	HatPower
};
