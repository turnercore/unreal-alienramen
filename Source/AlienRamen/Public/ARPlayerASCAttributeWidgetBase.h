/**
 * @file ARPlayerASCAttributeWidgetBase.h
 * @brief Player-focused ASC widget bridge with explicit core/primary/hat events.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARASCAttributeWidgetBase.h"
#include "ARASCAttributeWidgetTypes.h"
#include "ARPlayerTypes.h"
#include "ARPlayerASCAttributeWidgetBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnPlayerASCWidgetCoreAttributeChangedSignature,
	EARCoreAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnPlayerASCWidgetPrimaryAttributeChangedSignature,
	EARPlayerPrimaryCombatAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnPlayerASCWidgetHatAttributeChangedSignature,
	EARPlayerHatAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

/**
 * Player-specialized ASC widget bridge.
 *
 * Tracks:
 * - Core attributes: Health, MaxHealth, Spice, MaxSpice, MoveSpeed, Strength
 * - Primary combat lane: Damage, FireRate, Ammo, MaxAmmo
 * - Hat lane: HatEnergy, MaxHatEnergy, HatEnergyRegenRate, HatPower
 */
UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARPlayerASCAttributeWidgetBase : public UARASCAttributeWidgetBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes|Player")
	FAROnPlayerASCWidgetCoreAttributeChangedSignature OnPlayerASCWidgetCoreAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes|Player")
	FAROnPlayerASCWidgetPrimaryAttributeChangedSignature OnPlayerASCWidgetPrimaryAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes|Player")
	FAROnPlayerASCWidgetHatAttributeChangedSignature OnPlayerASCWidgetHatAttributeChanged;

protected:
	virtual void BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const override;

	virtual void HandleTrackedAttributeValueChanged(
		const FARASCTrackedAttributeRuntime& RuntimeState,
		float NewValue,
		float OldValue) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes|Player")
	void BP_OnPlayerASCWidgetCoreAttributeChanged(EARCoreAttributeType AttributeType, float NewValue, float OldValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes|Player")
	void BP_OnPlayerASCWidgetPrimaryAttributeChanged(EARPlayerPrimaryCombatAttributeType AttributeType, float NewValue, float OldValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes|Player")
	void BP_OnPlayerASCWidgetHatAttributeChanged(EARPlayerHatAttributeType AttributeType, float NewValue, float OldValue);

private:
	static bool TryResolveCoreAttributeType(const FGameplayAttribute& Attribute, EARCoreAttributeType& OutType);
	static bool TryResolvePrimaryAttributeType(const FGameplayAttribute& Attribute, EARPlayerPrimaryCombatAttributeType& OutType);
	static bool TryResolveHatAttributeType(const FGameplayAttribute& Attribute, EARPlayerHatAttributeType& OutType);
};
