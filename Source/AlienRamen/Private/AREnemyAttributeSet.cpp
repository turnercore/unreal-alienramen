#include "AREnemyAttributeSet.h"

#include "Net/UnrealNetwork.h"

UAREnemyAttributeSet::UAREnemyAttributeSet()
{
	CollisionDamage.SetBaseValue(10.f);
	CollisionDamage.SetCurrentValue(10.f);
}

void UAREnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCollisionDamageAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}

void UAREnemyAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UAREnemyAttributeSet, CollisionDamage, COND_None, REPNOTIFY_Always);
}

void UAREnemyAttributeSet::OnRep_CollisionDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAREnemyAttributeSet, CollisionDamage, OldValue);
}
