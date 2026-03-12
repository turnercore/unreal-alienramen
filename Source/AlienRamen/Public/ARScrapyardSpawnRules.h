/**
 * @file ARScrapyardSpawnRules.h
 * @brief Shared structs/data asset for managed scrapyard spawner orchestration.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ARScrapyardTypes.h"
#include "ARScrapyardSpawnRules.generated.h"

/** Budget constraints for a single rarity tier. */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardRarityBudget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxCount = 0;
};

/** Aggregate spawn orchestration rules consumed by Scrapyard GameMode. */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardSpawnRules
{
	GENERATED_BODY()

	// Total spawn bounds applied after always-spawn entries.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinTotalSpawns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxTotalSpawns = 20;

	// Perlin noise frequency in world units; larger values spread the field.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float NoiseScale = 600.0f;

	// Minimum normalized noise value required for a spawner to be considered (0..1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseThreshold = 0.2f;

	// Random jitter added to noise per spawner to avoid visible grids.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseJitter = 0.1f;

	// Per-rarity budgets; missing keys fall back to zeroed budget.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns")
	TMap<EARScrapyardItemRarity, FARScrapyardRarityBudget> RarityBudgets;

	// Helper to read rarity budget with a safe default.
	FARScrapyardRarityBudget GetBudgetForRarity(EARScrapyardItemRarity Rarity) const;
};

/** Data asset designers author per map to tune scrapyard spawning. */
UCLASS(BlueprintType)
class ALIENRAMEN_API UARScrapyardSpawnRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UARScrapyardSpawnRuleSet();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scrapyard|Spawns")
	FARScrapyardSpawnRules Rules;
};

