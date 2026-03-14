#include "ARFactionVotingSubsystem.h"

#include "ARFactionVotingSettings.h"
#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ParleyFactionSubsystem.h"
#include "ParleyFactionTypes.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	static bool IsAuthorityWorld_Voting(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
	}
}

void UARFactionVotingSubsystem::Deinitialize()
{
	VotesByPlayerSlotTag.Reset();
	Super::Deinitialize();
}

void UARFactionVotingSubsystem::GetEligibleFactionCandidates(TArray<FARFactionVotingCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	BuildCandidateList(OutCandidates);
}

bool UARFactionVotingSubsystem::IsFactionCandidate(const FGameplayTag FactionTag) const
{
	if (!FactionTag.IsValid())
	{
		return false;
	}

	TArray<FARFactionVotingCandidate> Candidates;
	if (!BuildCandidateList(Candidates))
	{
		return false;
	}

	return Candidates.ContainsByPredicate(
		[FactionTag](const FARFactionVotingCandidate& Candidate)
		{
			return Candidate.FactionTag.MatchesTagExact(FactionTag);
		});
}

bool UARFactionVotingSubsystem::SubmitVoteForPlayerSlotTag(FGameplayTag PlayerSlotTag, const FGameplayTag FactionTag)
{
	if (!EnsureAuthorityWorld(TEXT("SubmitVoteForPlayerSlotTag")))
	{
		return false;
	}

	const EARPlayerSlot Slot = ARPlayer::GetPlayerSlotForTag(PlayerSlotTag);
	const FGameplayTag CanonicalSlotTag = ARPlayer::GetPlayerSlotTag(Slot);
	if (Slot == EARPlayerSlot::Unknown || !CanonicalSlotTag.IsValid() || !FactionTag.IsValid())
	{
		return false;
	}

	if (!IsFactionCandidate(FactionTag))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[FactionVoting] Rejecting vote for slot '%s': faction '%s' is not an eligible candidate."),
			*CanonicalSlotTag.ToString(),
			*FactionTag.ToString());
		return false;
	}

	const FGameplayTag PreviousFactionTag = VotesByPlayerSlotTag.FindRef(CanonicalSlotTag);
	if (PreviousFactionTag.MatchesTagExact(FactionTag))
	{
		return true;
	}

	VotesByPlayerSlotTag.FindOrAdd(CanonicalSlotTag) = FactionTag;
	OnFactionVoteSubmitted.Broadcast(CanonicalSlotTag, FactionTag, PreviousFactionTag);
	return true;
}

bool UARFactionVotingSubsystem::SubmitVoteForPlayerSlot(const EARPlayerSlot PlayerSlot, const FGameplayTag FactionTag)
{
	return SubmitVoteForPlayerSlotTag(ARPlayer::GetPlayerSlotTag(PlayerSlot), FactionTag);
}

bool UARFactionVotingSubsystem::SubmitVoteForPlayerState(const AARPlayerStateBase* PlayerState, const FGameplayTag FactionTag)
{
	if (!PlayerState)
	{
		return false;
	}

	return SubmitVoteForPlayerSlotTag(PlayerState->GetPlayerSlotTag(), FactionTag);
}

void UARFactionVotingSubsystem::ClearVoteForPlayerSlotTag(FGameplayTag PlayerSlotTag)
{
	if (!EnsureAuthorityWorld(TEXT("ClearVoteForPlayerSlotTag")))
	{
		return;
	}

	const EARPlayerSlot Slot = ARPlayer::GetPlayerSlotForTag(PlayerSlotTag);
	const FGameplayTag CanonicalSlotTag = ARPlayer::GetPlayerSlotTag(Slot);
	if (Slot == EARPlayerSlot::Unknown || !CanonicalSlotTag.IsValid())
	{
		return;
	}

	VotesByPlayerSlotTag.Remove(CanonicalSlotTag);
}

