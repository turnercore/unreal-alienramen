#include "AREnemyAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UAREnemyAttributeSet::UAREnemyAttributeSet()
{
	CollisionDamage.SetBaseValue(10.f);
	CollisionDamage.SetCurrentValue(10.f);

	DropChance.SetBaseValue(0.0f);
	DropChance.SetCurrentValue(0.0f);

	DropAmount.SetBaseValue(0.0f);
	DropAmount.SetCurrentValue(0.0f);
}

void UAREnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCollisionDamageAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetDropChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	}
	else if (Attribute == GetDropAmountAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UAREnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetCollisionDamageAttribute())
	{
		SetCollisionDamage(FMath::Max(0.0f, GetCollisionDamage()));
	}
	else if (Attribute == GetDropChanceAttribute())
	{
		SetDropChance(FMath::Clamp(GetDropChance(), 0.0f, 1.0f));
	}
	else if (Attribute == GetDropAmountAttribute())
	{
		SetDropAmount(FMath::Max(0.0f, GetDropAmount()));
	}
}

void UAREnemyAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UAREnemyAttributeSet, CollisionDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAREnemyAttributeSet, DropChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAREnemyAttributeSet, DropAmount, COND_None, REPNOTIFY_Always);
}

void UAREnemyAttributeSet::OnRep_CollisionDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAREnemyAttributeSet, CollisionDamage, OldValue);
}

void UAREnemyAttributeSet::OnRep_DropChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAREnemyAttributeSet, DropChance, OldValue);
}

void UAREnemyAttributeSet::OnRep_DropAmount(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAREnemyAttributeSet, DropAmount, OldValue);
}
