#include "ARFactionSubsystem.h"

#include "ARFactionSettings.h"
#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ContentLookupSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"

namespace
{
	static bool IsAuthorityWorld_Faction(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
	}

	static void GetCloutAndProgressionTags(const UARSaveSubsystem* SaveSubsystem, int32& OutClout, FGameplayTagContainer& OutProgressionTags)
	{
		OutClout = 0;
		OutProgressionTags.Reset();

		if (!SaveSubsystem)
		{
			return;
		}

		const UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
		if (!SaveGame)
		{
			return;
		}

		OutClout = FMath::Max(0, SaveGame->FactionClout);
		OutProgressionTags = SaveGame->ProgressionTags;
	}
}

void UARFactionSubsystem::Deinitialize()
{
	CurrentCandidates.Reset();
	VoteSelections.Reset();
	SnapshotPopularityStates.Reset();
	SnapshotRankedFactions.Reset();
	bSnapshotValid = false;
	Super::Deinitialize();
}

bool UARFactionSubsystem::RefreshElectionSnapshot()
{
	if (!IsAuthorityWorld_Faction(GetWorld()))
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] RefreshElectionSnapshot ignored on non-authority."));
		return false;
	}

	bSnapshotValid = false;
	CurrentCandidates.Reset();
	SnapshotPopularityStates.Reset();
	SnapshotRankedFactions.Reset();
	ClearVotes();
	return EnsureElectionSnapshot();
}

bool UARFactionSubsystem::SubmitVote(EARPlayerSlot PlayerSlot, FGameplayTag SelectedFactionTag)
{
	if (!IsAuthorityWorld_Faction(GetWorld()))
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] SubmitVote ignored on non-authority."));
		return false;
	}

	if (PlayerSlot == EARPlayerSlot::Unknown || !SelectedFactionTag.IsValid())
	{
		return false;
	}

	if (!EnsureElectionSnapshot())
	{
		return false;
	}

	if (!IsFactionInCandidates(CurrentCandidates, SelectedFactionTag))
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] SubmitVote rejected: '%s' is not in current candidate list."), *SelectedFactionTag.ToString());
		return false;
	}

	const int32 VoteIndex = FindVoteIndexBySlot(VoteSelections, PlayerSlot);
	if (VoteIndex != INDEX_NONE)
	{
		VoteSelections[VoteIndex].SelectedFactionTag = SelectedFactionTag;
		VoteSelections[VoteIndex].bHasSelection = true;
		return true;
	}

	FARFactionVoteSelection Selection;
	Selection.PlayerSlot = PlayerSlot;
	Selection.SelectedFactionTag = SelectedFactionTag;
	Selection.bHasSelection = true;
	VoteSelections.Add(Selection);
	return true;
}

void UARFactionSubsystem::ClearVotes()
{
	VoteSelections.Reset();
}

bool UARFactionSubsystem::FinalizeElectionForTravel(FGameplayTag& OutWinnerFactionTag, EARFactionWinnerReason& OutReason)
{
	if (!IsAuthorityWorld_Faction(GetWorld()))
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] FinalizeElectionForTravel ignored on non-authority."));
		return false;
	}

	OutWinnerFactionTag = FGameplayTag();
	OutReason = EARFactionWinnerReason::NoValidFactions;

	UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] FinalizeElectionForTravel failed: SaveSubsystem missing."));
		return false;
	}

	if (!EnsureElectionSnapshot())
	{
		// No snapshot means no valid factions loaded; clear active state to avoid stale effects.
		const bool bApplied = ApplyWinner(SaveSubsystem, FGameplayTag(), EARFactionWinnerReason::NoValidFactions);
		OutReason = EARFactionWinnerReason::NoValidFactions;
		return bApplied;
	}

	if (SnapshotRankedFactions.IsEmpty())
	{
		const bool bApplied = ApplyWinner(SaveSubsystem, FGameplayTag(), EARFactionWinnerReason::NoValidFactions);
		OutReason = EARFactionWinnerReason::NoValidFactions;
		return bApplied;
	}

	CommitPopularitySnapshotToSave(SaveSubsystem);

	int32 Clout = 0;
	FGameplayTagContainer IgnoredProgressionTags;
	GetCloutAndProgressionTags(SaveSubsystem, Clout, IgnoredProgressionTags);
	if (Clout <= 0)
	{
		const bool bApplied = ApplyWinner(SaveSubsystem, FGameplayTag(), EARFactionWinnerReason::DisabledByClout);
		OutReason = EARFactionWinnerReason::DisabledByClout;
		ClearVotes();
		bSnapshotValid = false;
		CurrentCandidates.Reset();
		SnapshotPopularityStates.Reset();
		SnapshotRankedFactions.Reset();
		return bApplied;
	}

	FGameplayTag Winner;
	EARFactionWinnerReason Reason = EARFactionWinnerReason::NoValidFactions;
	if (!TryResolveWinnerFromVotes(SnapshotRankedFactions, Winner, Reason))
	{
		return false;
	}

	if (!ApplyWinner(SaveSubsystem, Winner, Reason))
	{
		return false;
	}

	OutWinnerFactionTag = Winner;
	OutReason = Reason;
	ClearVotes();
	bSnapshotValid = false;
	CurrentCandidates.Reset();
	SnapshotPopularityStates.Reset();
	SnapshotRankedFactions.Reset();
	return true;
}

