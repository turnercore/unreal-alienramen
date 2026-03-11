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
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Emotion"); }

	// Preferred TagContentResolver route root used to resolve the emotion icon DataTable.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content", meta = (Categories = "Dialogue"))
	FGameplayTag EmotionResolverRootTag;

	// Generic emotion root used for speaker-specific fallback (for example Dialogue.Speaker.Fred.Angry -> Dialogue.Emotion.Angry).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content", meta = (Categories = "Dialogue"))
	FGameplayTag GenericEmotionRootTag;

	// Enables extra resolver diagnostic logging for source selection and per-tag lookup outcomes.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Diagnostics")
	bool bEnableVerboseResolverLogs = false;

	// Enables extra HUD projection diagnostic logging for emotion icon render eligibility.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Diagnostics")
	bool bEnableVerboseRenderLogs = false;

	// Optional default offset used by emotion components when they do not override AnchorWorldOffset.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Presentation")
	FVector DefaultAnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);

	// Emotion tag applied by speaker runtime when a speaker is currently talkable/offerable.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (Categories = "Dialogue.Emotion"))
	FGameplayTag WantsToTalkEmotionTag;

	// Emotion tag applied by dialogue runtime when one-talker-per-speaker mode marks a speaker as busy.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (Categories = "Dialogue.Emotion"))
	FGameplayTag BusyEmotionTag;

	// Priority used when dialogue runtime applies BusyEmotionTag through generic system override source.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior")
	int32 BusyEmotionPriority = 3;

	// Default duration used by timed system-emotion override helpers when duration input is <= 0.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float DefaultTimedSystemOverrideDurationSeconds = 1.5f;
};
