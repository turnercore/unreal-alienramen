/**
 * @file ARInvaderSpicyTrackSettings.h
 * @brief Project settings for Invader spicy-track runtime tuning.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARInvaderSpicyTrackSettings.generated.h"

class UDataTable;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Invader Spicy Track"))
class ALIENRAMEN_API UARInvaderSpicyTrackSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UARInvaderSpicyTrackSettings();

	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// Time since last credited kill before combo auto-resets.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0.0"))
	float ComboTimeoutSeconds = 5.0f;

	// Spice required per tier step (for example 100).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "1"))
	int32 SpicePerTier = 100;

	// Full-blast tier cap (v1 default: 5 => max spice 500 with SpicePerTier=100).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "1"))
	int32 MaxFullBlastTier = 5;

	// Number of full-blast offers to generate each activation.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Offer", meta = (ClampMin = "1"))
	int32 FullBlastOfferCount = 3;

	// Per-tier skip reward in scrap. Index 0 = tier 1.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Offer")
	TArray<int32> SkipScrapRewardByTier;

	// Level roll weights for +/- offsets around base offer level.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Offer")
	TArray<FARInvaderLevelOffsetWeight> LevelOffsetWeights;

	// Upgrade definitions used for offer generation and activation behavior.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> UpgradeDataTable;

	// Enemy definition fallback value when a per-enemy row value is unavailable.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data", meta = (ClampMin = "0.0"))
	float DefaultBaseKillSpiceValue = 10.0f;

	// Actor tag name to identify enemy projectiles for full-blast clear.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Effects")
	FGameplayTag EnemyProjectileActorTag;

	// Gameplay cue tag fired on full-blast resolve.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Effects")
	FGameplayTag FullBlastGameplayCueTag;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetSkipScrapRewardForTier(int32 Tier) const;
};

