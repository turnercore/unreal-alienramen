/**
 * @file ARShopRamenTypes.h
 * @brief Shared shop ramen/customer/station runtime and authoring types.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARColorTypes.h"
#include "ARShopRamenTypes.generated.h"

class AARInvaderDropBase;

UENUM(BlueprintType)
enum class EARRamenTasteReaction : uint8
{
	Hate = 0,
	Ok,
	Like,
	Love
};

UENUM(BlueprintType)
enum class EARVendingQualityTier : uint8
{
	Low = 0,
	Standard,
	High,
	Premium
};

UENUM(BlueprintType)
enum class EARRamenStationType : uint8
{
	Noodles = 0,
	Broth,
	Toppings
};

UENUM(BlueprintType)
enum class EARRamenStationRuntimeState : uint8
{
	Idle = 0,
	MeatReady,
	Processing,
	Processed
};

UENUM(BlueprintType)
enum class EARRamenStationProcessingInputMode : uint8
{
	Hold = 0,
	Tap
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenBowlSlotSpec
{
	GENERATED_BODY()

	FARRamenBowlSlotSpec() = default;
	explicit FARRamenBowlSlotSpec(const EARRamenStationType InSlotType)
		: SlotType(InSlotType)
	{
	}

	/** Canonical bowl slot this payload belongs to (noodles, broth, toppings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARRamenStationType SlotType = EARRamenStationType::Noodles;

	/** Color currently authored in this bowl slot (None means empty slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor Color = EARAffinityColor::None;

	/** Meat item tag applied to this slot (invalid when no meat identity was applied). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Item.Meat"))
	FGameplayTag MeatTag;

	/** Quality tier applied to this slot when processed/filled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARVendingQualityTier QualityTier = EARVendingQualityTier::Standard;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenBowlSpec
{
	GENERATED_BODY()

	/** Noodle slot payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARRamenBowlSlotSpec Noodles = FARRamenBowlSlotSpec(EARRamenStationType::Noodles);

	/** Broth slot payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARRamenBowlSlotSpec Broth = FARRamenBowlSlotSpec(EARRamenStationType::Broth);

	/** Toppings slot payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARRamenBowlSlotSpec Toppings = FARRamenBowlSlotSpec(EARRamenStationType::Toppings);

	FARRamenBowlSlotSpec& GetSlot(const EARRamenStationType SlotType)
	{
		switch (SlotType)
		{
		case EARRamenStationType::Noodles:
			return Noodles;
		case EARRamenStationType::Broth:
			return Broth;
		default:
			return Toppings;
		}
	}

	const FARRamenBowlSlotSpec& GetSlot(const EARRamenStationType SlotType) const
	{
		switch (SlotType)
		{
		case EARRamenStationType::Noodles:
			return Noodles;
		case EARRamenStationType::Broth:
			return Broth;
		default:
			return Toppings;
		}
	}
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARMeatDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Canonical meat identity gameplay tag used by shop inventory/runtime flows and shared item-definition lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Item.Meat"))
	FGameplayTag MeatTag;

	/** Optional enemy identifier tag that maps invader drops to this meat definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Enemy.Identifier"))
	FGameplayTag EnemyIdentifierTag;

	/** Optional invader pickup actor class override for this meat when spawned as an invader drop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Optional invader-mode pickup actor class override for this meat definition. Must derive from ARInvaderDropBase."))
	TSoftClassPtr<AARInvaderDropBase> InvaderDropActorClass;

	/** Display name used for UI and diagnostics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FText Name;

	/** Description used for UI and diagnostics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FText Description;

	/** Number of bowl fill units one item contributes when loaded into a station. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "1", UIMin = "1"))
	int32 StationFillAmount = 1;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenOrderRequest
{
	GENERATED_BODY()

	// Authoring/runtime list of requested colors. Runtime normalizes this to max 3 entries and
	// treats omitted slots as None for strict-composition checks.
	// Example: [Red, Blue, None] = noodles red, broth blue, toppings free choice.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<EARAffinityColor> RequestedColors;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenServeResult
{
	GENERATED_BODY()

	/** Reaction bucket used for scoring/UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARRamenTasteReaction Reaction = EARRamenTasteReaction::Hate;

	/** Number of matching colors (0-3) between served bowl and requested order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 MatchedColorCount = 0;

	/** True when all three slots match (strict success). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bExactCompositionMatch = false;

	/** True when picky rule was applied (used when exact composition was required). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bUsedPickyRule = false;

	/** Relationship delta applied for this serve result. Designers can tune downstream uses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 RelationshipDeltaPoints = 0;

	/** Emotion tag pushed to dialogue/emotion systems for this reaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag AppliedReactionEmotionTag;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenOrderOption
{
	GENERATED_BODY()

	/** Requested bowl composition for this option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARRamenOrderRequest Order;

	/** Weighted selection chance relative to sibling options. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "1", UIMin = "1"))
	int32 Weight = 1;

	/** Minimum relationship level required for this order to be considered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 MinRelationshipLevel = 0;

	/** Maximum relationship level that still allows this order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 MaxRelationshipLevel = 999;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCustomerDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** When true, customers only accept exact composition matches (ignores partial credit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bPickyExactMatch = false;

	/** List of possible orders for this customer; weighted/randomly selected at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<FARRamenOrderOption> OrderOptions;

	// Optional per-customer overrides. If unset, system settings defaults are used.
	/** Override emotion tag for the Hate reaction (Parley.Emotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag HateEmotionTagOverride;

	/** Override emotion tag for the Ok reaction (Parley.Emotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag OkEmotionTagOverride;

	/** Override emotion tag for the Like reaction (Parley.Emotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag LikeEmotionTagOverride;

	/** Override emotion tag for the Love reaction (Parley.Emotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag LoveEmotionTagOverride;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopReactionTipRange
{
	GENERATED_BODY()

	/** Inclusive minimum multiplier used when sampling tip payout for this reaction bucket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinMultiplier = 0.0f;

	/** Inclusive maximum multiplier used when sampling tip payout for this reaction bucket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxMultiplier = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARVendingStockedBowlEntry
{
	GENERATED_BODY()

	/** Vending machine quality tier used when calculating post-run sale payout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARVendingQualityTier QualityTier = EARVendingQualityTier::Standard;

	/** Completed bowl payload to monetize on next shop-entry settlement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARRamenBowlSpec BowlSpec;
};
