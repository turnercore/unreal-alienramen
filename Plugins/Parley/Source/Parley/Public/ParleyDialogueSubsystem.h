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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnConversationStarted, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, PlayerSlotTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnConversationEnded, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, PlayerSlotTag, bool, bCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnLineDelivered, FGameplayTag, SpeakerTag, FGameplayTag, ConversationTag, FGameplayTag, PlayerSlotTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnImportantChoiceMade, FGuid, ChoiceBranchId, FGameplayTag, ConversationTag, FGameplayTag, SpeakerTag, FGameplayTag, PlayerSlotTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnRelationshipLevelChanged, FGameplayTag, SpeakerTag, FGameplayTag, PlayerSlotTag, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnConversationCompleted, FGameplayTag, ConversationTag, FGameplayTag, PlayerSlotTag, FGameplayTag, CharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnRelationshipChanged, FGameplayTag, SpeakerTag, FGameplayTag, PlayerSlotTag, float, Delta, float, NewTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnProgressionTagMutated, FGameplayTag, ProgressionTag, bool, bAdded, FGameplayTag, PlayerSlotTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnChoiceLookaheadEmotion, FGameplayTag, PrimarySpeakerTag, FGameplayTag, PreviewEmotionTag, FGuid, ChoiceBranchId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnChoiceLookaheadCleared, FGameplayTag, PlayerSlotTag);
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
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool GetAvailableConversationForSpeaker(APlayerController* RequestingController, FGameplayTag PrimarySpeakerTag, FDialogueConversationOffer& OutOffer, bool bSpeakerLocalStateAllowsDialogue = true);

	/** Starts a conversation by tag for the requesting controller/speaker pair. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool StartConversation(APlayerController* RequestingController, FGameplayTag ConversationTag, FGameplayTag PrimarySpeakerTag);

	/** Advances the active conversation to the next node when no choice is required. Call on interact/confirm input. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool AdvanceConversation(APlayerController* RequestingController);

	/** Submits a player choice by branch id (from the latest client view). Returns false if choice is invalid. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool SubmitChoice(APlayerController* RequestingController, FGuid ChoiceBranchId);

	/** Toggle eavesdrop mode, letting a player hear another slot's conversation. Use in shop two-up flows. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ForceEavesdrop(APlayerController* RequestingController, bool bEnable, FGameplayTag TargetSlotTag);

	/** Preview the highlighted choice branch without mutating dialogue state. Pass invalid branch id to clear preview. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Performs read-only choice lookahead and broadcasts a preview emotion for the active primary speaker."))
	bool HighlightDialogueChoice(APlayerController* RequestingController, FGuid ChoiceBranchId);

	/** Merge player and world progression tags into one container for conversation evaluation. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	FGameplayTagContainer GetCombinedDialogueTags(const FGameplayTagContainer& PlayerOnlyProgressionTags, const FGameplayTagContainer& GameOnlyProgressionTags) const;

	/** Evaluate a single dialogue condition against the provided runtime context (pure helper for graph testing). */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool EvaluateDialogueCondition(const FDialogueCondition& Condition, const FDialogueRuntimeContext& Context) const;

	/** Apply tag mutations (add/remove) to the runtime state; authority-only. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ApplyDialogueTagMutation(const FDialogueTagMutation& Mutation, const FDialogueRuntimeContext& Context);

	/** Apply relationship delta for a speaker; authority-only. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ApplyDialogueRelationshipMutation(const FDialogueRelationshipMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	/** Apply faction relationship delta; authority-only. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ApplyDialogueFactionMutation(const FDialogueFactionMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	// Shop/customer integration endpoint: applies relationship delta and emotion output in one call.
	/** Convenience helper for shop serve results: bumps relationship and records reaction emotion for the given speaker. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ApplyRamenServeOutcome(
		FGameplayTag SpeakerTag,
		int32 RelationshipDeltaPoints,
		FGameplayTag ReactionEmotionTag,
		AActor* PreferredSpeakerActor = nullptr);

	/** Run validation on a conversation asset (runtime-safe). Use in editor tooling and runtime assertions. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ValidateConversation(UParleyConversationAsset* ConversationAsset, FDialogueValidationReport& OutReport) const;

	/** Validate a single speaker row for required fields. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool ValidateSpeaker(const FParleySpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const;

	/** Preview a conversation with a provided runtime context to see the first client view without committing state. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
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

	// ---- Compatibility wrappers used by gameplay code ----

	/** Backwards-compatible wrapper: start the best conversation for the speaker if one is available. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool TryStartDialogueWithSpeaker(APlayerController* RequestingController, FGameplayTag PrimarySpeakerTag);

	/** Backwards-compatible alias for SubmitChoice. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool SubmitDialogueChoice(APlayerController* RequestingController, FGuid ChoiceBranchId)
	{
		return SubmitChoice(RequestingController, ChoiceBranchId);
	}

	/** Backwards-compatible alias for ForceEavesdrop. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	bool SetShopEavesdropTarget(APlayerController* RequestingController, FGameplayTag TargetSlotTag, bool bEnable)
	{
		return ForceEavesdrop(RequestingController, bEnable, TargetSlotTag);
	}

	/** Returns true when the given player slot has unlocked any conversation for this speaker. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool HasUnlockedDialogueForSpeakerForSlot(FGameplayTag PrimarySpeakerTag, FGameplayTag PlayerSlotTag) const;

	/** Returns true when any player slot has an unlocked conversation for this speaker. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool HasUnlockedDialogueForSpeakerForAnyPlayer(FGameplayTag PrimarySpeakerTag) const;

	/** Returns true when this primary speaker currently has any active dialogue session (any slot). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool IsPrimarySpeakerInActiveSession(FGameplayTag PrimarySpeakerTag) const;

	/** Resolves primary speaker tag for a conversation tag from registered runtime conversation data. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool GetPrimarySpeakerForConversation(FGameplayTag ConversationTag, FGameplayTag& OutPrimarySpeakerTag) const;

	// Returns the union of registered speaker tags known to dialogue runtime (conversation primaries + speaker records).
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	void GetRegisteredPrimarySpeakerTags(TArray<FGameplayTag>& OutSpeakerTags) const;

	/** True when the speaker is already in a conversation for this controller (blocks new starts). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool IsSpeakerBusyForController(const APlayerController* RequestingController, FGameplayTag PrimarySpeakerTag) const;

	/** Gets the latest client view for the requesting controller (choices, lines, etc.). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool GetLocalViewForController(const APlayerController* RequestingController, FDialogueClientView& OutView) const;

	/** True when any dialogue session is active. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	bool HasActiveDialogueSession() const;

	// Clears transient per-cycle offer blockers (seen/skipped). Pass Unknown to clear all player slots.
	/** Clear seen/skipped blockers for the current offer cycle. Pass Unknown to clear for all slots. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	void ClearConversationCycleOfferState(FGameplayTag PlayerSlotTag = FGameplayTag());

	/** Current relationship points for the speaker (already includes save + runtime mutations). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	float GetRelationshipPointsForSpeaker(FGameplayTag SpeakerTag) const;

	/** Current relationship level bucket for the speaker. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns Parley dialogue runtime state without mutation."))
	int32 GetRelationshipLevelForSpeaker(FGameplayTag SpeakerTag) const;

	/** Injects persistent progression state for a player slot from an external save system bridge. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	void SetProgressionStateForPlayer(FGameplayTag PlayerSlotTag, const FParleyProgressionState& State);

	/** Injects global game-owned progression tags used by dialogue conditions. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	void SetGameProgressionTags(const FGameplayTagContainer& Tags);

	/** Injects game-scope completed conversation tags used by dialogue completion checks. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	void SetCompletedConversationTagsByGame(const FGameplayTagContainer& Tags);

	/** Returns the currently injected game-scope completed conversation tags. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue", meta = (ToolTip = "Returns game-scope completed conversation tags currently tracked by Parley runtime state."))
	void GetCompletedConversationTagsByGame(FGameplayTagContainer& OutTags) const;

	/** Injects relationship states from an external save system bridge. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue", meta = (ToolTip = "Executes a Parley dialogue runtime operation."))
	void SetRelationshipStates(const TArray<FDialogueRelationshipState>& States);

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast delegate exposed to Blueprint for dialogue lifecycle updates."))
	FParleyOnDialogueSessionUpdated OnDialogueSessionUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast delegate exposed to Blueprint for dialogue lifecycle updates."))
	FParleyOnDialogueSessionEnded OnDialogueSessionEnded;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast delegate exposed to Blueprint for dialogue lifecycle updates."))
	FParleyOnConversationCompletedSignature OnConversationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation session starts. Params: ConversationTag, SpeakerTag, PlayerSlotTag."))
	FParleyOnConversationStarted OnConversationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation session ends. Params: ConversationTag, SpeakerTag, PlayerSlotTag, bCompleted."))
	FParleyOnConversationEnded OnConversationEnded;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a dialogue line is delivered to clients. Params: SpeakerTag, ConversationTag, PlayerSlotTag."))
	FParleyOnLineDelivered OnLineDelivered;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a submitted important choice branch is selected. Params: ChoiceBranchId, ConversationTag, SpeakerTag, PlayerSlotTag."))
	FParleyOnImportantChoiceMade OnImportantChoiceMade;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when relationship level threshold changes for a speaker. Params: SpeakerTag, PlayerSlotTag, OldLevel, NewLevel."))
	FParleyOnRelationshipLevelChanged OnRelationshipLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when a conversation is completed for a player slot. Save bridges should persist and mark dirty."))
	FParleyOnConversationCompleted OnParleyConversationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when relationship points are mutated by dialogue. Save bridges should persist and mark dirty."))
	FParleyOnRelationshipChanged OnRelationshipChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when progression tags are added/removed by dialogue. Save bridges should persist and mark dirty."))
	FParleyOnProgressionTagMutated OnProgressionTagMutated;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when highlighted-choice lookahead resolves a preview emotion for the current primary speaker. PreviewEmotionTag is invalid when no preview is available."))
	FParleyOnChoiceLookaheadEmotion OnChoiceLookaheadEmotion;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue", meta = (ToolTip = "Broadcast when highlighted-choice lookahead is cleared for a player slot."))
	FParleyOnChoiceLookaheadCleared OnChoiceLookaheadCleared;

	// Optional query delegates owned by the game module.
	FParleyIsConversationCompleted OnQueryConversationCompleted;
	FParleyGetCurrentModeTag OnQueryCurrentModeTag;

	// Internal runtime-state accessors used by split implementation units.
	FParleyDialogueRuntimeState& GetRuntimeState();
	const FParleyDialogueRuntimeState& GetRuntimeState() const;

private:
	struct FParleyDialogueRuntimeStateDeleter
	{
		void operator()(FParleyDialogueRuntimeState* Ptr) const;
	};

	TUniquePtr<FParleyDialogueRuntimeState, FParleyDialogueRuntimeStateDeleter> RuntimeState;
};