void UARFactionVotingSubsystem::ClearAllVotes()
{
	if (!EnsureAuthorityWorld(TEXT("ClearAllVotes")))
	{
		return;
	}

	VotesByPlayerSlotTag.Reset();
}

void UARFactionVotingSubsystem::GetCurrentVotes(TArray<FARFactionVoteEntry>& OutVotes) const
{
	OutVotes.Reset();
	OutVotes.Reserve(VotesByPlayerSlotTag.Num());

	for (const TPair<FGameplayTag, FGameplayTag>& Pair : VotesByPlayerSlotTag)
	{
		if (!Pair.Key.IsValid() || !Pair.Value.IsValid())
		{
			continue;
		}

		FARFactionVoteEntry& Entry = OutVotes.AddDefaulted_GetRef();
		Entry.PlayerSlotTag = Pair.Key;
		Entry.VotedFactionTag = Pair.Value;
	}

	OutVotes.Sort([](const FARFactionVoteEntry& A, const FARFactionVoteEntry& B)
	{
		return A.PlayerSlotTag.ToString() < B.PlayerSlotTag.ToString();
	});
}

bool UARFactionVotingSubsystem::FinalizeElection(
	FGameplayTag& OutWinnerFactionTag,
	FGameplayTagContainer& OutWinnerEffectTags,
	const bool bApplyWinnerToGameStateAndSave,
	const bool bClearVotesAfterFinalize)
{
	OutWinnerFactionTag = FGameplayTag();
	OutWinnerEffectTags.Reset();

	if (!EnsureAuthorityWorld(TEXT("FinalizeElection")))
	{
		return false;
	}

	TArray<FARFactionVotingCandidate> Candidates;
	if (!BuildCandidateList(Candidates) || Candidates.IsEmpty())
	{
		return false;
	}

	if (!HasAnySubmittedVotes())
	{
		UE_LOG(ARLog, Verbose, TEXT("[FactionVoting] Election finalize skipped: no submitted votes."));
		return false;
	}

	const FARFactionVotingCandidate* Winner = nullptr;
	int32 WinnerVoteCount = INDEX_NONE;
	for (const FARFactionVotingCandidate& Candidate : Candidates)
	{
		const int32 VoteCount = CountVotesForFaction(Candidate.FactionTag);
		if (!Winner)
		{
			Winner = &Candidate;
			WinnerVoteCount = VoteCount;
			continue;
		}

		const bool bBetterVoteCount = VoteCount > WinnerVoteCount;
		const bool bSameVotes = VoteCount == WinnerVoteCount;
		const bool bBetterPopularity = bSameVotes && Candidate.EffectivePopularity > Winner->EffectivePopularity;
		const bool bSamePopularity = bSameVotes && FMath::IsNearlyEqual(Candidate.EffectivePopularity, Winner->EffectivePopularity);
		const bool bBetterPriority = bSamePopularity && Candidate.CandidatePriority > Winner->CandidatePriority;
		const bool bSamePriority = bSamePopularity && Candidate.CandidatePriority == Winner->CandidatePriority;
		const bool bLexicalFallback = bSamePriority && Candidate.FactionTag.ToString() < Winner->FactionTag.ToString();

		if (bBetterVoteCount || bBetterPopularity || bBetterPriority || bLexicalFallback)
		{
			Winner = &Candidate;
			WinnerVoteCount = VoteCount;
		}
	}

	if (!Winner)
	{
		return false;
	}

	OutWinnerFactionTag = Winner->FactionTag;
	OutWinnerEffectTags = Winner->ElectedEffectTags;

	if (bApplyWinnerToGameStateAndSave && !TryApplyWinnerToGame(OutWinnerFactionTag, OutWinnerEffectTags))
	{
		return false;
	}

	OnFactionElectionFinalized.Broadcast(OutWinnerFactionTag, OutWinnerEffectTags, FMath::Max(0, WinnerVoteCount));

	const UARFactionVotingSettings* Settings = GetDefault<UARFactionVotingSettings>();
	const bool bSettingsAllowClear = Settings ? Settings->bClearVotesAfterElection : true;
	if (bClearVotesAfterFinalize && bSettingsAllowClear)
	{
		VotesByPlayerSlotTag.Reset();
	}

	return true;
}

