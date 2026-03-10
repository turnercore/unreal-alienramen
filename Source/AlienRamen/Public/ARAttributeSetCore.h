/**
 * @file ARAttributeSetCore.h
 * @brief ARAttributeSetCore header for Alien Ramen.
 */
#pragma once

// ARAttributeSetCore.h


#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ARAttributeSetCore.generated.h"

// Convenience macro for GAS accessors
#define AR_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Core shared attributes for Alien Ramen.
 * Works for players and enemies (enemies can ignore player-only resources).
 */
UCLASS()
class ALIENRAMEN_API UARAttributeSetCore : public UAttributeSet
{
	GENERATED_BODY()

public:
	UARAttributeSetCore();

	// ---- UAttributeSet overrides ----
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// -----------------------
	// Survivability
	// -----------------------
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "AR|Survivability")
	FGameplayAttributeData Health;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Health)

	// Meta attribute used as a temporary sink for incoming damage specs.
	UPROPERTY(BlueprintReadOnly, Category = "AR|Survivability|Meta")
	FGameplayAttributeData IncomingDamage;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, IncomingDamage)


		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "AR|Survivability")
	FGameplayAttributeData MaxHealth;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxHealth)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "AR|Survivability")
	FGameplayAttributeData Shield;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Shield)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "AR|Survivability")
	FGameplayAttributeData MaxShield;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxShield)

		// Regen/degeneration hooks (allow negative for poison/DoT-style systems)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegenRate, Category = "AR|Survivability")
	FGameplayAttributeData HealthRegenRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealthRegenRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegenDelay, Category = "AR|Survivability")
	FGameplayAttributeData HealthRegenDelay;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealthRegenDelay)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldRegenRate, Category = "AR|Survivability")
	FGameplayAttributeData ShieldRegenRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ShieldRegenRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldRegenDelay, Category = "AR|Survivability")
	FGameplayAttributeData ShieldRegenDelay;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ShieldRegenDelay)

		// Global scaling hooks
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageTakenMultiplier, Category = "AR|Survivability")
	FGameplayAttributeData DamageTakenMultiplier; // default 1.0
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, DamageTakenMultiplier)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingReceivedMultiplier, Category = "AR|Survivability")
	FGameplayAttributeData HealingReceivedMultiplier; // default 1.0
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealingReceivedMultiplier)

		// Healing output scaling (enemy healers, player repair tools, etc.)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingDealtMultiplier, Category = "AR|Support")
	FGameplayAttributeData HealingDealtMultiplier; // default 1.0
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealingDealtMultiplier)

		// Repair speed/rate (your repair interaction loop)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RepairRate, Category = "AR|Support")
	FGameplayAttributeData RepairRate; // interpret as "progress per second" or similar
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, RepairRate)

		// -----------------------
		// Movement
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "AR|Movement")
	FGameplayAttributeData MoveSpeed;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MoveSpeed)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeDistance, Category = "AR|Movement")
	FGameplayAttributeData DodgeDistance;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, DodgeDistance)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeDuration, Category = "AR|Movement")
	FGameplayAttributeData DodgeDuration;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, DodgeDuration)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpDistance, Category = "AR|Movement")
	FGameplayAttributeData JumpDistance;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, JumpDistance)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JetpackFuel, Category = "AR|Movement")
	FGameplayAttributeData JetpackFuel;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, JetpackFuel)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxJetpackFuel, Category = "AR|Movement")
	FGameplayAttributeData MaxJetpackFuel;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxJetpackFuel)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JetpackFuelRegenRate, Category = "AR|Movement")
	FGameplayAttributeData JetpackFuelRegenRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, JetpackFuelRegenRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JetpackFuelDrainRate, Category = "AR|Movement")
	FGameplayAttributeData JetpackFuelDrainRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, JetpackFuelDrainRate)

		// -----------------------
		// Combat - Primary lane
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Damage, Category = "AR|Combat|Primary")
	FGameplayAttributeData Damage;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Damage)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireRate, Category = "AR|Combat|Primary")
	FGameplayAttributeData FireRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, FireRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProjectileSpeed, Category = "AR|Combat|Primary")
	FGameplayAttributeData ProjectileSpeed;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ProjectileSpeed)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Range, Category = "AR|Combat|Primary")
	FGameplayAttributeData Range;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Range)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LockOnTime, Category = "AR|Combat|Primary")
	FGameplayAttributeData LockOnTime;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, LockOnTime)

		// Accuracy/spread (multiplier: 1.0 = baseline, <1 tighter, >1 wider)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadMultiplier, Category = "AR|Combat|Primary")
	FGameplayAttributeData SpreadMultiplier; // default 1.0
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpreadMultiplier)

		// Crits
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "AR|Combat|Primary")
	FGameplayAttributeData CritChance; // 0..1
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, CritChance)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritMultiplier, Category = "AR|Combat|Primary")
	FGameplayAttributeData CritMultiplier; // e.g. 1.5
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, CritMultiplier)

		// Ammo - Primary
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Ammo, Category = "AR|Combat|Primary")
	FGameplayAttributeData Ammo;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Ammo)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAmmo, Category = "AR|Combat|Primary")
	FGameplayAttributeData MaxAmmo;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxAmmo)

		// -----------------------
		// Combat - Secondary lane
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryDamage, Category = "AR|Combat|Secondary")
	FGameplayAttributeData SecondaryDamage;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SecondaryDamage)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryFireRate, Category = "AR|Combat|Secondary")
	FGameplayAttributeData SecondaryFireRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SecondaryFireRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryProjectileSpeed, Category = "AR|Combat|Secondary")
	FGameplayAttributeData SecondaryProjectileSpeed;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SecondaryProjectileSpeed)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryRange, Category = "AR|Combat|Secondary")
	FGameplayAttributeData SecondaryRange;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SecondaryRange)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryAmmo, Category = "AR|Combat|Secondary")
	FGameplayAttributeData SecondaryAmmo;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SecondaryAmmo)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryMaxAmmo, Category = "AR|Combat|Secondary")
	FGameplayAttributeData SecondaryMaxAmmo;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SecondaryMaxAmmo)

		// -----------------------
		// Combat - Special lane
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialDamage, Category = "AR|Combat|Special")
	FGameplayAttributeData SpecialDamage;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpecialDamage)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialFireRate, Category = "AR|Combat|Special")
	FGameplayAttributeData SpecialFireRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpecialFireRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialProjectileSpeed, Category = "AR|Combat|Special")
	FGameplayAttributeData SpecialProjectileSpeed;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpecialProjectileSpeed)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialRange, Category = "AR|Combat|Special")
	FGameplayAttributeData SpecialRange;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpecialRange)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialAmmo, Category = "AR|Combat|Special")
	FGameplayAttributeData SpecialAmmo;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpecialAmmo)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialMaxAmmo, Category = "AR|Combat|Special")
	FGameplayAttributeData SpecialMaxAmmo;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpecialMaxAmmo)

		// -----------------------
		// Spice system
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spice, Category = "AR|Spice")
	FGameplayAttributeData Spice;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Spice)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpice, Category = "AR|Spice")
	FGameplayAttributeData MaxSpice;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxSpice)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpiceGainMultiplier, Category = "AR|Spice")
	FGameplayAttributeData SpiceGainMultiplier; // default 1.0
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpiceGainMultiplier)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpiceDrainRate, Category = "AR|Spice")
	FGameplayAttributeData SpiceDrainRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpiceDrainRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpiceShareRatio, Category = "AR|Spice")
	FGameplayAttributeData SpiceShareRatio; // can be 0..N (allow >1)
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, SpiceShareRatio)

		// -----------------------
		// Hat system
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HatEnergy, Category = "AR|Hat")
	FGameplayAttributeData HatEnergy;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HatEnergy)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHatEnergy, Category = "AR|Hat")
	FGameplayAttributeData MaxHatEnergy;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxHatEnergy)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HatEnergyRegenRate, Category = "AR|Hat")
	FGameplayAttributeData HatEnergyRegenRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HatEnergyRegenRate)

		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HatPower, Category = "AR|Hat")
	FGameplayAttributeData HatPower;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HatPower)

		// -----------------------
		// Co-op / Interaction
		// -----------------------
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReviveSpeed, Category = "AR|Support")
	FGameplayAttributeData ReviveSpeed;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ReviveSpeed)

	// Roguelike staple
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PickupRadius, Category = "AR|Support")
	FGameplayAttributeData PickupRadius;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, PickupRadius)

	// Enemy-side baseline drop chance used for invader currency drops.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DropChance, Category = "AR|Rewards|Drops")
	FGameplayAttributeData DropChance;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, DropChance)

	// Enemy-side baseline drop amount authored per definition.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DropAmount, Category = "AR|Rewards|Drops")
	FGameplayAttributeData DropAmount;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, DropAmount)

	// Killer-side multiplier for meat drops.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MeatDropMultiplier, Category = "AR|Rewards|Drops")
	FGameplayAttributeData MeatDropMultiplier;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MeatDropMultiplier)

	// Killer-side multiplier for scrap drops.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ScrapDropMultiplier, Category = "AR|Rewards|Drops")
	FGameplayAttributeData ScrapDropMultiplier;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ScrapDropMultiplier)

