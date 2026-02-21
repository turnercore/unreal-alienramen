#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ARAbilitySet.generated.h"

class UGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FARAbilitySet_AbilityEntry
{
	GENERATED_BODY()

	// Ability class to grant.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability;

	// Tag the input/activation will use to activate this ability.
	// This should match a tag on the ability (AbilityTags) OR we will add it as a dynamic tag on the spec.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag ActivationTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Level = 1;
};

USTRUCT(BlueprintType)
struct FARAbilitySet_EffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Level = 1.0f;
};

UCLASS(BlueprintType)
class ALIENRAMEN_API UARAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FARAbilitySet_AbilityEntry> Abilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FARAbilitySet_EffectEntry> StartupEffects;
};