/**
 * @file AREmotionSettings.h
 * @brief Emotion runtime/content settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "AREmotionSettings.generated.h"

class UDataTable;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Emotion"))
class ALIENRAMEN_API UAREmotionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen|NPC"); }
	virtual FName GetSectionName() const override { return TEXT("Emotion"); }

	// Authoritative DataTable source for emotion tag->icon mappings.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content")
	TSoftObjectPtr<UDataTable> EmotionDataTable;

	// Generic emotion root used for speaker-specific fallback (for example Dialogue.Speaker.Fred.Angry -> Dialogue.Emotion.Angry).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content", meta = (Categories = "Dialogue"))
	FGameplayTag GenericEmotionRootTag;

	// Optional default socket used by emotion components when they do not override AnchorSocketName.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Presentation")
	FName DefaultAnchorSocketName = NAME_None;

	// Optional default offset used by emotion components when they do not override AnchorWorldOffset.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Presentation")
	FVector DefaultAnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);
};
