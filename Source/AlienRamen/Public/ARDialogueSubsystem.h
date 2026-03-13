/**
 * @file ARDialogueSubsystem.h
 * @brief Server-authoritative compiled-graph dialogue runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARDialogueTypes.h"
#include "ARDialogueSubsystem.generated.h"

class AARPlayerController;
class AActor;
class UARDialogueConversationAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnDialogueSessionUpdated, const FDialogueClientView&, View);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnDialogueSessionEnded, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnConversationCompletedSignature, FGameplayTag, ConversationTag);

UCLASS()
class ALIENRAMEN_API UARDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Required runtime API contracts ----

	/** Finds the best available conversation for the given speaker and returns an offer view. Returns false when nothing is talkable. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool GetAvailableConversationForSpeaker(AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag, FDialogueConversationOffer& OutOffer, bool bSpeakerLocalStateAllowsDialogue = true);

	/** Starts a conversation by tag for the requesting controller/speaker pair. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool StartConversation(AARPlayerController* RequestingController, FGameplayTag ConversationTag, FGameplayTag PrimarySpeakerTag);

	/** Advances the active conversation to the next node when no choice is required. Call on interact/confirm input. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool AdvanceConversation(AARPlayerController* RequestingController);

	/** Submits a player choice by branch id (from the latest client view). Returns false if choice is invalid. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SubmitChoice(AARPlayerController* RequestingController, FGuid ChoiceBranchId);

	/** Toggle eavesdrop mode, letting a player hear another slot's conversation. Use in shop two-up flows. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ForceEavesdrop(AARPlayerController* RequestingController, bool bEnable, EARPlayerSlot TargetSlot);

	/** Merge player and world progression tags into one container for conversation evaluation. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	FGameplayTagContainer GetCombinedDialogueTags(const FGameplayTagContainer& PlayerOnlyProgressionTags, const FGameplayTagContainer& GameOnlyProgressionTags) const;

	/** Evaluate a single dialogue condition against the provided runtime context (pure helper for graph testing). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool EvaluateDialogueCondition(const FDialogueCondition& Condition, const FDialogueRuntimeContext& Context) const;

	/** Apply tag mutations (add/remove) to the runtime state; authority-only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyDialogueTagMutation(const FDialogueTagMutation& Mutation, const FDialogueRuntimeContext& Context);

	/** Apply relationship delta for a speaker; authority-only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyDialogueRelationshipMutation(const FDialogueRelationshipMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	/** Apply faction relationship delta; authority-only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyDialogueFactionMutation(const FDialogueFactionMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	// Shop/customer integration endpoint: applies relationship delta and emotion output in one call.
	/** Convenience helper for shop serve results: bumps relationship and records reaction emotion for the given speaker. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyRamenServeOutcome(
		FGameplayTag SpeakerTag,
		int32 RelationshipDeltaPoints,
		FGameplayTag ReactionEmotionTag,
		AActor* PreferredSpeakerActor = nullptr);

	/** Run validation on a conversation asset (runtime-safe). Use in editor tooling and runtime assertions. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ValidateConversation(UARDialogueConversationAsset* ConversationAsset, FDialogueValidationReport& OutReport) const;

	/** Validate a single speaker row for required fields. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ValidateSpeaker(const FARDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const;

	/** Preview a conversation with a provided runtime context to see the first client view without committing state. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool PreviewConversation(UARDialogueConversationAsset* ConversationAsset, const FDialogueRuntimeContext& PreviewContext, FDialogueClientView& OutFirstView, FDialogueValidationReport& OutReport) const;

	// Editor/tooling preview runner: simulates a full conversation trace with auto-advance and auto-choice routing.
	bool PreviewConversationTrace(
		UARDialogueConversationAsset* ConversationAsset,
		const FDialogueRuntimeContext& PreviewContext,
		int32 MaxInteractiveSteps,
		TArray<FDialogueClientView>& OutViews,
		TArray<FGuid>& OutAutoSelectedChoiceBranchIds,
		bool& bOutEndedCompleted,
		FDialogueValidationReport& OutReport) const;

	// ---- Compatibility wrappers used by gameplay code ----

	/** Backwards-compatible wrapper: start the best conversation for the speaker if one is available. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool TryStartDialogueWithSpeaker(AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag);

	/** Backwards-compatible alias for SubmitChoice. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SubmitDialogueChoice(AARPlayerController* RequestingController, FGuid ChoiceBranchId)
	{
		return SubmitChoice(RequestingController, ChoiceBranchId);
	}

	/** Backwards-compatible alias for ForceEavesdrop. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SetShopEavesdropTarget(AARPlayerController* RequestingController, EARPlayerSlot TargetSlot, bool bEnable)
	{
		return ForceEavesdrop(RequestingController, bEnable, TargetSlot);
	}

	/** Returns true when the given player slot has unlocked any conversation for this speaker. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasUnlockedDialogueForSpeakerForSlot(FGameplayTag PrimarySpeakerTag, EARPlayerSlot PlayerSlot) const;

	/** Returns true when any player slot has an unlocked conversation for this speaker. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasUnlockedDialogueForSpeakerForAnyPlayer(FGameplayTag PrimarySpeakerTag) const;

	// Returns the union of registered speaker tags known to dialogue runtime (conversation primaries + speaker records).
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	void GetRegisteredPrimarySpeakerTags(TArray<FGameplayTag>& OutSpeakerTags) const;

	/** True when the speaker is already in a conversation for this controller (blocks new starts). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool IsSpeakerBusyForController(const AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag) const;

	/** Gets the latest client view for the requesting controller (choices, lines, etc.). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool GetLocalViewForController(const AARPlayerController* RequestingController, FDialogueClientView& OutView) const;

	/** True when any dialogue session is active. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasActiveDialogueSession() const;

	// Clears transient per-cycle offer blockers (seen/skipped). Pass Unknown to clear all player slots.
	/** Clear seen/skipped blockers for the current offer cycle. Pass Unknown to clear for all slots. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void ClearConversationCycleOfferState(EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown);

	/** Current relationship points for the speaker (already includes save + runtime mutations). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	float GetRelationshipPointsForSpeaker(FGameplayTag SpeakerTag) const;

	/** Current relationship level bucket for the speaker. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	int32 GetRelationshipLevelForSpeaker(FGameplayTag SpeakerTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionUpdated OnDialogueSessionUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionEnded OnDialogueSessionEnded;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnConversationCompletedSignature OnConversationCompleted;

private:
	struct FARDialogueRuntimeState;
	struct FARDialogueRuntimeStateDeleter
	{
		void operator()(FARDialogueRuntimeState* Ptr) const;
	};

	FARDialogueRuntimeState& GetRuntimeState();
	const FARDialogueRuntimeState& GetRuntimeState() const;

	TUniquePtr<FARDialogueRuntimeState, FARDialogueRuntimeStateDeleter> RuntimeState;
};
