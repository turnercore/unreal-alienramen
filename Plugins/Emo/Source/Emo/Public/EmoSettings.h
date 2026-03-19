/**
 * @file EmoSettings.h
 * @brief Emotion runtime/content settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "EmoSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Emotion"))
class EMO_API UEmoSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Emo"); }
	virtual FName GetSectionName() const override { return TEXT("Emotion"); }

	// Preferred TagKey route root used to resolve the emotion icon DataTable.
	// Example: Emotion (row names should match emotion tags under the selected root).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content", meta = (ToolTip = "Emotion content root tags used by resolver lookup and fallback."))
	FGameplayTag EmotionResolverRootTag;

	// Generic emotion root used for speaker-specific fallback.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Content", meta = (ToolTip = "Emotion content root tags used by resolver lookup and fallback."))
	FGameplayTag GenericEmotionRootTag;

	// Enables extra resolver diagnostic logging for source selection and per-tag lookup outcomes.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Diagnostics", meta = (ToolTip = "Enables optional verbose diagnostics for emotion systems."))
	bool bEnableVerboseResolverLogs = false;

	// Enables extra HUD projection diagnostic logging for emotion icon render eligibility.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Diagnostics", meta = (ToolTip = "Enables optional verbose diagnostics for emotion systems."))
	bool bEnableVerboseRenderLogs = false;

	// Optional default offset used by emotion components when they do not override AnchorWorldOffset.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Presentation", meta = (ToolTip = "Default presentation values used by emotion display components."))
	FVector DefaultAnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);

	// Emotion tag applied by speaker runtime when a speaker is currently talkable/offerable.
	// Used by NPCs when they have an active offer; sets overhead icon if available.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (ToolTip = "Emotion tags used by runtime behavior policies."))
	FGameplayTag WantsToTalkEmotionTag;

	// Emotion tag applied by dialogue runtime when one-talker-per-speaker mode marks a speaker as busy.
	// Shown while another player is talking to this speaker.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (ToolTip = "Emotion tags used by runtime behavior policies."))
	FGameplayTag BusyEmotionTag;

	// Priority used when dialogue runtime applies BusyEmotionTag through a generic emotion registration source.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (ToolTip = "Numeric runtime tuning values for emotion behavior."))
	int32 BusyEmotionPriority = 3;

	// Default duration used by timed emotion registration helpers when duration input is <= 0.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Emotion|Behavior", meta = (ClampMin = "0.01", UIMin = "0.01", ToolTip = "Default duration used when timed emotion registrations omit a positive duration."))
	float DefaultTimedSystemOverrideDurationSeconds = 1.5f;
};
