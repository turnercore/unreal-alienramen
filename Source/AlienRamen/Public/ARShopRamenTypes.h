/**
 * @file ARShopRamenTypes.h
 * @brief Shared shop ramen/customer/station runtime and authoring types.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ARColorTypes.h"
#include "ARShopRamenTypes.generated.h"

UENUM(BlueprintType)
enum class EARRamenTasteReaction : uint8
{
	Hate = 0,
	Ok,
	Like,
	Love
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
struct ALIENRAMEN_API FARRamenBowlSpec
{
	GENERATED_BODY()

	/** Noodle color currently in the bowl (None means empty slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor NoodlesColor = EARAffinityColor::None;

	/** Broth color currently in the bowl (None means empty slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor BrothColor = EARAffinityColor::None;

	/** Toppings color currently in the bowl (None means empty slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor ToppingsColor = EARAffinityColor::None;
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
struct ALIENRAMEN_API FARShopStationConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Station type this config row applies to; drives which slot it feeds in bowls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARRamenStationType StationType = EARRamenStationType::Noodles;

	/** Upgrade tags that must be unlocked for this station to run as upgraded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer RequiredUpgradeTags;

	/** Max processed servings the station buffers when using this config row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxStock = 5;

	/** Base processing time per serving when using this config row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float ProcessingDurationSeconds = 1.5f;

	/** Input mode (Hold or Tap) required to process servings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARRamenStationProcessingInputMode ProcessingInputMode = EARRamenStationProcessingInputMode::Hold;

	/** Progress contribution per tap when in Tap mode; scale relative to ProcessingDurationSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TapProcessingSecondsPerPress = 0.20f;
};

