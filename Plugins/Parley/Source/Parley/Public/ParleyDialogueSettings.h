/**
 * @file ParleyDialogueSettings.h
 * @brief Dialogue runtime/content settings for Parley.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ParleyDialogueSettings.generated.h"

UCLASS(Config=Parley, DefaultConfig, meta=(DisplayName="Dialogue"))
class PARLEY_API UParleyDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Dialogue"); }

	// TagContentResolver root tag used to resolve FParleySpeakerRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content", meta = (ToolTip = "TagContentResolver root tag used to discover dialogue content assets."))
	FGameplayTag SpeakerDefinitionRootTag;

	// TagContentResolver root tag used to resolve FParleyConversationAssetRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content", meta = (ToolTip = "TagContentResolver root tag used to discover dialogue content assets."))
	FGameplayTag ConversationDefinitionRootTag;

	// Modes that use one shared session for all slotted players.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Mode", meta = (ToolTip = "Mode tag set used to configure dialogue runtime behavior."))
	FGameplayTagContainer SharedDialogueModeTags;

	// Modes that use one session per active player.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Mode", meta = (ToolTip = "Mode tag set used to configure dialogue runtime behavior."))
	FGameplayTagContainer PerPlayerDialogueModeTags;

	// Modes where shared dialogue should assert pause through an external game-state pause reason.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Mode", meta = (ToolTip = "Mode tag set used to configure dialogue runtime behavior."))
	FGameplayTagContainer PauseOnDialogueModeTags;

	// Safety guard for runtime node traversal loops.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta=(ClampMin="16", UIMin="16", ToolTip = "Safety cap for maximum node steps processed per dialogue advance."))
	int32 MaxExecutionStepsPerAdvance = 1024;

	// Safety guard for read-only choice lookahead traversal.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta=(ClampMin="16", UIMin="16", ToolTip = "Safety cap for maximum node steps processed when previewing a highlighted choice branch."))
	int32 MaxLookaheadSteps = 1024;

	// Ordered list mapping player slots to tags (Index 0 = P1, Index 1 = P2).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta=(ToolTip = "Ordered list mapping player slots to tags (index 0 = P1, index 1 = P2)."))
	TArray<FGameplayTag> PlayerSlotTags;

	// In per-player dialogue modes, allow only one active owner session per primary speaker at a time.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta = (ToolTip = "Runtime dialogue behavior toggle used by Parley systems."))
	bool bOnlyOneTalkerPerSpeakerInPerPlayerModes = true;

	// When a speaker is busy and one-talker mode blocks starting a new session, attempt auto-eavesdrop to the active owner by default.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta = (ToolTip = "Runtime dialogue behavior toggle used by Parley systems."))
	bool bAutoEavesdropOnBusySpeakerByDefault = true;

	// Emotion tag requested when a speaker is busy in one-talker mode.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta = (Categories = "Dialogue", ToolTip = "Emotion tag requested when a speaker is busy in one-talker-per-speaker mode."))
	FGameplayTag BusySpeakerEmotionTag;
};
