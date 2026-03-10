/**
 * @file AREmotionTypes.h
 * @brief Shared emotion display payloads and resolver row types.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "AREmotionTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FAREmotionIconRow : public FTableRowBase
{
	GENERATED_BODY()

	// Authoritative emotion key mapped to the icon for this row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue"))
	FGameplayTag EmotionTag;

	// Texture shown when this row resolves for a requested emotion tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TSoftObjectPtr<UTexture2D> IconTexture;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FAREmotionDisplayState
{
	GENERATED_BODY()

	// Shared tag used when no per-slot override exists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag SharedEmotionTag;

	// Player-slot override for P1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag P1EmotionTag;

	// Player-slot override for P2.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag P2EmotionTag;
};
