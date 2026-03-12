/**
 * @file ARScrapyardTypes.h
 * @brief Shared scrapyard authoring/runtime structs.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ARScrapyardTypes.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EARScrapyardRewardType : uint8
{
	None = 0,
	LicenseUnlock,
	EnergyDrink
};

UENUM(BlueprintType)
enum class EARScrapyardStackRule : uint8
{
	Unique = 0,
	Stackable
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardItemDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard", meta = (ClampMin = "0", UIMin = "0"))
	int32 ScrapCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardRewardType RewardType = EARScrapyardRewardType::None;

	// Reward type = LicenseUnlock.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag LicenseUnlockTag;

	// Reward type = EnergyDrink. Defaults to ItemTag when unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag EnergyDrinkTag;

	// Run-buff payload used when energy drink is activated for a run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	TArray<TSubclassOf<UGameplayEffect>> RunBuffGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTagContainer RunBuffGrantedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard", meta = (ClampMin = "0", UIMin = "0"))
	int32 SellMoneyValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardStackRule StackRule = EARScrapyardStackRule::Stackable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxStackCount = 99;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardRewardGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardRewardType RewardType = EARScrapyardRewardType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag LicenseUnlockTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag EnergyDrinkTag;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardExtractionSummary
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 CurrentScrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 ReservedCostTotal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 ReservedItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	float RemainingTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	bool bRunActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 KeptItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 TrimmedItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	TArray<FARScrapyardRewardGrant> GrantedRewards;
};
