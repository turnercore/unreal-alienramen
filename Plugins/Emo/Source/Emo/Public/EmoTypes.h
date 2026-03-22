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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Emotion tag key used to resolve icon content."))
	FGameplayTag EmotionTag;

	// Texture shown when this row resolves for a requested emotion tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed emotion data field used by runtime display logic."))
	TSoftObjectPtr<UTexture2D> IconTexture;
};

USTRUCT(BlueprintType)
struct EMO_API FEmoEmotionRegistration
{
	GENERATED_BODY()

	/** Logical writer id used to update or clear this registration deterministically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Logical source id that owns this registration entry. Reusing the same source id and target viewer tags updates the existing entry."))
	FName SourceId = NAME_None;

	/** Emotion tag displayed when this registration wins viewer resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Emotion tag displayed when this registration wins viewer resolution."))
	FGameplayTag EmotionTag;

	/** Higher values win before tie-break rules apply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Priority used when multiple matching or global registrations compete. Higher values win first."))
	int32 Priority = 0;

	/**
	 * Exact-match viewer tags for this registration.
	 * Empty means global fallback visible to any HUD when no higher-priority targeted registration wins.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Viewer tags that must match exactly against the HUD viewer container. Empty means this registration is global."))
	FGameplayTagContainer TargetViewerTags;

	/** Monotonic write order used to break same-priority ties deterministically. */
	UPROPERTY()
	uint64 WriteSerial = 0;
};
