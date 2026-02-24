// ARAttributeSetCore.cpp

#include "ARAttributeSetCore.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UARAttributeSetCore::UARAttributeSetCore()
{
	// Reasonable defaults (you can also set these in a GE instead)
	const float DefaultMaxHealth = 100.0f;
	MaxHealth.SetBaseValue(DefaultMaxHealth);
	MaxHealth.SetCurrentValue(DefaultMaxHealth);
	Health.SetBaseValue(DefaultMaxHealth);
	Health.SetCurrentValue(DefaultMaxHealth);

	DamageTakenMultiplier.SetBaseValue(1.0f);
	DamageTakenMultiplier.SetCurrentValue(1.0f);

	HealingReceivedMultiplier.SetBaseValue(1.0f);
	HealingReceivedMultiplier.SetCurrentValue(1.0f);

	HealingDealtMultiplier.SetBaseValue(1.0f);
	HealingDealtMultiplier.SetCurrentValue(1.0f);

	SpiceGainMultiplier.SetBaseValue(1.0f);
	SpiceGainMultiplier.SetCurrentValue(1.0f);

	SpreadMultiplier.SetBaseValue(1.0f);
	SpreadMultiplier.SetCurrentValue(1.0f);

	CritChance.SetBaseValue(0.0f);
	CritChance.SetCurrentValue(0.0f);

	CritMultiplier.SetBaseValue(1.0f);
	CritMultiplier.SetCurrentValue(1.0f);
}

void UARAttributeSetCore::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	// Clamp common "max" attributes to non-negative
	if (Attribute == GetMaxHealthAttribute() ||
		Attribute == GetMaxShieldAttribute() ||
		Attribute == GetMaxJetpackFuelAttribute() ||
		Attribute == GetMaxSpiceAttribute() ||
		Attribute == GetMaxGadgetEnergyAttribute() ||
		Attribute == GetMaxAmmoAttribute() ||
		Attribute == GetSecondaryMaxAmmoAttribute() ||
		Attribute == GetSpecialMaxAmmoAttribute())
	{
		NewValue = ClampNonNegative(NewValue);
	}

	// Clamp [0..1] style attributes
	if (Attribute == GetCritChanceAttribute())
	{
		NewValue = Clamp01(NewValue);
	}

	// Multipliers that should never go negative
	if (Attribute == GetDamageTakenMultiplierAttribute() ||
		Attribute == GetHealingReceivedMultiplierAttribute() ||
		Attribute == GetHealingDealtMultiplierAttribute() ||
		Attribute == GetSpiceGainMultiplierAttribute() ||
		Attribute == GetSpreadMultiplierAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}

	// Keep current values within new max values
	if (Attribute == GetMaxHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, NewValue));
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.0f, NewValue));
	}
	else if (Attribute == GetMaxJetpackFuelAttribute())
	{
		SetJetpackFuel(FMath::Clamp(GetJetpackFuel(), 0.0f, NewValue));
	}
	else if (Attribute == GetMaxSpiceAttribute())
	{
		SetSpice(FMath::Clamp(GetSpice(), 0.0f, NewValue));
	}
	else if (Attribute == GetMaxGadgetEnergyAttribute())
	{
		SetGadgetEnergy(FMath::Clamp(GetGadgetEnergy(), 0.0f, NewValue));
	}
	else if (Attribute == GetMaxAmmoAttribute())
	{
		SetAmmo(FMath::Clamp(GetAmmo(), 0.0f, NewValue));
	}
	else if (Attribute == GetSecondaryMaxAmmoAttribute())
	{
		SetSecondaryAmmo(FMath::Clamp(GetSecondaryAmmo(), 0.0f, NewValue));
	}
	else if (Attribute == GetSpecialMaxAmmoAttribute())
	{
		SetSpecialAmmo(FMath::Clamp(GetSpecialAmmo(), 0.0f, NewValue));
	}
}

