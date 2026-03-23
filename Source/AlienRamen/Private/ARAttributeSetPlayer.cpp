#include "ARAttributeSetPlayer.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
	void ClampPlayerAttributeDataForNewMax(FGameplayAttributeData& AttributeData, const float NewMaxValue)
	{
		const float ClampedMaxValue = FMath::Max(0.0f, NewMaxValue);
		AttributeData.SetBaseValue(FMath::Clamp(AttributeData.GetBaseValue(), 0.0f, ClampedMaxValue));
		AttributeData.SetCurrentValue(FMath::Clamp(AttributeData.GetCurrentValue(), 0.0f, ClampedMaxValue));
	}
}

UARAttributeSetPlayer::UARAttributeSetPlayer()
{
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

	Strength.SetBaseValue(10.0f);
	Strength.SetCurrentValue(10.0f);

	MeatDropMultiplier.SetBaseValue(1.0f);
	MeatDropMultiplier.SetCurrentValue(1.0f);

	ScrapDropMultiplier.SetBaseValue(1.0f);
	ScrapDropMultiplier.SetCurrentValue(1.0f);
}

void UARAttributeSetPlayer::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetMaxJetpackFuelAttribute()
		|| Attribute == GetMaxSpiceAttribute()
		|| Attribute == GetMaxHatEnergyAttribute()
		|| Attribute == GetMaxAmmoAttribute()
		|| Attribute == GetSecondaryMaxAmmoAttribute()
		|| Attribute == GetSpecialMaxAmmoAttribute())
	{
		NewValue = ClampNonNegative(NewValue);
	}

	if (Attribute == GetCritChanceAttribute())
	{
		NewValue = Clamp01(NewValue);
	}

	if (Attribute == GetHealingDealtMultiplierAttribute()
		|| Attribute == GetRepairRateAttribute()
		|| Attribute == GetStrengthAttribute()
		|| Attribute == GetDodgeDistanceAttribute()
		|| Attribute == GetDodgeDurationAttribute()
		|| Attribute == GetJumpDistanceAttribute()
		|| Attribute == GetJetpackFuelRegenRateAttribute()
		|| Attribute == GetJetpackFuelDrainRateAttribute()
		|| Attribute == GetProjectileSpeedAttribute()
		|| Attribute == GetRangeAttribute()
		|| Attribute == GetLockOnTimeAttribute()
		|| Attribute == GetSpreadMultiplierAttribute()
		|| Attribute == GetCritMultiplierAttribute()
		|| Attribute == GetAmmoAttribute()
		|| Attribute == GetSecondaryDamageAttribute()
		|| Attribute == GetSecondaryFireRateAttribute()
		|| Attribute == GetSecondaryProjectileSpeedAttribute()
		|| Attribute == GetSecondaryRangeAttribute()
		|| Attribute == GetSecondaryAmmoAttribute()
		|| Attribute == GetSpecialDamageAttribute()
		|| Attribute == GetSpecialFireRateAttribute()
		|| Attribute == GetSpecialProjectileSpeedAttribute()
		|| Attribute == GetSpecialRangeAttribute()
		|| Attribute == GetSpecialAmmoAttribute()
		|| Attribute == GetSpiceGainMultiplierAttribute()
		|| Attribute == GetSpiceDrainRateAttribute()
		|| Attribute == GetSpiceShareRatioAttribute()
		|| Attribute == GetHatEnergyRegenRateAttribute()
		|| Attribute == GetHatPowerAttribute()
		|| Attribute == GetReviveSpeedAttribute()
		|| Attribute == GetPickupRadiusAttribute()
		|| Attribute == GetMeatDropMultiplierAttribute()
		|| Attribute == GetScrapDropMultiplierAttribute())
	{
		NewValue = ClampNonNegative(NewValue);
	}

	if (Attribute == GetMaxJetpackFuelAttribute())
	{
		ClampPlayerAttributeDataForNewMax(JetpackFuel, NewValue);
	}
	else if (Attribute == GetMaxSpiceAttribute())
	{
		ClampPlayerAttributeDataForNewMax(Spice, NewValue);
	}
	else if (Attribute == GetMaxHatEnergyAttribute())
	{
		ClampPlayerAttributeDataForNewMax(HatEnergy, NewValue);
	}
	else if (Attribute == GetMaxAmmoAttribute())
	{
		ClampPlayerAttributeDataForNewMax(Ammo, NewValue);
	}
	else if (Attribute == GetSecondaryMaxAmmoAttribute())
	{
		ClampPlayerAttributeDataForNewMax(SecondaryAmmo, NewValue);
	}
	else if (Attribute == GetSpecialMaxAmmoAttribute())
	{
		ClampPlayerAttributeDataForNewMax(SpecialAmmo, NewValue);
	}
}

