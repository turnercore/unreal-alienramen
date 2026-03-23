#include "AREnemyASCAttributeWidgetBase.h"

#include "ARASCAttributeWidgetShared.h"
#include "AREnemyAttributeSet.h"

void UAREnemyASCAttributeWidgetBase::BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const
{
	OutDefinitions.Reset();

	ARASCAttributeWidgetShared::AddCoreTrackedAttributes(
		[this, &OutDefinitions](const FGameplayAttribute& Attribute, const TCHAR* Name)
		{
			AddTrackedAttributeDefinition(OutDefinitions, Attribute, Name);
		});
	AddTrackedAttributeDefinition(OutDefinitions, UAREnemyAttributeSet::GetCollisionDamageAttribute(), TEXT("CollisionDamage"));
}

void UAREnemyASCAttributeWidgetBase::HandleTrackedAttributeValueChanged(
	const FARASCTrackedAttributeRuntime& RuntimeState,
	const float NewValue,
	const float OldValue)
{
	Super::HandleTrackedAttributeValueChanged(RuntimeState, NewValue, OldValue);

	EARCoreAttributeType CoreAttributeType = EARCoreAttributeType::Health;
	if (ARASCAttributeWidgetShared::TryResolveCoreAttributeType(RuntimeState.Attribute, CoreAttributeType))
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
