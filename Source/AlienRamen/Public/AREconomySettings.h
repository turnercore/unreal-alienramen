/**
 * @file AREconomySettings.h
 * @brief Shared economy tuning for Invader/Scrapyard/Shop transitions.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AREconomySettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Economy"))
class ALIENRAMEN_API UAREconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// Percent of run-ledger value lost when Invader ends due to team-down loss.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Invader", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InvaderDeathPenaltyPercent = 0.0f;

	// Runtime budget cap applied when depositing leftover scrapyard scrap into shop storage.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxScrapStorage = 0;

	// Runtime storage cap applied when depositing run-ledger meat into shop storage.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxMeatStorage = 0;

	// Default scrapyard timer used when mode setup does not explicitly override.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Scrapyard", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float DefaultScrapyardDurationSeconds = 180.0f;

	// Salt used for deterministic scrapyard spawn RNG stream generation.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Scrapyard")
	int32 ScrapyardSpawnSeedSalt = 1337;
};

