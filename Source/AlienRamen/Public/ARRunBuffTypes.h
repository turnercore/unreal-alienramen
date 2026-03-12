/**
 * @file ARRunBuffTypes.h
 * @brief Shared save/runtime structs for run buff inventory + active payload.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARRunBuffTypes.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRunBuffItemStack
{
	GENERATED_BODY()

	// Optional owning character key. Invalid tag means shared/global stack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	FGameplayTag CharacterTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff", meta = (ClampMin = "0", UIMin = "0"))
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRunBuffActivePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	FGameplayTag CharacterTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff", meta = (ClampMin = "1", UIMin = "1"))
	int32 AppliedCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	TArray<TSubclassOf<UGameplayEffect>> GameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	FGameplayTagContainer GrantedTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRunBuffStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	TArray<FARRunBuffItemStack> StoredEnergyDrinkStacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	TArray<FARRunBuffItemStack> QueuedEnergyDrinkStacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	TArray<FARRunBuffActivePayload> ActiveRunBuffPayloads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run Buff")
	int32 ActiveRunBuffCycleId = 0;
};
