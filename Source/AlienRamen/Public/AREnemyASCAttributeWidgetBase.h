/**
 * @file AREnemyASCAttributeWidgetBase.h
 * @brief Enemy-focused ASC widget bridge with explicit core + enemy attribute events.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARASCAttributeWidgetBase.h"
#include "ARPlayerTypes.h"
#include "AREnemyASCAttributeWidgetBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnEnemyASCWidgetCoreAttributeChangedSignature,
	EARCoreAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnEnemyASCWidgetCollisionDamageChangedSignature,
	float,
	NewValue,
	float,
	OldValue);

/**
 * Enemy-specialized ASC widget bridge.
 *
 * Tracks:
 * - Core attributes: Health, MaxHealth, Spice, MaxSpice, MoveSpeed, Strength
 * - Enemy attribute: CollisionDamage
 */
UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UAREnemyASCAttributeWidgetBase : public UARASCAttributeWidgetBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes|Enemy")
	FAROnEnemyASCWidgetCoreAttributeChangedSignature OnEnemyASCWidgetCoreAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|UI|ASC Attributes|Enemy")
	FAROnEnemyASCWidgetCollisionDamageChangedSignature OnEnemyASCWidgetCollisionDamageChanged;

protected:
	virtual void BuildTrackedAttributeDefinitions(TArray<FARASCTrackedAttributeDefinition>& OutDefinitions) const override;

	virtual void HandleTrackedAttributeValueChanged(
		const FARASCTrackedAttributeRuntime& RuntimeState,
		float NewValue,
		float OldValue) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes|Enemy")
	void BP_OnEnemyASCWidgetCoreAttributeChanged(EARCoreAttributeType AttributeType, float NewValue, float OldValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|ASC Attributes|Enemy")
	void BP_OnEnemyASCWidgetCollisionDamageChanged(float NewValue, float OldValue);

private:
};
