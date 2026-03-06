/**
 * @file ARFactionSubsystem.h
 * @brief Server-authoritative faction election runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARFactionTypes.h"
#include "ARFactionSubsystem.generated.h"

class UARSaveSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnFactionCandidatesRefreshed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAROnFactionElectionFinalized, FGameplayTag, WinnerFactionTag, EARFactionWinnerReason, Reason, const FGameplayTagContainer&, WinnerEffectTags);

UCLASS()
class ALIENRAMEN_API UARFactionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// Rebuilds the election snapshot (rankings + candidate list) and clears transient vote selections.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction")
	bool RefreshElectionSnapshot();

	// Returns candidate faction tags for the current election snapshot.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Faction")
	TArray<FGameplayTag> GetCurrentCandidates() const { return CurrentCandidates; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Faction")
	int32 GetCurrentCandidateCount() const { return CurrentCandidates.Num(); }

	// Server-authoritative vote submission.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction")
	bool SubmitVote(EARPlayerSlot PlayerSlot, FGameplayTag SelectedFactionTag);

	// Clears all transient votes for the current election snapshot.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction")
	void ClearVotes();

	// Finalizes election and applies elected faction/effects to save + replicated GameState state.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Faction")
	bool FinalizeElectionForTravel(FGameplayTag& OutWinnerFactionTag, EARFactionWinnerReason& OutReason);

	// Returns current vote state for UI/debug.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Faction")
	TArray<FARFactionVoteSelection> GetVoteSelections() const { return VoteSelections; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Faction")
	FAROnFactionCandidatesRefreshed OnFactionCandidatesRefreshed;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Faction")
	FAROnFactionElectionFinalized OnFactionElectionFinalized;

private:
	struct FFactionResolvedDef
	{
		FGameplayTag FactionTag;
		FARFactionDefinitionRow Definition;
		float EffectiveScore = 0.0f;
		float DriftedPopularity = 0.0f;
	};

	bool EnsureElectionSnapshot();
	bool BuildResolvedDefinitions(TArray<FFactionResolvedDef>& OutDefs, FString& OutError) const;
	bool ResolveFactionDefinition(const FGameplayTag& FactionTag, FARFactionDefinitionRow& OutRow, FString& OutError) const;
	float ComputeModifierDelta(const FARFactionDefinitionRow& Row, const FGameplayTagContainer& ProgressionTags) const;
	void CommitPopularitySnapshotToSave(UARSaveSubsystem* SaveSubsystem) const;
	bool TryResolveWinnerFromVotes(const TArray<FGameplayTag>& RankedFactions, FGameplayTag& OutWinner, EARFactionWinnerReason& OutReason) const;
	bool ApplyWinner(UARSaveSubsystem* SaveSubsystem, const FGameplayTag& WinnerFactionTag, EARFactionWinnerReason Reason);

	static int32 FindVoteIndexBySlot(const TArray<FARFactionVoteSelection>& InVotes, EARPlayerSlot Slot);
	static bool IsFactionInCandidates(const TArray<FGameplayTag>& Candidates, const FGameplayTag& FactionTag);
	static bool SortByScoreThenTag(const FFactionResolvedDef& A, const FFactionResolvedDef& B);
	static float ClampPopularity(const FARFactionDefinitionRow& Row, float Value);
	static FGameplayTag BuildFactionTagFromRootAndLeaf(const FGameplayTag& RootTag, FName LeafRowName);

	UPROPERTY(Transient)
	TArray<FGameplayTag> CurrentCandidates;

	UPROPERTY(Transient)
	TArray<FARFactionVoteSelection> VoteSelections;

	UPROPERTY(Transient)
	TArray<FARFactionRuntimeState> SnapshotPopularityStates;

	UPROPERTY(Transient)
	TArray<FGameplayTag> SnapshotRankedFactions;

	UPROPERTY(Transient)
	bool bSnapshotValid = false;
};
