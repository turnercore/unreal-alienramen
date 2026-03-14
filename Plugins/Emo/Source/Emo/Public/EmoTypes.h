/**
 * @file EmoTypes.h
 * @brief Shared emotion display payloads and resolver row types.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "EmoTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct EMO_API FEmoIconRow : public FTableRowBase
{
	GENERATED_BODY()

	// Authoritative emotion key mapped to the icon for this row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue", ToolTip = "Emotion tag key used to resolve icon content."))
	FGameplayTag EmotionTag;

	// Texture shown when this row resolves for a requested emotion tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed emotion data field used by runtime display logic."))
	TSoftObjectPtr<UTexture2D> IconTexture;
};

USTRUCT(BlueprintType)
struct EMO_API FEmoDisplayState
{
	GENERATED_BODY()

	// Shared tag used when no per-slot override exists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed emotion data field used by runtime display logic."))
	FGameplayTag SharedEmotionTag;

	// Player-slot override for P1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed emotion data field used by runtime display logic."))
	FGameplayTag P1EmotionTag;

	// Player-slot override for P2.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed emotion data field used by runtime display logic."))
	FGameplayTag P2EmotionTag;
};
