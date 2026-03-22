#include "AREnemyASCAttributeWidgetBase.h"

#include "ARAttributeSetCore.h"
#include "AREnemyAttributeSet.h"

void UAREnemyASCAttributeWidgetBase::BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const
{
	OutDefinitions.Reset();

	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetHealthAttribute(), TEXT("Health"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetMaxHealthAttribute(), TEXT("MaxHealth"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetSpiceAttribute(), TEXT("Spice"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetMaxSpiceAttribute(), TEXT("MaxSpice"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetMoveSpeedAttribute(), TEXT("MoveSpeed"));
	AddTrackedAttributeDefinition(OutDefinitions, UARAttributeSetCore::GetStrengthAttribute(), TEXT("Strength"));
	AddTrackedAttributeDefinition(OutDefinitions, UAREnemyAttributeSet::GetCollisionDamageAttribute(), TEXT("CollisionDamage"));
}

void UAREnemyASCAttributeWidgetBase::HandleTrackedAttributeValueChanged(
	const FARASCTrackedAttributeRuntime& RuntimeState,
	const float NewValue,
	const float OldValue)
{
	Super::HandleTrackedAttributeValueChanged(RuntimeState, NewValue, OldValue);

	EARCoreAttributeType CoreAttributeType = EARCoreAttributeType::Health;
	if (TryResolveCoreAttributeType(RuntimeState.Attribute, CoreAttributeType))
	{
		OnEnemyASCWidgetCoreAttributeChanged.Broadcast(CoreAttributeType, NewValue, OldValue);
		BP_OnEnemyASCWidgetCoreAttributeChanged(CoreAttributeType, NewValue, OldValue);
	}

	if (RuntimeState.Attribute == UAREnemyAttributeSet::GetCollisionDamageAttribute())
	{
		OnEnemyASCWidgetCollisionDamageChanged.Broadcast(NewValue, OldValue);
		BP_OnEnemyASCWidgetCollisionDamageChanged(NewValue, OldValue);
	}
}

bool UAREnemyASCAttributeWidgetBase::TryResolveCoreAttributeType(const FGameplayAttribute& Attribute, EARCoreAttributeType& OutType)
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
