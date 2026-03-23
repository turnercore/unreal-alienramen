#include "ARAttributeSetCore.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UARAttributeSetCore::UARAttributeSetCore()
{
	const float DefaultMaxHealth = 100.0f;
	MaxHealth.SetBaseValue(DefaultMaxHealth);
	MaxHealth.SetCurrentValue(DefaultMaxHealth);
	Health.SetBaseValue(DefaultMaxHealth);
	Health.SetCurrentValue(DefaultMaxHealth);
	IncomingDamage.SetBaseValue(0.0f);
	IncomingDamage.SetCurrentValue(0.0f);

	DamageTakenMultiplier.SetBaseValue(1.0f);
	DamageTakenMultiplier.SetCurrentValue(1.0f);

	HealingReceivedMultiplier.SetBaseValue(1.0f);
	HealingReceivedMultiplier.SetCurrentValue(1.0f);
}

void UARAttributeSetCore::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldAttribute())
	{
		NewValue = ClampNonNegative(NewValue);
	}

	if (Attribute == GetDamageTakenMultiplierAttribute() || Attribute == GetHealingReceivedMultiplierAttribute())
	{
		NewValue = ClampNonNegative(NewValue);
	}

	if (Attribute == GetMoveSpeedAttribute() || Attribute == GetDamageAttribute() || Attribute == GetFireRateAttribute())
	{
		NewValue = ClampNonNegative(NewValue);
	}

	if (Attribute == GetMaxHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, NewValue));
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.0f, NewValue));
	}
}

void UARAttributeSetCore::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	const FGameplayAttribute& Attr = Data.EvaluatedData.Attribute;

	if (Attr == GetIncomingDamageAttribute())
	{
		const float PendingDamage = FMath::Max(0.0f, GetIncomingDamage());
		SetIncomingDamage(0.0f);

		if (PendingDamage > 0.0f)
		{
			float RemainingDamage = PendingDamage * FMath::Max(0.0f, GetDamageTakenMultiplier());

			if (GetMaxShield() > 0.0f && GetShield() > 0.0f)
			{
				const float PreviousShield = GetShield();
				SetShield(FMath::Max(0.0f, PreviousShield - RemainingDamage));
				RemainingDamage = FMath::Max(0.0f, RemainingDamage - PreviousShield);
			}

			if (RemainingDamage > 0.0f)
			{
				SetHealth(FMath::Clamp(GetHealth() - RemainingDamage, 0.0f, GetMaxHealth()));
			}
		}

		return;
	}

	if (Attr == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Attr == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.0f, GetMaxShield()));
	}
	else if (Attr == GetDamageTakenMultiplierAttribute())
	{
		SetDamageTakenMultiplier(FMath::Max(0.0f, GetDamageTakenMultiplier()));
	}
	else if (Attr == GetHealingReceivedMultiplierAttribute())
	{
		SetHealingReceivedMultiplier(FMath::Max(0.0f, GetHealingReceivedMultiplier()));
	}
	else if (Attr == GetMoveSpeedAttribute())
	{
		SetMoveSpeed(FMath::Max(0.0f, GetMoveSpeed()));
	}
	else if (Attr == GetDamageAttribute())
	{
		SetDamage(FMath::Max(0.0f, GetDamage()));
	}
	else if (Attr == GetFireRateAttribute())
	{
		SetFireRate(FMath::Max(0.0f, GetFireRate()));
	}
}

void UARAttributeSetCore::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MaxShield, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, HealthRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, HealthRegenDelay, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, ShieldRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, ShieldRegenDelay, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, DamageTakenMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, HealingReceivedMultiplier, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Damage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, FireRate, COND_None, REPNOTIFY_Always);
}

#define AR_REP_NOTIFY(Prop) \
void UARAttributeSetCore::OnRep_##Prop(const FGameplayAttributeData& OldValue) \
{ \
	GAMEPLAYATTRIBUTE_REPNOTIFY(UARAttributeSetCore, Prop, OldValue); \
}

AR_REP_NOTIFY(Health)
AR_REP_NOTIFY(MaxHealth)
AR_REP_NOTIFY(Shield)
AR_REP_NOTIFY(MaxShield)
AR_REP_NOTIFY(HealthRegenRate)
AR_REP_NOTIFY(HealthRegenDelay)
AR_REP_NOTIFY(ShieldRegenRate)
AR_REP_NOTIFY(ShieldRegenDelay)
AR_REP_NOTIFY(DamageTakenMultiplier)
AR_REP_NOTIFY(HealingReceivedMultiplier)
AR_REP_NOTIFY(MoveSpeed)
AR_REP_NOTIFY(Damage)
AR_REP_NOTIFY(FireRate)

#undef AR_REP_NOTIFY
