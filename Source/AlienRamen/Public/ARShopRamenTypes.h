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

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenBowlSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor NoodlesColor = EARAffinityColor::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor BrothColor = EARAffinityColor::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARAffinityColor ToppingsColor = EARAffinityColor::None;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenOrderRequest
{
	GENERATED_BODY()

	// Authoring/runtime list of requested colors. Runtime normalizes this to max 3 entries and
	// treats omitted slots as None for strict-composition checks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<EARAffinityColor> RequestedColors;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenServeResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARRamenTasteReaction Reaction = EARRamenTasteReaction::Hate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 MatchedColorCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bExactCompositionMatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bUsedPickyRule = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 RelationshipDeltaPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag AppliedReactionEmotionTag;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARRamenOrderOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARRamenOrderRequest Order;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "1", UIMin = "1"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 MinRelationshipLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 MaxRelationshipLevel = 999;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCustomerDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bPickyExactMatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<FARRamenOrderOption> OrderOptions;

	// Optional per-customer overrides. If unset, system settings defaults are used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag HateEmotionTagOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag OkEmotionTagOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag LikeEmotionTagOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag LoveEmotionTagOverride;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopStationConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EARRamenStationType StationType = EARRamenStationType::Noodles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer RequiredUpgradeTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxStock = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float ProcessingDurationSeconds = 1.5f;
};
