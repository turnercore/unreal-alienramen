/**
 * @file ARScrapyardTypes.h
 * @brief Shared item authoring/runtime structs used by Scrapyard + Shop.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARScrapyardTypes.generated.h"

class AActor;
class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class EARScrapyardRewardType : uint8
{
	None = 0,        // no reward (invalid/default)
	LicenseUnlock,   // grants a license unlock tag
	EnergyDrink,     // grants an energy drink item tag
	ProgressionTag,  // grants a progression tag
	UnlockTag        // grants a generic unlock tag (shared with save unlocks)
};

UENUM(BlueprintType)
enum class EARScrapyardItemType : uint8
{
	Garbage = 0,   // flavor-only scrapable items
	EnergyDrink,   // energy drink consumables
	License,       // unlock tokens/licenses
	Other          // everything else (use RewardType for behavior)
};

UENUM(BlueprintType)
enum class EARScrapyardItemRarity : uint8
{
	Common = 0,
	Uncommon,
	Rare,
	Epic,
	Legendary
};

UENUM(BlueprintType)
enum class EARScrapyardStackRule : uint8
{
	Unique = 0,  // only one instance allowed per inventory slot
	Stackable    // can stack up to MaxStackCount
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardItemDefRow : public FTableRowBase
{
	GENERATED_BODY()

	// Canonical shared item row consumed by both Scrapyard and Shop systems.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard", meta = (ClampMin = "0", UIMin = "0"))
	int32 ScrapCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardItemType ItemType = EARScrapyardItemType::Other;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardItemRarity Rarity = EARScrapyardItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FText AltDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FText AltDescription;

	// Character/loadout/progression knowledge requirements for primary text visibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTagContainer RequiredCharacterKnowledgeTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTagContainer RequiredKnowledgeTags;

	// Spawn-gating requirements (for example progression/loadout prerequisites).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTagContainer SpawnConditionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardRewardType RewardType = EARScrapyardRewardType::None;

	// Reward type = LicenseUnlock/UnlockTag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag LicenseUnlockTag;

	// Reward type = EnergyDrink. Defaults to ItemTag when unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag EnergyDrinkTag;

	// Reward type = ProgressionTag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag ProgressionRewardTag;

	// Legacy inline run-buff payload (prefer FAREnergyDrinkDefRow via Scrapyard.EnergyDrink route).
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	TSoftClassPtr<AActor> ItemModelClass;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FAREnergyDrinkDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard|Energy Drink")
	FGameplayTag EnergyDrinkTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard|Energy Drink")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard|Energy Drink")
	TArray<TSubclassOf<UGameplayEffect>> RunBuffGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard|Energy Drink")
	FGameplayTagContainer RunBuffGrantedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard|Energy Drink")
	EARScrapyardStackRule StackRule = EARScrapyardStackRule::Unique;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard|Energy Drink", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxStackCount = 1;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardRewardGrant
{
	GENERATED_BODY()

	/** Item tag associated with the grant (mirrors item def tag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag ItemTag;

	/** Reward category that determines which field below is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	EARScrapyardRewardType RewardType = EARScrapyardRewardType::None;

	/** License unlock to apply when RewardType = LicenseUnlock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag LicenseUnlockTag;

	/** Energy drink tag granted when RewardType = EnergyDrink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag EnergyDrinkTag;

	/** Progression tag granted when RewardType = ProgressionTag or UnlockTag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	FGameplayTag ProgressionRewardTag;
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
	int32 LeftoverScrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 TrimmedScrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 WastedScrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 PurchasedItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 DiscardedItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	int32 ConvertedMoney = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scrapyard")
	TArray<FARScrapyardRewardGrant> GrantedRewards;
};
