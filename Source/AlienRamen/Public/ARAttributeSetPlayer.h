/**
 * @file ARAttributeSetPlayer.h
 * @brief Player-domain GAS attributes for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ARAttributeSetPlayer.generated.h"

#define AR_PLAYER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Player-only attributes for progression, player verbs, and player-centric combat tuning.
 */
UCLASS()
class ALIENRAMEN_API UARAttributeSetPlayer : public UAttributeSet
{
	GENERATED_BODY()

public:
	UARAttributeSetPlayer();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingDealtMultiplier, Category = "AR|Player|Support")
	FGameplayAttributeData HealingDealtMultiplier;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, HealingDealtMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RepairRate, Category = "AR|Player|Support")
	FGameplayAttributeData RepairRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, RepairRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "AR|Player|Support")
	FGameplayAttributeData Strength;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, Strength)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeDistance, Category = "AR|Player|Movement")
	FGameplayAttributeData DodgeDistance;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, DodgeDistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeDuration, Category = "AR|Player|Movement")
	FGameplayAttributeData DodgeDuration;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, DodgeDuration)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpDistance, Category = "AR|Player|Movement")
	FGameplayAttributeData JumpDistance;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, JumpDistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JetpackFuel, Category = "AR|Player|Movement")
	FGameplayAttributeData JetpackFuel;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, JetpackFuel)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxJetpackFuel, Category = "AR|Player|Movement")
	FGameplayAttributeData MaxJetpackFuel;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, MaxJetpackFuel)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JetpackFuelRegenRate, Category = "AR|Player|Movement")
	FGameplayAttributeData JetpackFuelRegenRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, JetpackFuelRegenRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JetpackFuelDrainRate, Category = "AR|Player|Movement")
	FGameplayAttributeData JetpackFuelDrainRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, JetpackFuelDrainRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProjectileSpeed, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData ProjectileSpeed;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, ProjectileSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Range, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData Range;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, Range)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LockOnTime, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData LockOnTime;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, LockOnTime)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadMultiplier, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData SpreadMultiplier;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpreadMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData CritChance;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, CritChance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritMultiplier, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData CritMultiplier;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, CritMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Ammo, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData Ammo;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, Ammo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAmmo, Category = "AR|Player|Combat|Primary")
	FGameplayAttributeData MaxAmmo;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, MaxAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryDamage, Category = "AR|Player|Combat|Secondary")
	FGameplayAttributeData SecondaryDamage;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SecondaryDamage)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryFireRate, Category = "AR|Player|Combat|Secondary")
	FGameplayAttributeData SecondaryFireRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SecondaryFireRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryProjectileSpeed, Category = "AR|Player|Combat|Secondary")
	FGameplayAttributeData SecondaryProjectileSpeed;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SecondaryProjectileSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryRange, Category = "AR|Player|Combat|Secondary")
	FGameplayAttributeData SecondaryRange;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SecondaryRange)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryAmmo, Category = "AR|Player|Combat|Secondary")
	FGameplayAttributeData SecondaryAmmo;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SecondaryAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SecondaryMaxAmmo, Category = "AR|Player|Combat|Secondary")
	FGameplayAttributeData SecondaryMaxAmmo;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SecondaryMaxAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialDamage, Category = "AR|Player|Combat|Special")
	FGameplayAttributeData SpecialDamage;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpecialDamage)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialFireRate, Category = "AR|Player|Combat|Special")
	FGameplayAttributeData SpecialFireRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpecialFireRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialProjectileSpeed, Category = "AR|Player|Combat|Special")
	FGameplayAttributeData SpecialProjectileSpeed;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpecialProjectileSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialRange, Category = "AR|Player|Combat|Special")
	FGameplayAttributeData SpecialRange;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpecialRange)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialAmmo, Category = "AR|Player|Combat|Special")
	FGameplayAttributeData SpecialAmmo;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpecialAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialMaxAmmo, Category = "AR|Player|Combat|Special")
	FGameplayAttributeData SpecialMaxAmmo;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpecialMaxAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spice, Category = "AR|Player|Spice")
	FGameplayAttributeData Spice;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, Spice)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpice, Category = "AR|Player|Spice")
	FGameplayAttributeData MaxSpice;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, MaxSpice)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpiceGainMultiplier, Category = "AR|Player|Spice")
	FGameplayAttributeData SpiceGainMultiplier;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpiceGainMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpiceDrainRate, Category = "AR|Player|Spice")
	FGameplayAttributeData SpiceDrainRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpiceDrainRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpiceShareRatio, Category = "AR|Player|Spice")
	FGameplayAttributeData SpiceShareRatio;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, SpiceShareRatio)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HatEnergy, Category = "AR|Player|Hat")
	FGameplayAttributeData HatEnergy;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, HatEnergy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHatEnergy, Category = "AR|Player|Hat")
	FGameplayAttributeData MaxHatEnergy;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, MaxHatEnergy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HatEnergyRegenRate, Category = "AR|Player|Hat")
	FGameplayAttributeData HatEnergyRegenRate;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, HatEnergyRegenRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HatPower, Category = "AR|Player|Hat")
	FGameplayAttributeData HatPower;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, HatPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReviveSpeed, Category = "AR|Player|Support")
	FGameplayAttributeData ReviveSpeed;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, ReviveSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PickupRadius, Category = "AR|Player|Support")
	FGameplayAttributeData PickupRadius;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, PickupRadius)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MeatDropMultiplier, Category = "AR|Player|Rewards")
	FGameplayAttributeData MeatDropMultiplier;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, MeatDropMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ScrapDropMultiplier, Category = "AR|Player|Rewards")
	FGameplayAttributeData ScrapDropMultiplier;
	AR_PLAYER_ATTRIBUTE_ACCESSORS(UARAttributeSetPlayer, ScrapDropMultiplier)

protected:
	UFUNCTION() void OnRep_HealingDealtMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RepairRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DodgeDistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DodgeDuration(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JumpDistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JetpackFuel(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxJetpackFuel(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JetpackFuelRegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JetpackFuelDrainRate(const FGameplayAttributeData& OldValue);
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
	UFUNCTION() void OnRep_MeatDropMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ScrapDropMultiplier(const FGameplayAttributeData& OldValue);

private:
	static float Clamp01(float Value) { return FMath::Clamp(Value, 0.0f, 1.0f); }
	static float ClampNonNegative(float Value) { return FMath::Max(0.0f, Value); }
};

#undef AR_PLAYER_ATTRIBUTE_ACCESSORS