bool UARFactionVotingSubsystem::EnsureAuthorityWorld(const TCHAR* Context) const
{
	if (!IsAuthorityWorld_Voting(GetWorld()))
	{
		UE_LOG(ARLog, Verbose, TEXT("[FactionVoting] %s requires authority world."), Context ? Context : TEXT("Unknown"));
		return false;
	}

	return true;
}

UParleyFactionSubsystem* UARFactionVotingSubsystem::ResolveParleyFactionSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UParleyFactionSubsystem>() : nullptr;
}

UARSaveSubsystem* UARFactionVotingSubsystem::ResolveSaveSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
}

AARGameStateBase* UARFactionVotingSubsystem::ResolveGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<AARGameStateBase>() : nullptr;
}

int32 UARFactionVotingSubsystem::ResolveCurrentFactionClout() const
{
	const UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	return SaveSubsystem ? FMath::Max(0, SaveSubsystem->GetFactionClout()) : 0;
}

int32 UARFactionVotingSubsystem::ResolveDesiredCandidateCount(const int32 FactionClout) const
{
	const UARFactionVotingSettings* Settings = GetDefault<UARFactionVotingSettings>();
	if (!Settings)
	{
		return 2;
	}

	const int32 BaseCount = FMath::Max(1, Settings->MinCandidateCount);
	const int32 MaxCount = FMath::Max(BaseCount, Settings->MaxCandidateCount);
	const int32 CloutPerAdditional = FMath::Max(1, Settings->CloutPerAdditionalCandidate);
	const int32 AdditionalCount = FMath::Max(0, FactionClout) / CloutPerAdditional;
	return FMath::Clamp(BaseCount + AdditionalCount, BaseCount, MaxCount);
}

bool UARFactionVotingSubsystem::BuildCandidateList(TArray<FARFactionVotingCandidate>& OutCandidates) const
{
	OutCandidates.Reset();

	const int32 FactionClout = ResolveCurrentFactionClout();
	const int32 DesiredCount = ResolveDesiredCandidateCount(FactionClout);
	if (!BuildCandidatesFromVotingDataTable(OutCandidates, FactionClout))
	{
		UE_LOG(ARLog, Verbose, TEXT("[FactionVoting] BuildCandidateList failed: no eligible candidates from VotingDefinitionDataTable."));
		return false;
	}

	if (OutCandidates.IsEmpty())
	{
		return false;
	}

	SortAndTrimCandidates(OutCandidates, DesiredCount);
	return OutCandidates.Num() > 0;
}

bool UARFactionVotingSubsystem::BuildCandidatesFromVotingDataTable(TArray<FARFactionVotingCandidate>& OutCandidates, const int32 FactionClout) const
{
	const UARFactionVotingSettings* Settings = GetDefault<UARFactionVotingSettings>();
	if (!Settings || Settings->VotingDefinitionDataTable.IsNull())
	{
		UE_LOG(ARLog, Verbose, TEXT("[FactionVoting] VotingDefinitionDataTable is not configured."));
		return false;
	}

	UDataTable* VotingTable = Settings->VotingDefinitionDataTable.LoadSynchronous();
	if (!VotingTable)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[FactionVoting] VotingDefinitionDataTable failed to load from '%s'."),
			*Settings->VotingDefinitionDataTable.ToSoftObjectPath().ToString());
		return false;
	}

	TArray<FARFactionVotingDefinitionRow*> Rows;
	VotingTable->GetAllRows(TEXT("ARFactionVotingSubsystem::BuildCandidatesFromVotingDataTable"), Rows);
	if (Rows.IsEmpty())
	{
		return false;
	}

	for (const FARFactionVotingDefinitionRow* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}

		FARFactionVotingCandidate Candidate;
		if (TryResolveCandidateFromVotingRow(*Row, FactionClout, Candidate))
		{
			OutCandidates.Add(Candidate);
		}
	}

	return OutCandidates.Num() > 0;
}

