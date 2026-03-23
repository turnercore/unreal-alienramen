/**
 * @file ARAttributeSetCore.h
 * @brief Shared cross-domain GAS attributes (player + enemy) for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ARAttributeSetCore.generated.h"

#define AR_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Core/shared attributes used across combat actors.
 *
 * Contract:
 * - Contains only broadly shared attributes.
 * - Player-only and enemy-only attributes live in dedicated sets.
 */
UCLASS()
class ALIENRAMEN_API UARAttributeSetCore : public UAttributeSet
{
	GENERATED_BODY()

public:
	UARAttributeSetCore();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -----------------------
	// Survivability
	// -----------------------
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "AR|Core|Survivability")
	FGameplayAttributeData Health;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Health)

	// Meta attribute consumed into shield/health by PostGameplayEffectExecute.
	UPROPERTY(BlueprintReadOnly, Category = "AR|Core|Survivability|Meta")
	FGameplayAttributeData IncomingDamage;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "AR|Core|Survivability")
	FGameplayAttributeData MaxHealth;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "AR|Core|Survivability")
	FGameplayAttributeData Shield;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Shield)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "AR|Core|Survivability")
	FGameplayAttributeData MaxShield;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MaxShield)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegenRate, Category = "AR|Core|Survivability")
	FGameplayAttributeData HealthRegenRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealthRegenRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegenDelay, Category = "AR|Core|Survivability")
	FGameplayAttributeData HealthRegenDelay;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealthRegenDelay)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldRegenRate, Category = "AR|Core|Survivability")
	FGameplayAttributeData ShieldRegenRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ShieldRegenRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldRegenDelay, Category = "AR|Core|Survivability")
	FGameplayAttributeData ShieldRegenDelay;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, ShieldRegenDelay)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageTakenMultiplier, Category = "AR|Core|Survivability")
	FGameplayAttributeData DamageTakenMultiplier;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, DamageTakenMultiplier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingReceivedMultiplier, Category = "AR|Core|Survivability")
	FGameplayAttributeData HealingReceivedMultiplier;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, HealingReceivedMultiplier)

	// -----------------------
	// Movement
	// -----------------------
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "AR|Core|Movement")
	FGameplayAttributeData MoveSpeed;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, MoveSpeed)

	// -----------------------
	// Combat
	// -----------------------
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Damage, Category = "AR|Core|Combat")
	FGameplayAttributeData Damage;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, Damage)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireRate, Category = "AR|Core|Combat")
	FGameplayAttributeData FireRate;
	AR_ATTRIBUTE_ACCESSORS(UARAttributeSetCore, FireRate)

protected:
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
	UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Damage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FireRate(const FGameplayAttributeData& OldValue);

private:
	static float Clamp01(float Value) { return FMath::Clamp(Value, 0.0f, 1.0f); }
	static float ClampNonNegative(float Value) { return FMath::Max(0.0f, Value); }
};

#undef AR_ATTRIBUTE_ACCESSORS
