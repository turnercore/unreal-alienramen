/**
 * @file ARFactionVotingSubsystem.h
 * @brief AR-owned election/voting runtime layered on Parley faction data.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "ARPlayerTypes.h"
#include "ARFactionVotingTypes.h"
#include "ARFactionVotingSubsystem.generated.h"

class AARPlayerStateBase;
class UParleyFactionSubsystem;
class UARSaveSubsystem;
class AARGameStateBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnFactionVoteSubmittedSignature,
	FGameplayTag,
	PlayerSlotTag,
	FGameplayTag,
	VotedFactionTag,
	FGameplayTag,
	PreviousFactionTag);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnFactionElectionFinalizedSignature,
	FGameplayTag,
	WinnerFactionTag,
	FGameplayTagContainer,
	WinnerEffectTags,
	int32,
	WinnerVoteCount);

/**
 * Game-owned voting layer that consumes Parley faction state and applies election outcomes to AR game state/save.
 */
UCLASS()
class ALIENRAMEN_API UARFactionVotingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Returns the current eligible candidate list resolved from voting settings + Parley faction state. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "Builds the current faction candidate list from voting settings and Parley faction runtime state."))
	void GetEligibleFactionCandidates(TArray<FARFactionVotingCandidate>& OutCandidates) const;

	/** Returns true when the faction tag is currently an eligible voting candidate. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "Returns whether the provided faction is currently eligible in the voting candidate pool."))
	bool IsFactionCandidate(FGameplayTag FactionTag) const;

	/** Submit or replace vote for a canonical player slot tag (for example Player.Slot.P1). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (BlueprintAuthorityOnly, ToolTip = "Submits or replaces a vote for a canonical player slot tag."))
	bool SubmitVoteForPlayerSlotTag(FGameplayTag PlayerSlotTag, FGameplayTag FactionTag);

	/** Compatibility wrapper for enum slot-based callers. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (BlueprintAuthorityOnly, ToolTip = "Submits or replaces a vote for an enum player slot."))
	bool SubmitVoteForPlayerSlot(EARPlayerSlot PlayerSlot, FGameplayTag FactionTag);

	/** Convenience wrapper that resolves slot tag from player state. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (BlueprintAuthorityOnly, ToolTip = "Submits or replaces a vote using slot identity from the provided player state."))
	bool SubmitVoteForPlayerState(const AARPlayerStateBase* PlayerState, FGameplayTag FactionTag);

	/** Clears one slot's current vote (if any). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (BlueprintAuthorityOnly, ToolTip = "Clears the current vote for one canonical player slot tag."))
	void ClearVoteForPlayerSlotTag(FGameplayTag PlayerSlotTag);

	/** Clears all submitted votes. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (BlueprintAuthorityOnly, ToolTip = "Clears all submitted faction votes."))
	void ClearAllVotes();

	/** Snapshot of all submitted votes keyed by canonical slot tags. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "Returns current submitted vote entries keyed by canonical slot tags."))
	void GetCurrentVotes(TArray<FARFactionVoteEntry>& OutVotes) const;

	/**
	 * Finalizes election winner from current votes and candidate pool.
	 * Optionally applies outcome to game state/save and optionally clears submitted votes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction|Voting", meta = (BlueprintAuthorityOnly, ToolTip = "Finalizes election winner from current votes and candidates, optionally applying and clearing votes."))
	bool FinalizeElection(
		FGameplayTag& OutWinnerFactionTag,
		FGameplayTagContainer& OutWinnerEffectTags,
		bool bApplyWinnerToGameStateAndSave = true,
		bool bClearVotesAfterFinalize = true);

	/** True when at least one slot currently has a submitted vote. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "Returns whether any player slot currently has a submitted faction vote."))
	bool HasAnySubmittedVotes() const { return VotesByPlayerSlotTag.Num() > 0; }

	/** Fired when a slot vote is submitted/replaced. */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "Broadcast when a slot submits or replaces its faction vote."))
	FAROnFactionVoteSubmittedSignature OnFactionVoteSubmitted;

	/** Fired when election winner is finalized. */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Faction|Voting", meta = (ToolTip = "Broadcast when faction election winner is finalized."))
	FAROnFactionElectionFinalizedSignature OnFactionElectionFinalized;

private:
	bool EnsureAuthorityWorld(const TCHAR* Context) const;
	UParleyFactionSubsystem* ResolveParleyFactionSubsystem() const;
	UARSaveSubsystem* ResolveSaveSubsystem() const;
	AARGameStateBase* ResolveGameState() const;
	int32 ResolveCurrentFactionClout() const;
	int32 ResolveDesiredCandidateCount(int32 FactionClout) const;
	bool BuildCandidateList(TArray<FARFactionVotingCandidate>& OutCandidates) const;
	bool BuildCandidatesFromVotingDataTable(TArray<FARFactionVotingCandidate>& OutCandidates, int32 FactionClout) const;
	void SortAndTrimCandidates(TArray<FARFactionVotingCandidate>& Candidates, int32 DesiredCount) const;
	bool TryResolveCandidateFromVotingRow(const FARFactionVotingDefinitionRow& Row, int32 FactionClout, FARFactionVotingCandidate& OutCandidate) const;
	int32 CountVotesForFaction(FGameplayTag FactionTag) const;
	bool TryApplyWinnerToGame(FGameplayTag WinnerFactionTag, const FGameplayTagContainer& WinnerEffectTags) const;

	TMap<FGameplayTag, FGameplayTag> VotesByPlayerSlotTag;
};