bool UARFactionSubsystem::EnsureElectionSnapshot()
{
	if (bSnapshotValid)
	{
		return true;
	}

	TArray<FFactionResolvedDef> RankedDefs;
	FString Error;
	if (!BuildResolvedDefinitions(RankedDefs, Error))
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] EnsureElectionSnapshot failed: %s"), *Error);
		return false;
	}

	UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr;
	int32 Clout = 0;
	FGameplayTagContainer IgnoredProgressionTags;
	GetCloutAndProgressionTags(SaveSubsystem, Clout, IgnoredProgressionTags);

	const int32 CandidateCount = FMath::Clamp(Clout, 0, RankedDefs.Num());
	CurrentCandidates.Reset(CandidateCount);
	for (int32 Index = 0; Index < CandidateCount; ++Index)
	{
		CurrentCandidates.Add(RankedDefs[Index].FactionTag);
	}

	SnapshotPopularityStates.Reset(RankedDefs.Num());
	SnapshotRankedFactions.Reset(RankedDefs.Num());
	for (const FFactionResolvedDef& Def : RankedDefs)
	{
		FARFactionRuntimeState Entry;
		Entry.FactionTag = Def.FactionTag;
		Entry.Popularity = Def.DriftedPopularity;
		SnapshotPopularityStates.Add(Entry);
		SnapshotRankedFactions.Add(Def.FactionTag);
	}

	bSnapshotValid = true;
	OnFactionCandidatesRefreshed.Broadcast();
	return true;
}

bool UARFactionSubsystem::BuildResolvedDefinitions(TArray<FFactionResolvedDef>& OutDefs, FString& OutError) const
{
	OutDefs.Reset();
	OutError.Reset();

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		OutError = TEXT("GameInstance is null.");
		return false;
	}

	UContentLookupSubsystem* Lookup = GI->GetSubsystem<UContentLookupSubsystem>();
	UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>();
	if (!Lookup || !SaveSubsystem)
	{
		OutError = TEXT("Required subsystems are missing.");
		return false;
	}

	const UARFactionSettings* FactionSettings = GetDefault<UARFactionSettings>();
	const FGameplayTag RootTag = (FactionSettings ? FactionSettings->FactionDefinitionRootTag : FGameplayTag());
	if (!RootTag.IsValid())
	{
		OutError = TEXT("FactionDefinitionRootTag is not configured.");
		return false;
	}

	TArray<FName> RowNames;
	if (!Lookup->GetAllRowNamesForRootTag(RootTag, RowNames, OutError))
	{
		return false;
	}

	const UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
	FGameplayTagContainer ProgressionTags;
	int32 IgnoredClout = 0;
	GetCloutAndProgressionTags(SaveSubsystem, IgnoredClout, ProgressionTags);

	TMap<FGameplayTag, float> PersistedPopularity;
	if (SaveGame)
	{
		for (const FARFactionRuntimeState& RuntimeState : SaveGame->FactionPopularityStates)
		{
			if (RuntimeState.FactionTag.IsValid())
			{
				PersistedPopularity.Add(RuntimeState.FactionTag, RuntimeState.Popularity);
			}
		}
	}

	for (const FName RowName : RowNames)
	{
		const FGameplayTag CandidateTag = BuildFactionTagFromRootAndLeaf(RootTag, RowName);
		if (!CandidateTag.IsValid())
		{
			continue;
		}

		FARFactionDefinitionRow Row;
		FString ResolveError;
		if (!ResolveFactionDefinition(CandidateTag, Row, ResolveError))
		{
			UE_LOG(ARLog, Warning, TEXT("[Faction] Failed to resolve '%s': %s"), *CandidateTag.ToString(), *ResolveError);
			continue;
		}

		const FGameplayTag EffectiveFactionTag = Row.FactionTag.IsValid() ? Row.FactionTag : CandidateTag;
		const float PriorPopularity = PersistedPopularity.Contains(EffectiveFactionTag) ? PersistedPopularity[EffectiveFactionTag] : Row.BasePopularity;
		const float DriftMin = FMath::Min(Row.DriftPerCycleMin, Row.DriftPerCycleMax);
		const float DriftMax = FMath::Max(Row.DriftPerCycleMin, Row.DriftPerCycleMax);
		const float DriftedPopularity = ClampPopularity(Row, PriorPopularity + FMath::FRandRange(DriftMin, DriftMax));
		const float ModifierDelta = ComputeModifierDelta(Row, ProgressionTags);

		FFactionResolvedDef Resolved;
		Resolved.FactionTag = EffectiveFactionTag;
		Resolved.Definition = Row;
		Resolved.EffectiveScore = DriftedPopularity + ModifierDelta;
		Resolved.DriftedPopularity = DriftedPopularity;
		OutDefs.Add(MoveTemp(Resolved));
	}

	OutDefs.Sort(&UARFactionSubsystem::SortByScoreThenTag);
	return true;
}

