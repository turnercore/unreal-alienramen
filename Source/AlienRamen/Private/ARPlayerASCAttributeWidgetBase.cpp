#include "ARPlayerASCAttributeWidgetBase.h"

#include "ARASCAttributeWidgetShared.h"
#include "ARAttributeSetCore.h"

void UARPlayerASCAttributeWidgetBase::BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const
{
	OutDefinitions.Reset();

	ARASCAttributeWidgetShared::AddCoreTrackedAttributes(
		[this, &OutDefinitions](const FGameplayAttribute& Attribute, const TCHAR* Name)
		{
			AddTrackedAttributeDefinition(OutDefinitions, Attribute, Name);
		});

	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetDamageAttribute(), TEXT("Damage"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetFireRateAttribute(), TEXT("FireRate"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetAmmoAttribute(), TEXT("Ammo"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetMaxAmmoAttribute(), TEXT("MaxAmmo"));

	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetHatEnergyAttribute(), TEXT("HatEnergy"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetMaxHatEnergyAttribute(), TEXT("MaxHatEnergy"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetHatEnergyRegenRateAttribute(), TEXT("HatEnergyRegenRate"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetHatPowerAttribute(), TEXT("HatPower"));
}

void UARPlayerASCAttributeWidgetBase::HandleTrackedAttributeValueChanged(
	const FARASCTrackedAttributeRuntime& RuntimeState,
	const float NewValue,
	const float OldValue)
{
	Super::HandleTrackedAttributeValueChanged(RuntimeState, NewValue, OldValue);

	EARCoreAttributeType CoreAttributeType = EARCoreAttributeType::Health;
	if (ARASCAttributeWidgetShared::TryResolveCoreAttributeType(RuntimeState.Attribute, CoreAttributeType))
	{
		OnPlayerASCWidgetCoreAttributeChanged.Broadcast(CoreAttributeType, NewValue, OldValue);
		BP_OnPlayerASCWidgetCoreAttributeChanged(CoreAttributeType, NewValue, OldValue);
	}

	EARPlayerPrimaryCombatAttributeType PrimaryAttributeType = EARPlayerPrimaryCombatAttributeType::Damage;
	if (TryResolvePrimaryAttributeType(RuntimeState.Attribute, PrimaryAttributeType))
	{
		OnPlayerASCWidgetPrimaryAttributeChanged.Broadcast(PrimaryAttributeType, NewValue, OldValue);
		BP_OnPlayerASCWidgetPrimaryAttributeChanged(PrimaryAttributeType, NewValue, OldValue);
	}

	EARPlayerHatAttributeType HatAttributeType = EARPlayerHatAttributeType::HatEnergy;
	if (TryResolveHatAttributeType(RuntimeState.Attribute, HatAttributeType))
	{
		OnPlayerASCWidgetHatAttributeChanged.Broadcast(HatAttributeType, NewValue, OldValue);
		BP_OnPlayerASCWidgetHatAttributeChanged(HatAttributeType, NewValue, OldValue);
	}
}
bool UARPlayerASCAttributeWidgetBase::TryResolvePrimaryAttributeType(
	const FGameplayAttribute& Attribute,
	EARPlayerPrimaryCombatAttributeType& OutType)
{
	if (Attribute == UARAttributeSetCore::GetDamageAttribute())
	{
		OutType = EARPlayerPrimaryCombatAttributeType::Damage;
		return true;
	}

	if (Attribute == UARAttributeSetCore::GetFireRateAttribute())
	{
		OutType = EARPlayerPrimaryCombatAttributeType::FireRate;
		return true;
	}

	if (Attribute == UARAttributeSetCore::GetAmmoAttribute())
	{
		OutType = EARPlayerPrimaryCombatAttributeType::Ammo;
		return true;
	}

	if (Attribute == UARAttributeSetCore::GetMaxAmmoAttribute())
	{
		OutType = EARPlayerPrimaryCombatAttributeType::MaxAmmo;
		return true;
	}

	return false;
}

bool UARPlayerASCAttributeWidgetBase::TryResolveHatAttributeType(const FGameplayAttribute& Attribute, EARPlayerHatAttributeType& OutType)
{
	if (Attribute == UARAttributeSetCore::GetHatEnergyAttribute())
	{
		OutType = EARPlayerHatAttributeType::HatEnergy;
		return true;
	}

	if (Attribute == UARAttributeSetCore::GetMaxHatEnergyAttribute())
	{
		OutType = EARPlayerHatAttributeType::MaxHatEnergy;
		return true;
	}

	if (Attribute == UARAttributeSetCore::GetHatEnergyRegenRateAttribute())
	{
		OutType = EARPlayerHatAttributeType::HatEnergyRegenRate;
		return true;
	}

	if (Attribute == UARAttributeSetCore::GetHatPowerAttribute())
	{
		OutType = EARPlayerHatAttributeType::HatPower;
		return true;
	}

	return false;
}
