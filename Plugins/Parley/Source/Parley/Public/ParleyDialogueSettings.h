/**
 * @file ParleyDialogueSettings.h
 * @brief Dialogue runtime/content settings for Parley.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ParleyDialogueSettings.generated.h"

UENUM(BlueprintType)
enum class EParleyDialogueAudioMode : uint8
{
	NativeAudio = 0,
	AudioSignals
};

UCLASS(Config=Parley, DefaultConfig, meta=(DisplayName="Dialogue"))
class PARLEY_API UParleyDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Parley"); }
	virtual FName GetSectionName() const override { return TEXT("Dialogue"); }

	// TagKey root tag used to resolve FParleySpeakerRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content", meta = (ToolTip = "TagKey root tag used to discover dialogue content assets."))
	FGameplayTag SpeakerDefinitionRootTag;

	// TagKey root tag used to resolve FParleyConversationAssetRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content", meta = (ToolTip = "TagKey root tag used to discover dialogue content assets."))
	FGameplayTag ConversationDefinitionRootTag;

	// Modes that use one shared session for all active characters/controllers.
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

	// Controls whether dialogue line audio resolves to native USound playback or signal-style cue events.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Audio", meta = (ToolTip = "Selects how dialogue line audio is emitted. NativeAudio resolves and plays Sound assets. AudioSignals suppresses native sounds and emits cue tags for external systems (for example FMOD bridges)."))
	EParleyDialogueAudioMode DialogueAudioMode = EParleyDialogueAudioMode::NativeAudio;

	// Safety guard for read-only choice lookahead traversal.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta=(ClampMin="16", UIMin="16", ToolTip = "Safety cap for maximum node steps processed when previewing a highlighted choice branch."))
	int32 MaxLookaheadSteps = 1024;

	// In per-player dialogue modes, allow only one active owner session per primary speaker at a time.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta = (ToolTip = "Runtime dialogue behavior toggle used by Parley systems."))
	bool bOnlyOneTalkerPerSpeakerInPerPlayerModes = true;

	// When a speaker is busy and one-talker mode blocks starting a new session, attempt auto-eavesdrop to the active owner by default.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta = (ToolTip = "Runtime dialogue behavior toggle used by Parley systems."))
	bool bAutoEavesdropOnBusySpeakerByDefault = true;

	// Emotion tag requested when a speaker is busy in one-talker mode.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta = (ToolTip = "Emotion tag requested when a speaker is busy in one-talker-per-speaker mode."))
	FGameplayTag BusySpeakerEmotionTag;
};
