#pragma once

#include "ARAttributeSetCore.h"
#include "ARPlayerTypes.h"

namespace ARASCAttributeWidgetShared
{
	template <typename TAddDefinitionFn>
	void AddCoreTrackedAttributes(TAddDefinitionFn&& AddDefinition)
	{
		AddDefinition(UARAttributeSetCore::GetHealthAttribute(), TEXT("Health"));
		AddDefinition(UARAttributeSetCore::GetMaxHealthAttribute(), TEXT("MaxHealth"));
		AddDefinition(UARAttributeSetCore::GetSpiceAttribute(), TEXT("Spice"));
		AddDefinition(UARAttributeSetCore::GetMaxSpiceAttribute(), TEXT("MaxSpice"));
		AddDefinition(UARAttributeSetCore::GetMoveSpeedAttribute(), TEXT("MoveSpeed"));
		AddDefinition(UARAttributeSetCore::GetStrengthAttribute(), TEXT("Strength"));
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

		if (Attribute == UARAttributeSetCore::GetSpiceAttribute())
		{
			OutType = EARCoreAttributeType::Spice;
			return true;
		}

		if (Attribute == UARAttributeSetCore::GetMaxSpiceAttribute())
		{
			OutType = EARCoreAttributeType::MaxSpice;
			return true;
		}

		if (Attribute == UARAttributeSetCore::GetMoveSpeedAttribute())
		{
			OutType = EARCoreAttributeType::MoveSpeed;
			return true;
		}

		if (Attribute == UARAttributeSetCore::GetStrengthAttribute())
		{
			OutType = EARCoreAttributeType::Strength;
			return true;
		}

		return false;
	}
}