void UARAttributeSetCore::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	const FGameplayAttribute& Attr = Data.EvaluatedData.Attribute;

	// Clamp currents to [0..Max]
	if (Attr == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Attr == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.0f, GetMaxShield()));
	}
	else if (Attr == GetJetpackFuelAttribute())
	{
		SetJetpackFuel(FMath::Clamp(GetJetpackFuel(), 0.0f, GetMaxJetpackFuel()));
	}
	else if (Attr == GetSpiceAttribute())
	{
		SetSpice(FMath::Clamp(GetSpice(), 0.0f, GetMaxSpice()));
	}
	else if (Attr == GetGadgetEnergyAttribute())
	{
		SetGadgetEnergy(FMath::Clamp(GetGadgetEnergy(), 0.0f, GetMaxGadgetEnergy()));
	}
	else if (Attr == GetAmmoAttribute())
	{
		SetAmmo(FMath::Clamp(GetAmmo(), 0.0f, GetMaxAmmo()));
	}
	else if (Attr == GetSecondaryAmmoAttribute())
	{
		SetSecondaryAmmo(FMath::Clamp(GetSecondaryAmmo(), 0.0f, GetSecondaryMaxAmmo()));
	}
	else if (Attr == GetSpecialAmmoAttribute())
	{
		SetSpecialAmmo(FMath::Clamp(GetSpecialAmmo(), 0.0f, GetSpecialMaxAmmo()));
	}

	// Clamp [0..1] attributes post-mod too
	if (Attr == GetCritChanceAttribute())
	{
		SetCritChance(Clamp01(GetCritChance()));
	}

	// Non-negative clamps
	if (Attr == GetDamageTakenMultiplierAttribute())
	{
		SetDamageTakenMultiplier(FMath::Max(0.0f, GetDamageTakenMultiplier()));
	}
	else if (Attr == GetHealingReceivedMultiplierAttribute())
	{
		SetHealingReceivedMultiplier(FMath::Max(0.0f, GetHealingReceivedMultiplier()));
	}
	else if (Attr == GetHealingDealtMultiplierAttribute())
	{
		SetHealingDealtMultiplier(FMath::Max(0.0f, GetHealingDealtMultiplier()));
	}
	else if (Attr == GetSpiceGainMultiplierAttribute())
	{
		SetSpiceGainMultiplier(FMath::Max(0.0f, GetSpiceGainMultiplier()));
	}
	else if (Attr == GetSpreadMultiplierAttribute())
	{
		SetSpreadMultiplier(FMath::Max(0.0f, GetSpreadMultiplier()));
	}

	// Optional: keep crit multiplier sensible (>= 1)
	if (Attr == GetCritMultiplierAttribute())
	{
		SetCritMultiplier(FMath::Max(1.0f, GetCritMultiplier()));
	}
}

void UARAttributeSetCore::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Use REPNOTIFY_Always so UI reliably updates even if value is "same" due to reapplication
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
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, HealingDealtMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, RepairRate, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, DodgeDistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, DodgeDuration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, JumpDistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, JetpackFuel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MaxJetpackFuel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, JetpackFuelRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, JetpackFuelDrainRate, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Damage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, FireRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, ProjectileSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Range, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, LockOnTime, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpreadMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, CritMultiplier, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Ammo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MaxAmmo, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SecondaryDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SecondaryFireRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SecondaryProjectileSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SecondaryRange, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SecondaryAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SecondaryMaxAmmo, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpecialDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpecialFireRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpecialProjectileSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpecialRange, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpecialAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpecialMaxAmmo, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, Spice, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MaxSpice, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpiceGainMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpiceDrainRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, SpiceShareRatio, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, GadgetEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, MaxGadgetEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, GadgetEnergyRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, GadgetPower, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, ReviveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetCore, PickupRadius, COND_None, REPNOTIFY_Always);
}

// Rep notify implementations
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
AR_REP_NOTIFY(HealingDealtMultiplier)
AR_REP_NOTIFY(RepairRate)

AR_REP_NOTIFY(MoveSpeed)
AR_REP_NOTIFY(DodgeDistance)
AR_REP_NOTIFY(DodgeDuration)
AR_REP_NOTIFY(JumpDistance)
AR_REP_NOTIFY(JetpackFuel)
AR_REP_NOTIFY(MaxJetpackFuel)
AR_REP_NOTIFY(JetpackFuelRegenRate)
AR_REP_NOTIFY(JetpackFuelDrainRate)

AR_REP_NOTIFY(Damage)
AR_REP_NOTIFY(FireRate)
AR_REP_NOTIFY(ProjectileSpeed)
AR_REP_NOTIFY(Range)
AR_REP_NOTIFY(LockOnTime)
AR_REP_NOTIFY(SpreadMultiplier)
AR_REP_NOTIFY(CritChance)
AR_REP_NOTIFY(CritMultiplier)

AR_REP_NOTIFY(Ammo)
AR_REP_NOTIFY(MaxAmmo)

AR_REP_NOTIFY(SecondaryDamage)
AR_REP_NOTIFY(SecondaryFireRate)
AR_REP_NOTIFY(SecondaryProjectileSpeed)
AR_REP_NOTIFY(SecondaryRange)
AR_REP_NOTIFY(SecondaryAmmo)
AR_REP_NOTIFY(SecondaryMaxAmmo)

AR_REP_NOTIFY(SpecialDamage)
AR_REP_NOTIFY(SpecialFireRate)
AR_REP_NOTIFY(SpecialProjectileSpeed)
AR_REP_NOTIFY(SpecialRange)
AR_REP_NOTIFY(SpecialAmmo)
AR_REP_NOTIFY(SpecialMaxAmmo)

AR_REP_NOTIFY(Spice)
AR_REP_NOTIFY(MaxSpice)
AR_REP_NOTIFY(SpiceGainMultiplier)
AR_REP_NOTIFY(SpiceDrainRate)
AR_REP_NOTIFY(SpiceShareRatio)

AR_REP_NOTIFY(GadgetEnergy)
AR_REP_NOTIFY(MaxGadgetEnergy)
AR_REP_NOTIFY(GadgetEnergyRegenRate)
AR_REP_NOTIFY(GadgetPower)

AR_REP_NOTIFY(ReviveSpeed)
AR_REP_NOTIFY(PickupRadius)

#undef AR_REP_NOTIFY