protected:
	// Rep notifies
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_HealthRegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealthRegenDelay(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShieldRegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShieldRegenDelay(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealingReceivedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealingDealtMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RepairRate(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DodgeDistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DodgeDuration(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JumpDistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JetpackFuel(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxJetpackFuel(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JetpackFuelRegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JetpackFuelDrainRate(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_Damage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FireRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ProjectileSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Range(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_LockOnTime(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpreadMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_Ammo(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_SecondaryDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryFireRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryProjectileSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryRange(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryAmmo(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryMaxAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_SpecialDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpecialFireRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpecialProjectileSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpecialRange(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpecialAmmo(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpecialMaxAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_Spice(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxSpice(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpiceGainMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpiceDrainRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpiceShareRatio(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_HatEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHatEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HatEnergyRegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HatPower(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_ReviveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_PickupRadius(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DropChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DropAmount(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeatDropMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ScrapDropMultiplier(const FGameplayAttributeData& OldValue);

	// Helpers
	static float Clamp01(float v) { return FMath::Clamp(v, 0.0f, 1.0f); }
	static float ClampNonNegative(float v) { return FMath::Max(0.0f, v); }

	void ClampCurrentToMax(const FGameplayAttribute& CurrentAttr, const FGameplayAttribute& MaxAttr);
};