void UARFactionVotingSubsystem::SortAndTrimCandidates(TArray<FARFactionVotingCandidate>& Candidates, const int32 DesiredCount) const
{
	Candidates.Sort([](const FARFactionVotingCandidate& A, const FARFactionVotingCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.EffectivePopularity, B.EffectivePopularity))
		{
			return A.EffectivePopularity > B.EffectivePopularity;
		}

		if (A.CandidatePriority != B.CandidatePriority)
		{
			return A.CandidatePriority > B.CandidatePriority;
		}

		return A.FactionTag.ToString() < B.FactionTag.ToString();
	});

	Candidates.SetNum(FMath::Min(FMath::Max(1, DesiredCount), Candidates.Num()));
}

bool UARFactionVotingSubsystem::TryResolveCandidateFromVotingRow(
	const FARFactionVotingDefinitionRow& Row,
	const int32 FactionClout,
	FARFactionVotingCandidate& OutCandidate) const
{
	if (!Row.bEnabled || !Row.FactionTag.IsValid())
	{
		return false;
	}

	if (FactionClout < FMath::Max(0, Row.MinRequiredClout))
	{
		return false;
	}

	if (Row.MaxAllowedClout >= 0 && FactionClout > Row.MaxAllowedClout)
	{
		return false;
	}

	UParleyFactionSubsystem* FactionSubsystem = ResolveParleyFactionSubsystem();
	if (!FactionSubsystem)
	{
		return false;
	}

	FARFactionDefinitionRow Definition;
	if (!FactionSubsystem->GetFactionDefinition(Row.FactionTag, Definition))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[FactionVoting] Skipping voting row for unknown faction '%s'."),
			*Row.FactionTag.ToString());
		return false;
	}

	OutCandidate.FactionTag = Row.FactionTag;
	OutCandidate.CandidatePriority = FMath::Max(0, Row.CandidatePriority);
	OutCandidate.EffectivePopularity = FactionSubsystem->GetEffectiveFactionPopularity(Row.FactionTag);
	OutCandidate.ElectedEffectTags = Row.ElectedEffectTags.IsEmpty() ? Definition.EffectTags : Row.ElectedEffectTags;
	return true;
}

int32 UARFactionVotingSubsystem::CountVotesForFaction(const FGameplayTag FactionTag) const
{
	if (!FactionTag.IsValid())
	{
		return 0;
	}

	int32 VoteCount = 0;
	for (const TPair<FGameplayTag, FGameplayTag>& Pair : VotesByPlayerSlotTag)
	{
		if (Pair.Value.MatchesTagExact(FactionTag))
		{
			++VoteCount;
		}
	}

	return VoteCount;
}

bool UARFactionVotingSubsystem::TryApplyWinnerToGame(const FGameplayTag WinnerFactionTag, const FGameplayTagContainer& WinnerEffectTags) const
{
	AARGameStateBase* GameState = ResolveGameState();
	if (!GameState)
	{
		UE_LOG(ARLog, Warning, TEXT("[FactionVoting] Cannot apply winner '%s': AARGameStateBase unavailable."), *WinnerFactionTag.ToString());
		return false;
	}

	GameState->SetActiveFactionTagFromSave(WinnerFactionTag);
	GameState->SetActiveFactionEffectTagsFromSave(WinnerEffectTags);

	UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	if (SaveSubsystem)
	{
		if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
		{
			SaveGame->ActiveFactionTag = WinnerFactionTag;
			SaveGame->ActiveFactionEffectTags = WinnerEffectTags;
		}

		SaveSubsystem->MarkSaveDirty();
	}

	UE_LOG(
		ARLog,
		Log,
		TEXT("[FactionVoting] Applied election winner '%s' with %d effect tags."),
		*WinnerFactionTag.ToString(),
		WinnerEffectTags.Num());
	return true;
}