bool UARFactionSubsystem::ResolveFactionDefinition(const FGameplayTag& FactionTag, FARFactionDefinitionRow& OutRow, FString& OutError) const
{
	OutError.Reset();

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		OutError = TEXT("GameInstance is null.");
		return false;
	}

	UContentLookupSubsystem* Lookup = GI->GetSubsystem<UContentLookupSubsystem>();
	if (!Lookup)
	{
		OutError = TEXT("ContentLookupSubsystem missing.");
		return false;
	}

	FInstancedStruct RowData;
	if (!Lookup->LookupWithGameplayTag(FactionTag, RowData, OutError))
	{
		return false;
	}

	if (const FARFactionDefinitionRow* TypedRow = RowData.GetPtr<FARFactionDefinitionRow>())
	{
		OutRow = *TypedRow;
		return true;
	}

	OutError = FString::Printf(TEXT("Row struct mismatch for '%s'; expected FARFactionDefinitionRow."), *FactionTag.ToString());
	return false;
}

float UARFactionSubsystem::ComputeModifierDelta(const FARFactionDefinitionRow& Row, const FGameplayTagContainer& ProgressionTags) const
{
	float Delta = 0.0f;
	for (const FARFactionPopularityModifierRule& Rule : Row.PopularityModifierRules)
	{
		if (!Rule.ConditionTag.IsValid())
		{
			continue;
		}

		if (ProgressionTags.HasTag(Rule.ConditionTag))
		{
			Delta += Rule.Delta;
		}
	}

	return Delta;
}

void UARFactionSubsystem::CommitPopularitySnapshotToSave(UARSaveSubsystem* SaveSubsystem) const
{
	if (!SaveSubsystem)
	{
		return;
	}

	UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
	if (!SaveGame)
	{
		return;
	}

	SaveGame->FactionPopularityStates = SnapshotPopularityStates;
	for (FARFactionRuntimeState& RuntimeState : SaveGame->FactionPopularityStates)
	{
		if (!RuntimeState.FactionTag.IsValid())
		{
			RuntimeState.Popularity = 0.0f;
		}
	}
	SaveSubsystem->MarkSaveDirty();
}