void UARAttributeSetPlayer::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;

	if (Attribute == GetJetpackFuelAttribute())
	{
		SetJetpackFuel(FMath::Clamp(GetJetpackFuel(), 0.0f, GetMaxJetpackFuel()));
	}
	else if (Attribute == GetSpiceAttribute())
	{
		SetSpice(FMath::Clamp(GetSpice(), 0.0f, GetMaxSpice()));
	}
	else if (Attribute == GetHatEnergyAttribute())
	{
		SetHatEnergy(FMath::Clamp(GetHatEnergy(), 0.0f, GetMaxHatEnergy()));
	}
	else if (Attribute == GetAmmoAttribute())
	{
		SetAmmo(FMath::Clamp(GetAmmo(), 0.0f, GetMaxAmmo()));
	}
	else if (Attribute == GetSecondaryAmmoAttribute())
	{
		SetSecondaryAmmo(FMath::Clamp(GetSecondaryAmmo(), 0.0f, GetSecondaryMaxAmmo()));
	}
	else if (Attribute == GetSpecialAmmoAttribute())
	{
		SetSpecialAmmo(FMath::Clamp(GetSpecialAmmo(), 0.0f, GetSpecialMaxAmmo()));
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		SetCritChance(Clamp01(GetCritChance()));
	}
	else if (Attribute == GetCritMultiplierAttribute())
	{
		SetCritMultiplier(FMath::Max(1.0f, GetCritMultiplier()));
	}
	else if (Attribute == GetHealingDealtMultiplierAttribute())
	{
		SetHealingDealtMultiplier(ClampNonNegative(GetHealingDealtMultiplier()));
	}
	else if (Attribute == GetRepairRateAttribute())
	{
		SetRepairRate(ClampNonNegative(GetRepairRate()));
	}
	else if (Attribute == GetStrengthAttribute())
	{
		SetStrength(ClampNonNegative(GetStrength()));
	}
	else if (Attribute == GetDodgeDistanceAttribute())
	{
		SetDodgeDistance(ClampNonNegative(GetDodgeDistance()));
	}
	else if (Attribute == GetDodgeDurationAttribute())
	{
		SetDodgeDuration(ClampNonNegative(GetDodgeDuration()));
	}
	else if (Attribute == GetJumpDistanceAttribute())
	{
		SetJumpDistance(ClampNonNegative(GetJumpDistance()));
	}
	else if (Attribute == GetJetpackFuelRegenRateAttribute())
	{
		SetJetpackFuelRegenRate(ClampNonNegative(GetJetpackFuelRegenRate()));
	}
	else if (Attribute == GetJetpackFuelDrainRateAttribute())
	{
		SetJetpackFuelDrainRate(ClampNonNegative(GetJetpackFuelDrainRate()));
	}
	else if (Attribute == GetProjectileSpeedAttribute())
	{
		SetProjectileSpeed(ClampNonNegative(GetProjectileSpeed()));
	}
	else if (Attribute == GetRangeAttribute())
	{
		SetRange(ClampNonNegative(GetRange()));
	}
	else if (Attribute == GetLockOnTimeAttribute())
	{
		SetLockOnTime(ClampNonNegative(GetLockOnTime()));
	}
	else if (Attribute == GetSpreadMultiplierAttribute())
	{
		SetSpreadMultiplier(ClampNonNegative(GetSpreadMultiplier()));
	}
	else if (Attribute == GetSpiceGainMultiplierAttribute())
	{
		SetSpiceGainMultiplier(ClampNonNegative(GetSpiceGainMultiplier()));
	}
	else if (Attribute == GetSpiceDrainRateAttribute())
	{
		SetSpiceDrainRate(ClampNonNegative(GetSpiceDrainRate()));
	}
	else if (Attribute == GetSpiceShareRatioAttribute())
	{
		SetSpiceShareRatio(ClampNonNegative(GetSpiceShareRatio()));
	}
	else if (Attribute == GetHatEnergyRegenRateAttribute())
	{
		SetHatEnergyRegenRate(ClampNonNegative(GetHatEnergyRegenRate()));
	}
	else if (Attribute == GetHatPowerAttribute())
	{
		SetHatPower(ClampNonNegative(GetHatPower()));
	}
	else if (Attribute == GetReviveSpeedAttribute())
	{
		SetReviveSpeed(ClampNonNegative(GetReviveSpeed()));
	}
	else if (Attribute == GetPickupRadiusAttribute())
	{
		SetPickupRadius(ClampNonNegative(GetPickupRadius()));
	}
	else if (Attribute == GetMeatDropMultiplierAttribute())
	{
		SetMeatDropMultiplier(ClampNonNegative(GetMeatDropMultiplier()));
	}
	else if (Attribute == GetScrapDropMultiplierAttribute())
	{
		SetScrapDropMultiplier(ClampNonNegative(GetScrapDropMultiplier()));
	}
}

void UARAttributeSetPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#define AR_PLAYER_REPLIFETIME(Prop) DOREPLIFETIME_CONDITION_NOTIFY(UARAttributeSetPlayer, Prop, COND_None, REPNOTIFY_Always)
	AR_PLAYER_REPLIFETIME(HealingDealtMultiplier);
	AR_PLAYER_REPLIFETIME(RepairRate);
	AR_PLAYER_REPLIFETIME(Strength);
	AR_PLAYER_REPLIFETIME(DodgeDistance);
	AR_PLAYER_REPLIFETIME(DodgeDuration);
	AR_PLAYER_REPLIFETIME(JumpDistance);
	AR_PLAYER_REPLIFETIME(JetpackFuel);
	AR_PLAYER_REPLIFETIME(MaxJetpackFuel);
	AR_PLAYER_REPLIFETIME(JetpackFuelRegenRate);
	AR_PLAYER_REPLIFETIME(JetpackFuelDrainRate);
	AR_PLAYER_REPLIFETIME(ProjectileSpeed);
	AR_PLAYER_REPLIFETIME(Range);
	AR_PLAYER_REPLIFETIME(LockOnTime);
	AR_PLAYER_REPLIFETIME(SpreadMultiplier);
	AR_PLAYER_REPLIFETIME(CritChance);
	AR_PLAYER_REPLIFETIME(CritMultiplier);
	AR_PLAYER_REPLIFETIME(Ammo);
	AR_PLAYER_REPLIFETIME(MaxAmmo);
	AR_PLAYER_REPLIFETIME(SecondaryDamage);
	AR_PLAYER_REPLIFETIME(SecondaryFireRate);
	AR_PLAYER_REPLIFETIME(SecondaryProjectileSpeed);
	AR_PLAYER_REPLIFETIME(SecondaryRange);
	AR_PLAYER_REPLIFETIME(SecondaryAmmo);
	AR_PLAYER_REPLIFETIME(SecondaryMaxAmmo);
	AR_PLAYER_REPLIFETIME(SpecialDamage);
	AR_PLAYER_REPLIFETIME(SpecialFireRate);
	AR_PLAYER_REPLIFETIME(SpecialProjectileSpeed);
	AR_PLAYER_REPLIFETIME(SpecialRange);
	AR_PLAYER_REPLIFETIME(SpecialAmmo);
	AR_PLAYER_REPLIFETIME(SpecialMaxAmmo);
	AR_PLAYER_REPLIFETIME(Spice);
	AR_PLAYER_REPLIFETIME(MaxSpice);
	AR_PLAYER_REPLIFETIME(SpiceGainMultiplier);
	AR_PLAYER_REPLIFETIME(SpiceDrainRate);
	AR_PLAYER_REPLIFETIME(SpiceShareRatio);
	AR_PLAYER_REPLIFETIME(HatEnergy);
	AR_PLAYER_REPLIFETIME(MaxHatEnergy);
	AR_PLAYER_REPLIFETIME(HatEnergyRegenRate);
	AR_PLAYER_REPLIFETIME(HatPower);
	AR_PLAYER_REPLIFETIME(ReviveSpeed);
	AR_PLAYER_REPLIFETIME(PickupRadius);
	AR_PLAYER_REPLIFETIME(MeatDropMultiplier);
	AR_PLAYER_REPLIFETIME(ScrapDropMultiplier);
