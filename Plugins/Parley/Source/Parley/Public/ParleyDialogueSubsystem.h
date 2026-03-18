/**
 * @file ParleyDialogueSubsystem.h
 * @brief Server-authoritative compiled-graph dialogue runtime for Parley.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ParleyDialogueTypes.h"
#include "ParleyDialogueSubsystem.generated.h"

class APlayerController;
class AActor;
class UParleyConversationAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnDialogueSessionUpdated, const FDialogueClientView&, View);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnDialogueSessionEnded, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnConversationCompletedSignature, FGameplayTag, ConversationTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnConversationStarted, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, OwnerCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnConversationEnded, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, OwnerCharacterTag, bool, bCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnLineDelivered, FGameplayTag, SpeakerTag, FGameplayTag, ConversationTag, FGameplayTag, OwnerCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnImportantChoiceMade, FGuid, ChoiceBranchId, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, OwnerCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FParleyOnSpeakerRelationshipLevelChanged, FGameplayTag, SourceSpeakerTag, FGameplayTag, TargetSpeakerTag, FGameplayTag, OwnerCharacterTag, int32, OldLevel, int32, NewLevel, float, NewTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnConversationCompleted, FGameplayTag, ConversationTag, FGameplayTag, OwnerCharacterTag, FGameplayTag, CharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FParleyOnSpeakerRelationshipChanged, FGameplayTag, SourceSpeakerTag, FGameplayTag, TargetSpeakerTag, FGameplayTag, OwnerCharacterTag, float, Delta, float, NewTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnProgressionTagMutated, FGameplayTag, ProgressionTag, bool, bAdded, FGameplayTag, OwnerCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FParleyOnProgressionStateMarkedDirty);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnChoiceLookaheadEmotion, FGameplayTag, PrimarySpeakerTag, FGameplayTag, PreviewEmotionTag, FGuid, ChoiceBranchId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnChoiceLookaheadCleared, FGameplayTag, OwnerCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FParleyOnDialogueSignalFired, FGameplayTag, SignalTag, FGameplayTagContainer, PayloadTags, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, OwnerCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnDialogueAudioRequested, const FDialogueAudioRequest&, Request);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FParleyIsConversationCompleted, FGameplayTag, FGameplayTag);
DECLARE_DELEGATE_RetVal(FGameplayTag, FParleyGetCurrentModeTag);

UCLASS()
class PARLEY_API UParleyDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	struct FParleyDialogueRuntimeState;
	struct FParleyDialogueRuntimeStateDeleter;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Required runtime API contracts ----

	/** Finds the best available conversation for the given speaker and returns an offer view. Returns false when nothing is talkable. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Runs this dialogue subsystem operation on authoritative runtime state."))
	bool GetAvailableConversationForSpeaker(APlayerController* RequestingController, FGameplayTag PrimarySpeakerTag, FDialogueConversationOffer& OutOffer, bool bSpeakerLocalStateAllowsDialogue = true);

	/** Starts a conversation by tag for the requesting controller/speaker pair. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Starts a conversation for the requesting controller and speaker on authoritative runtime state."))
	bool StartConversation(APlayerController* RequestingController, FGameplayTag ConversationTag, FGameplayTag PrimarySpeakerTag);

	/** Advances the active conversation to the next node when no choice is required. Call on interact/confirm input. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Advances the active conversation for the requesting controller on authoritative runtime state."))
	bool AdvanceConversation(APlayerController* RequestingController);

	/** Submits a player choice by branch id (from the latest client view). Returns false if choice is invalid. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Submits the selected choice branch for the requesting controller on authoritative runtime state."))
	bool SubmitChoice(APlayerController* RequestingController, FGuid ChoiceBranchId);

	/** Toggle eavesdrop mode, letting a character hear another character's conversation. Use in shop two-up flows. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Enables or disables eavesdrop mode for the requesting controller on authoritative runtime state."))
	bool ForceEavesdrop(APlayerController* RequestingController, bool bEnable, FGameplayTag TargetCharacterTag);

	/** Preview the highlighted choice branch without mutating dialogue state. Pass invalid branch id to clear preview. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Performs read-only choice lookahead and broadcasts a preview emotion for the active primary speaker."))
	bool HighlightDialogueChoice(APlayerController* RequestingController, FGuid ChoiceBranchId);

	/** Merge player and world progression tags into one container for conversation evaluation. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	FGameplayTagContainer GetCombinedDialogueTags(const FGameplayTagContainer& PlayerOnlyProgressionTags, const FGameplayTagContainer& GameOnlyProgressionTags) const;

	/** Evaluate a single dialogue condition against the provided runtime context (pure helper for graph testing). */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Evaluates a dialogue condition against the provided runtime context without mutating subsystem state."))
	bool EvaluateDialogueCondition(const FDialogueCondition& Condition, const FDialogueRuntimeContext& Context) const;

	/** Apply tag mutations (add/remove) to the runtime state; authority-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Applies a dialogue progression tag mutation on authoritative runtime state."))
	bool ApplyDialogueTagMutation(const FDialogueTagMutation& Mutation, const FDialogueRuntimeContext& Context);

	/** Apply relationship delta for a speaker; authority-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Applies a directed relationship delta on authoritative runtime state."))
	bool ApplyDialogueRelationshipMutation(const FDialogueRelationshipMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	/** Apply faction relationship delta; authority-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Applies a faction relationship delta on authoritative runtime state."))
	bool ApplyDialogueFactionMutation(const FDialogueFactionMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	// Shop/customer integration endpoint: applies relationship delta and emotion output in one call.
	/** Convenience helper for shop serve results: bumps relationship and records reaction emotion for the given speaker. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Records a ramen-serve relationship outcome and reaction emotion on authoritative runtime state."))
	bool ApplyRamenServeOutcome(
		FGameplayTag SpeakerTag,
		int32 RelationshipDeltaPoints,
		FGameplayTag ReactionEmotionTag,
		AActor* PreferredSpeakerActor = nullptr);

	/** Run validation on a conversation asset (runtime-safe). Use in editor tooling and runtime assertions. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Validates a conversation asset and returns a report without mutating subsystem state."))
	bool ValidateConversation(UParleyConversationAsset* ConversationAsset, FDialogueValidationReport& OutReport) const;

	/** Validate a single speaker row for required fields. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Validates a speaker row and returns a report without mutating subsystem state."))
	bool ValidateSpeaker(const FParleySpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const;

	/** Preview a conversation with a provided runtime context to see the first client view without committing state. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Builds a read-only preview of a conversation's first client view for the supplied runtime context."))
	bool PreviewConversation(UParleyConversationAsset* ConversationAsset, const FDialogueRuntimeContext& PreviewContext, FDialogueClientView& OutFirstView, FDialogueValidationReport& OutReport) const;

	// Editor/tooling preview runner: simulates a full conversation trace with auto-advance and auto-choice routing.
	bool PreviewConversationTrace(
		UParleyConversationAsset* ConversationAsset,
		const FDialogueRuntimeContext& PreviewContext,
		int32 MaxInteractiveSteps,
		TArray<FDialogueClientView>& OutViews,
		TArray<FGuid>& OutAutoSelectedChoiceBranchIds,
		bool& bOutEndedCompleted,
		FDialogueValidationReport& OutReport) const;

	/** Starts dialogue using explicit source and target speaker identities while keeping owning-character authority. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Starts dialogue between the specified speaker identities on authoritative runtime state."))
	bool TryStartDialogueBetweenSpeakers(APlayerController* RequestingController, FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag);

	/** Returns true when the given character has unlocked any conversation for this speaker. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool HasUnlockedDialogueForSpeakerForCharacter(FGameplayTag PrimarySpeakerTag, FGameplayTag CharacterTag) const;

	/** Returns true when any controlled character has an unlocked conversation for this speaker. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool HasUnlockedDialogueForSpeakerForAnyPlayer(FGameplayTag PrimarySpeakerTag) const;

	/** Returns true when this primary speaker currently has any active dialogue session (any character owner). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool IsPrimarySpeakerInActiveSession(FGameplayTag PrimarySpeakerTag) const;

	/** Resolves primary speaker tag for a conversation tag from registered runtime conversation data. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool GetPrimarySpeakerForConversation(FGameplayTag ConversationTag, FGameplayTag& OutPrimarySpeakerTag) const;

	// Returns the union of registered speaker tags known to dialogue runtime (conversation primaries + speaker records).
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	void GetRegisteredPrimarySpeakerTags(TArray<FGameplayTag>& OutSpeakerTags) const;

	/** True when the speaker is already in a conversation for this controller (blocks new starts). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool IsSpeakerBusyForController(const APlayerController* RequestingController, FGameplayTag PrimarySpeakerTag) const;

	/** Gets the latest client view for the requesting controller (choices, lines, etc.). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool GetLocalViewForController(const APlayerController* RequestingController, FDialogueClientView& OutView) const;

	/** True when any dialogue session is active. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	bool HasActiveDialogueSession() const;

	// Clears transient per-cycle offer blockers (seen/skipped). Pass invalid tag to clear all character states.
	/** Clear seen/skipped blockers for the current offer cycle. Pass invalid tag to clear all characters. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Clears per-cycle offer blockers for one character, or all characters when passed an invalid tag."))
	void ClearConversationCycleOfferState(FGameplayTag CharacterTag = FGameplayTag());

	/** Current directed relationship points for Source -> Target speakers. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	float GetRelationshipPointsForSpeakerPair(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag) const;

	/** Current directed relationship level bucket for Source -> Target speakers. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns current dialogue runtime state without mutating subsystem data."))
	int32 GetRelationshipLevelForSpeakerPair(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag) const;

	/** Injects persistent progression state for a character from an external save system bridge. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Injects persistent progression state for a character from an external save bridge."))
	void SetProgressionStateForCharacter(FGameplayTag CharacterTag, const FParleyProgressionState& State);

	/** Injects global game-owned progression tags used by dialogue conditions. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Injects game-scope progression tags used during dialogue evaluation."))
	void SetGameProgressionTags(const FGameplayTagContainer& Tags);

	/** Injects game-scope completed conversation tags used by dialogue completion checks. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Injects game-scope completed conversation tags used by dialogue completion checks."))
	void SetCompletedConversationTagsByGame(const FGameplayTagContainer& Tags);

	/** Returns the currently injected game-scope completed conversation tags. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns game-scope completed conversation tags currently tracked by Parley runtime state."))
	void GetCompletedConversationTagsByGame(FGameplayTagContainer& OutTags) const;

	/** Injects directed speaker relationship states from an external save system bridge. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Parley|Dialogue", meta = (ToolTip = "Injects directed speaker relationship state from an external save bridge."))
	void SetSpeakerRelationshipStates(const TArray<FDialogueSpeakerRelationshipState>& States);

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when the active client view changes for a dialogue session."))
	FParleyOnDialogueSessionUpdated OnDialogueSessionUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a dialogue session ends after the widget filters to the active session."))
	FParleyOnDialogueSessionEnded OnDialogueSessionEnded;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation completes for any owning character."))
	FParleyOnConversationCompletedSignature OnConversationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation session starts. Params: ConversationTag, SpeakerTag, OwnerCharacterTag."))
	FParleyOnConversationStarted OnConversationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation session ends. Params: ConversationTag, SpeakerTag, OwnerCharacterTag, bCompleted."))
	FParleyOnConversationEnded OnConversationEnded;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a dialogue line is delivered to clients. Params: SpeakerTag, ConversationTag, OwnerCharacterTag."))
	FParleyOnLineDelivered OnLineDelivered;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a submitted important choice branch is selected. Params: ChoiceBranchId, ConversationTag, SpeakerTag, OwnerCharacterTag."))
	FParleyOnImportantChoiceMade OnImportantChoiceMade;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when directed relationship level threshold changes for a source-target speaker pair."))
	FParleyOnSpeakerRelationshipLevelChanged OnSpeakerRelationshipLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation is completed for a character owner. Save bridges should persist and mark dirty."))
	FParleyOnConversationCompleted OnParleyConversationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when directed relationship points are mutated by dialogue. Save bridges should persist and mark dirty."))
	FParleyOnSpeakerRelationshipChanged OnSpeakerRelationshipChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when progression tags are added/removed by dialogue for a character owner. Save bridges should persist and mark dirty."))
	FParleyOnProgressionTagMutated OnProgressionTagMutated;

	/** Broadcast when dialogue progression data was mutated and requires save-bridge persistence. */
	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when dialogue progression state mutates and should be persisted by save bridges."))
	FParleyOnProgressionStateMarkedDirty OnProgressionStateMarkedDirty;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when highlighted-choice lookahead resolves a preview emotion for the current primary speaker. PreviewEmotionTag is invalid when no preview is available."))
	FParleyOnChoiceLookaheadEmotion OnChoiceLookaheadEmotion;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when highlighted-choice lookahead is cleared for a character owner."))
	FParleyOnChoiceLookaheadCleared OnChoiceLookaheadCleared;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Signals", meta = (ToolTip = "Broadcast when a Signal node fires. Params: SignalTag, PayloadTags, ConversationTag, SpeakerTag, OwnerCharacterTag."))
	FParleyOnDialogueSignalFired OnDialogueSignalFired;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Audio", meta = (ToolTip = "Broadcast when dialogue line audio resolves into a native-sound or cue-tag request payload."))
	FParleyOnDialogueAudioRequested OnDialogueAudioRequested;

	// Optional query delegates owned by the game module.
	FParleyIsConversationCompleted OnQueryConversationCompleted;
	FParleyGetCurrentModeTag OnQueryCurrentModeTag;

	// Internal runtime-state accessors used by split implementation units.
	FParleyDialogueRuntimeState& GetRuntimeState();
	const FParleyDialogueRuntimeState& GetRuntimeState() const;

private:
	bool GetAvailableConversationForSpeakerInternal(
		APlayerController* RequestingController,
		FGameplayTag PrimarySpeakerTag,
		FDialogueConversationOffer& OutOffer,
		bool bSpeakerLocalStateAllowsDialogue,
		FGameplayTag SourceSpeakerTagOverride,
		bool bPersistChanceSkipFailures);
	bool StartConversationInternal(
		APlayerController* RequestingController,
		FGameplayTag ConversationTag,
		FGameplayTag PrimarySpeakerTag,
		FGameplayTag SourceSpeakerTagOverride);

	struct FParleyDialogueRuntimeStateDeleter
	{
		void operator()(FParleyDialogueRuntimeState* Ptr) const;
	};

	TUniquePtr<FParleyDialogueRuntimeState, FParleyDialogueRuntimeStateDeleter> RuntimeState;
};
