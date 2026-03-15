/**
 * @file ARDialogueAudioSettings.h
 * @brief Dialogue-audio bridge settings and FMOD cue mapping row types.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARDialogueAudioSettings.generated.h"

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueAudioCueFMODRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.AudioCue", DisplayName = "Audio Cue Tag", ToolTip = "Dialogue audio cue tag emitted by Parley signal-mode requests."))
	FGameplayTag AudioCueTag;

	// Kept as UObject soft reference so this module has no hard compile-time dependency on FMOD classes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "FMOD Event", AllowedClasses = "/Script/FMODStudio.FMODEvent", ToolTip = "Optional FMOD event asset played for this dialogue audio cue in local 2D signal flow."))
	TSoftObjectPtr<UObject> FMODEventAsset;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Dialogue Audio"))
class ALIENRAMEN_API UARDialogueAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Dialogue Audio"); }

	// Optional DataTable of FARDialogueAudioCueFMODRow entries used by local FMOD bridge playback.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Audio", meta = (ToolTip = "Data table mapping Parley dialogue audio cue tags to FMOD events for local 2D playback."))
	TSoftObjectPtr<UDataTable> DialogueAudioCueFMODTable;

	// Prevent duplicate local playback for the same delivered dialogue line across multiple local controllers.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Audio", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "If > 0, suppresses duplicate local playback when the same session+line request arrives again within this window."))
	float LocalLineDedupeWindowSeconds = 0.5f;

	// Retention window for dedupe keys to prevent unbounded map growth.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Audio", meta = (ClampMin = "1.0", UIMin = "1.0", ToolTip = "How long to keep delivered-line dedupe keys before pruning."))
	float LocalLineDedupeRetentionSeconds = 6.0f;
};
