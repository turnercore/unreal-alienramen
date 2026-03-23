#pragma once

#include "ARAttributeSetCore.h"
#include "ARAttributeSetPlayer.h"
#include "ARPlayerTypes.h"

namespace ARASCAttributeWidgetShared
{
	template <typename TAddDefinitionFn>
	void AddCoreTrackedAttributes(TAddDefinitionFn&& AddDefinition)
	{
		AddDefinition(UARAttributeSetCore::GetHealthAttribute(), TEXT("Health"));
		AddDefinition(UARAttributeSetCore::GetMaxHealthAttribute(), TEXT("MaxHealth"));
		AddDefinition(UARAttributeSetCore::GetMoveSpeedAttribute(), TEXT("MoveSpeed"));
	}

	template <typename TAddDefinitionFn>
	void AddPlayerTrackedAttributes(TAddDefinitionFn&& AddDefinition)
	{
		AddDefinition(UARAttributeSetPlayer::GetSpiceAttribute(), TEXT("Spice"));
		AddDefinition(UARAttributeSetPlayer::GetMaxSpiceAttribute(), TEXT("MaxSpice"));
		AddDefinition(UARAttributeSetPlayer::GetStrengthAttribute(), TEXT("Strength"));
	}

	inline bool TryResolveCoreAttributeType(const FGameplayAttribute& Attribute, EARCoreAttributeType& OutType)
	{
		if (Attribute == UARAttributeSetCore::GetHealthAttribute())
		{
			OutType = EARCoreAttributeType::Health;
			return true;
		}

		if (Attribute == UARAttributeSetCore::GetMaxHealthAttribute())
		{
			OutType = EARCoreAttributeType::MaxHealth;
			return true;
		}

		if (Attribute == UARAttributeSetCore::GetMoveSpeedAttribute())
		{
			OutType = EARCoreAttributeType::MoveSpeed;
			return true;
		}

		return false;
	}

	inline bool TryResolvePlayerAttributeType(const FGameplayAttribute& Attribute, EARPlayerAttributeType& OutType)
	{
		if (Attribute == UARAttributeSetPlayer::GetSpiceAttribute())
		{
			OutType = EARPlayerAttributeType::Spice;
			return true;
		}

		if (Attribute == UARAttributeSetPlayer::GetMaxSpiceAttribute())
		{
			OutType = EARPlayerAttributeType::MaxSpice;
			return true;
		}

		if (Attribute == UARAttributeSetPlayer::GetStrengthAttribute())
		{
			OutType = EARPlayerAttributeType::Strength;
			return true;
		}

		return false;
	}
}
