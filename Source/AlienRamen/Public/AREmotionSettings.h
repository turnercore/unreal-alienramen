/**
 * @file AREmotionSettings.h
 * @brief Emotion runtime/content settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "AREmotionSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Emotion"))
class ALIENRAMEN_API UAREmotionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen|NPC"); }
	virtual FName GetSectionName() const override { return TEXT("Emotion"); }

	// Fallback roots used when resolving speaker-scoped emotion tags (for example Dialogue.Speaker.Fred.Angry -> Dialogue.Emotion.Angry).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content", meta = (Categories = "Dialogue"))
	FGameplayTagContainer FallbackEmotionRootTags;

	// Optional default socket used by emotion components when they do not override AnchorSocketName.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Presentation")
	FName DefaultAnchorSocketName = NAME_None;

	// Optional default offset used by emotion components when they do not override AnchorWorldOffset.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Presentation")
	FVector DefaultAnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);
};

