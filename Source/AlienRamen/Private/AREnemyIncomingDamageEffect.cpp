#include "AREnemyIncomingDamageEffect.h"

#include "ARAttributeSetCore.h"
#include "GameplayTagsManager.h"

UAREnemyIncomingDamageEffect::UAREnemyIncomingDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& DamageMod = Modifiers.AddDefaulted_GetRef();
	DamageMod.Attribute = UARAttributeSetCore::GetIncomingDamageAttribute();
	DamageMod.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"), false);
	DamageMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