#undef AR_PLAYER_REPLIFETIME
}

#define AR_PLAYER_REP_NOTIFY(Prop) \
	void UARAttributeSetPlayer::OnRep_##Prop(const FGameplayAttributeData& OldValue) \
	{ \
		GAMEPLAYATTRIBUTE_REPNOTIFY(UARAttributeSetPlayer, Prop, OldValue); \
	}

AR_PLAYER_REP_NOTIFY(HealingDealtMultiplier)
AR_PLAYER_REP_NOTIFY(RepairRate)
AR_PLAYER_REP_NOTIFY(Strength)
AR_PLAYER_REP_NOTIFY(DodgeDistance)
AR_PLAYER_REP_NOTIFY(DodgeDuration)
AR_PLAYER_REP_NOTIFY(JumpDistance)
AR_PLAYER_REP_NOTIFY(JetpackFuel)
AR_PLAYER_REP_NOTIFY(MaxJetpackFuel)
AR_PLAYER_REP_NOTIFY(JetpackFuelRegenRate)
AR_PLAYER_REP_NOTIFY(JetpackFuelDrainRate)
AR_PLAYER_REP_NOTIFY(ProjectileSpeed)
AR_PLAYER_REP_NOTIFY(Range)
AR_PLAYER_REP_NOTIFY(LockOnTime)
AR_PLAYER_REP_NOTIFY(SpreadMultiplier)
AR_PLAYER_REP_NOTIFY(CritChance)
AR_PLAYER_REP_NOTIFY(CritMultiplier)
AR_PLAYER_REP_NOTIFY(Ammo)
AR_PLAYER_REP_NOTIFY(MaxAmmo)
AR_PLAYER_REP_NOTIFY(SecondaryDamage)
AR_PLAYER_REP_NOTIFY(SecondaryFireRate)
AR_PLAYER_REP_NOTIFY(SecondaryProjectileSpeed)
AR_PLAYER_REP_NOTIFY(SecondaryRange)
AR_PLAYER_REP_NOTIFY(SecondaryAmmo)
AR_PLAYER_REP_NOTIFY(SecondaryMaxAmmo)
AR_PLAYER_REP_NOTIFY(SpecialDamage)
AR_PLAYER_REP_NOTIFY(SpecialFireRate)
AR_PLAYER_REP_NOTIFY(SpecialProjectileSpeed)
AR_PLAYER_REP_NOTIFY(SpecialRange)
AR_PLAYER_REP_NOTIFY(SpecialAmmo)
AR_PLAYER_REP_NOTIFY(SpecialMaxAmmo)
AR_PLAYER_REP_NOTIFY(Spice)
AR_PLAYER_REP_NOTIFY(MaxSpice)
AR_PLAYER_REP_NOTIFY(SpiceGainMultiplier)
AR_PLAYER_REP_NOTIFY(SpiceDrainRate)
AR_PLAYER_REP_NOTIFY(SpiceShareRatio)
AR_PLAYER_REP_NOTIFY(HatEnergy)
AR_PLAYER_REP_NOTIFY(MaxHatEnergy)
AR_PLAYER_REP_NOTIFY(HatEnergyRegenRate)
AR_PLAYER_REP_NOTIFY(HatPower)
AR_PLAYER_REP_NOTIFY(ReviveSpeed)
AR_PLAYER_REP_NOTIFY(PickupRadius)
AR_PLAYER_REP_NOTIFY(MeatDropMultiplier)
AR_PLAYER_REP_NOTIFY(ScrapDropMultiplier)

#undef AR_PLAYER_REP_NOTIFY
