#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AREnemyAttributeSet.generated.h"

#define AR_ENEMY_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class ALIENRAMEN_API UAREnemyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAREnemyAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CollisionDamage, Category = "AR|Enemy|Combat")
	FGameplayAttributeData CollisionDamage;
	AR_ENEMY_ATTRIBUTE_ACCESSORS(UAREnemyAttributeSet, CollisionDamage)

protected:
	UFUNCTION()
	void OnRep_CollisionDamage(const FGameplayAttributeData& OldValue);
};

#undef AR_ENEMY_ATTRIBUTE_ACCESSORS