bool UARFactionSubsystem::TryResolveWinnerFromVotes(const TArray<FGameplayTag>& RankedFactions, FGameplayTag& OutWinner, EARFactionWinnerReason& OutReason) const
{
	OutWinner = FGameplayTag();
	OutReason = EARFactionWinnerReason::NoValidFactions;

	if (RankedFactions.IsEmpty())
	{
		return false;
	}

	const FARFactionVoteSelection* P1Vote = nullptr;
	const FARFactionVoteSelection* P2Vote = nullptr;
	for (const FARFactionVoteSelection& Vote : VoteSelections)
	{
		if (!Vote.bHasSelection || !Vote.SelectedFactionTag.IsValid())
		{
			continue;
		}

		if (Vote.PlayerSlot == EARPlayerSlot::P1)
		{
			P1Vote = &Vote;
		}
		else if (Vote.PlayerSlot == EARPlayerSlot::P2)
		{
			P2Vote = &Vote;
		}
	}

	if (P1Vote && P2Vote)
	{
		if (P1Vote->SelectedFactionTag == P2Vote->SelectedFactionTag)
		{
			OutWinner = P1Vote->SelectedFactionTag;
			OutReason = EARFactionWinnerReason::SamePick;
			return true;
		}

		// Future hook: replace this random branch with deterministic RPS minigame resolution.
		OutWinner = (FMath::RandBool() ? P1Vote->SelectedFactionTag : P2Vote->SelectedFactionTag);
		OutReason = EARFactionWinnerReason::DivergedRandom;
		return true;
	}

	if (P1Vote || P2Vote)
	{
		OutWinner = P1Vote ? P1Vote->SelectedFactionTag : P2Vote->SelectedFactionTag;
		OutReason = EARFactionWinnerReason::SinglePick;
		return true;
	}

	OutWinner = RankedFactions[0];
	OutReason = EARFactionWinnerReason::NoVotesTopPopularity;
	return true;
}

bool UARFactionSubsystem::ApplyWinner(UARSaveSubsystem* SaveSubsystem, const FGameplayTag& WinnerFactionTag, EARFactionWinnerReason Reason)
{
	if (!SaveSubsystem)
	{
		return false;
	}

	FGameplayTagContainer EffectTags;
	if (WinnerFactionTag.IsValid())
	{
		FARFactionDefinitionRow WinnerRow;
		FString Error;
		if (!ResolveFactionDefinition(WinnerFactionTag, WinnerRow, Error))
		{
			UE_LOG(ARLog, Warning, TEXT("[Faction] Failed to resolve winner row '%s': %s"), *WinnerFactionTag.ToString(), *Error);
			return false;
		}

		EffectTags = WinnerRow.EffectTags;
	}

	if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
	{
		SaveGame->ActiveFactionTag = WinnerFactionTag;
		SaveGame->ActiveFactionEffectTags = EffectTags;
		SaveSubsystem->MarkSaveDirty();
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[Faction] ApplyWinner continuing without current save; applying runtime GameState only."));
	}

	if (UWorld* World = GetWorld())
	{
		if (AARGameStateBase* GS = World->GetGameState<AARGameStateBase>())
		{
			GS->SetActiveFactionTagFromSave(WinnerFactionTag);
			GS->SetActiveFactionEffectTagsFromSave(EffectTags);
		}
	}

	OnFactionElectionFinalized.Broadcast(WinnerFactionTag, Reason, EffectTags);
	return true;
}

int32 UARFactionSubsystem::FindVoteIndexBySlot(const TArray<FARFactionVoteSelection>& InVotes, EARPlayerSlot Slot)
{
	for (int32 Index = 0; Index < InVotes.Num(); ++Index)
	{
		if (InVotes[Index].PlayerSlot == Slot)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool UARFactionSubsystem::IsFactionInCandidates(const TArray<FGameplayTag>& Candidates, const FGameplayTag& FactionTag)
{
	for (const FGameplayTag& Candidate : Candidates)
	{
		if (Candidate == FactionTag)
		{
			return true;
		}
	}
	return false;
}

bool UARFactionSubsystem::SortByScoreThenTag(const FFactionResolvedDef& A, const FFactionResolvedDef& B)
{
	if (!FMath::IsNearlyEqual(A.EffectiveScore, B.EffectiveScore))
	{
		return A.EffectiveScore > B.EffectiveScore;
	}
	return A.FactionTag.ToString() < B.FactionTag.ToString();
}

float UARFactionSubsystem::ClampPopularity(const FARFactionDefinitionRow& Row, float Value)
{
	const float MinValue = FMath::Min(Row.MinPopularity, Row.MaxPopularity);
	const float MaxValue = FMath::Max(Row.MinPopularity, Row.MaxPopularity);
	return FMath::Clamp(Value, MinValue, MaxValue);
}

FGameplayTag UARFactionSubsystem::BuildFactionTagFromRootAndLeaf(const FGameplayTag& RootTag, FName LeafRowName)
{
	if (!RootTag.IsValid() || LeafRowName.IsNone())
	{
		return FGameplayTag();
	}

	const FString TagPath = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *LeafRowName.ToString());
	return UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagPath), false);
}
