#include "ARScrapyardGameState.h"

#include "AREconomySettings.h"
#include "ARGameStateModeStructs.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffTypes.h"
#include "ARRunBuffSubsystem.h"
#include "ARCarryItemBase.h"
#include "ARScrapyardExitZoneActor.h"
#include "ARSaveSubsystem.h"
#include "ARTravelSubsystem.h"
#include "ARSaveGame.h"
#include "ARShopCarryComponent.h"
#include "ARGameModeBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AARScrapyardGameState::AARScrapyardGameState()
{
	ClassStateStruct = FARScrapyardGameStateData::StaticStruct();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
}

UScriptStruct* AARScrapyardGameState::GetStateStruct_Implementation() const
{
	return ClassStateStruct ? ClassStateStruct.Get() : FARScrapyardGameStateData::StaticStruct();
}

float AARScrapyardGameState::GetScrapyardRunRemainingSeconds() const
{
	if (!bScrapyardRunActive)
	{
		return 0.0f;
	}

	const float Elapsed = ResolveScrapyardRunElapsedSeconds();
	return FMath::Max(0.0f, ScrapyardRunDurationSeconds - Elapsed);
}

float AARScrapyardGameState::ResolveScrapyardRunElapsedSeconds() const
{
	float Elapsed = GetServerWorldTimeSeconds() - ScrapyardRunStartServerTime;
	Elapsed -= ScrapyardRunAccumulatedPauseSeconds;
	if (bScrapyardRunTimerPaused && ScrapyardRunPauseStartServerTime > 0.0f)
	{
		Elapsed -= FMath::Max(0.0f, GetServerWorldTimeSeconds() - ScrapyardRunPauseStartServerTime);
	}

	return FMath::Max(0.0f, Elapsed);
}

void AARScrapyardGameState::StartScrapyardRun(float RunDurationSeconds, int32 InRunSeed)
{
	if (!HasAuthority())
	{
		return;
	}

	const UAREconomySettings* EconomySettings = GetDefault<UAREconomySettings>();
	const bool bOldRunActive = bScrapyardRunActive;
	bScrapyardRunActive = true;
	if (RunDurationSeconds <= 0.0f && EconomySettings)
	{
		RunDurationSeconds = EconomySettings->DefaultScrapyardDurationSeconds;
	}
	ScrapyardRunDurationSeconds = FMath::Max(1.0f, RunDurationSeconds);
	ScrapyardRunStartServerTime = GetServerWorldTimeSeconds();
	ScrapyardRunSeed = (InRunSeed != 0)
		? InRunSeed
		: static_cast<int32>(HashCombine(GetTypeHash(FMath::Rand()), GetTypeHash(EconomySettings ? EconomySettings->ScrapyardSpawnSeedSalt : 1337)));
	bScrapyardRunTimerPaused = false;
	ScrapyardRunPauseStartServerTime = 0.0f;
	ScrapyardRunAccumulatedPauseSeconds = 0.0f;
	ReservedCostByItem.Reset();
	LastBroadcastWholeRemainingSeconds = INDEX_NONE;
	SetScrapyardSharedScrap(GetScrap() + GetRunLedgerScrap());

	RefreshExtractionSummary(true);
	OnRep_ScrapyardRunActive(bOldRunActive);
	OnRep_ScrapyardRunTimerPaused(false);
	BroadcastTimerIfNeeded(true);
	ForceNetUpdate();
}

void AARScrapyardGameState::AddScrapyardTime(float AddedSeconds)
{
	if (!HasAuthority() || !bScrapyardRunActive)
	{
		return;
	}

	if (AddedSeconds <= 0.0f)
	{
		return;
	}

	ScrapyardRunDurationSeconds = FMath::Max(1.0f, ScrapyardRunDurationSeconds + AddedSeconds);
	RefreshExtractionSummary(true);
	BroadcastTimerIfNeeded(true);
	ForceNetUpdate();
}

void AARScrapyardGameState::SetScrapyardRunTimerPaused(const bool bPaused)
{
	if (!HasAuthority() || !bScrapyardRunActive)
	{
		return;
	}

	if (bScrapyardRunTimerPaused == bPaused)
	{
		return;
	}

	const bool bOldPaused = bScrapyardRunTimerPaused;
	const float Now = GetServerWorldTimeSeconds();
	if (bPaused)
	{
		ScrapyardRunPauseStartServerTime = Now;
	}
	else if (ScrapyardRunPauseStartServerTime > 0.0f)
	{
		ScrapyardRunAccumulatedPauseSeconds += FMath::Max(0.0f, Now - ScrapyardRunPauseStartServerTime);
		ScrapyardRunPauseStartServerTime = 0.0f;
	}

	bScrapyardRunTimerPaused = bPaused;
	RefreshExtractionSummary(true);
	OnRep_ScrapyardRunTimerPaused(bOldPaused);
	BroadcastTimerIfNeeded(true);
	ForceNetUpdate();
}

bool AARScrapyardGameState::FinalizeScrapyardRun()
{
	if (!HasAuthority())
	{
		return false;
	}

	PruneInvalidReservedItems();

	TArray<FScrapyardExtractionCandidate> Candidates;
	BuildExtractionCandidates(Candidates);

	int32 ReservedTotal = 0;
	for (const TPair<TWeakObjectPtr<AARCarryItemBase>, int32>& Pair : ReservedCostByItem)
	{
		ReservedTotal += FMath::Max(0, Pair.Value);
	}
	const int32 InitialScrapBudget = GetScrap() + ReservedTotal;
	int32 CurrentTotalCost = 0;
	for (const FScrapyardExtractionCandidate& Candidate : Candidates)
	{
		CurrentTotalCost += FMath::Max(0, Candidate.ScrapCost);
	}

	TArray<FScrapyardExtractionCandidate> KeptCandidates = Candidates;
	TArray<FScrapyardExtractionCandidate> TrimmedCandidates;

	if (CurrentTotalCost > InitialScrapBudget)
	{
		FRandomStream TrimRng(ScrapyardRunSeed != 0 ? ScrapyardRunSeed : 1337);
		while (CurrentTotalCost > InitialScrapBudget && KeptCandidates.Num() > 0)
		{
			const int32 CandidateIndex = TrimRng.RandRange(0, KeptCandidates.Num() - 1);
			const FScrapyardExtractionCandidate Trimmed = KeptCandidates[CandidateIndex];
			CurrentTotalCost -= FMath::Max(0, Trimmed.ScrapCost);
			TrimmedCandidates.Add(Trimmed);
			KeptCandidates.RemoveAtSwap(CandidateIndex, 1, EAllowShrinking::No);
		}
	}

	TArray<FARScrapyardRewardGrant> GrantedRewards;
	GrantedRewards.Reserve(KeptCandidates.Num());
	TArray<FScrapyardExtractionCandidate> SuccessfulKeptCandidates;
	SuccessfulKeptCandidates.Reserve(KeptCandidates.Num());
	TArray<FScrapyardExtractionCandidate> FailedRewardCandidates;
	FailedRewardCandidates.Reserve(KeptCandidates.Num());

	struct FResolvedCandidateState
	{
		FScrapyardExtractionCandidate Candidate;
		FARScrapyardItemDefRow ItemDef;
		bool bHasValidDefinition = false;
		bool bProcessed = false;
	};

	TArray<FResolvedCandidateState> CandidateStates;
	CandidateStates.Reserve(KeptCandidates.Num());
	for (const FScrapyardExtractionCandidate& Candidate : KeptCandidates)
	{
		FResolvedCandidateState& State = CandidateStates.AddDefaulted_GetRef();
		State.Candidate = Candidate;
		State.bHasValidDefinition = ResolveItemDefinitionForTag(Candidate.ItemTag, State.ItemDef);
	}

	auto ProcessCandidatesByRewardType = [this, &CandidateStates, &GrantedRewards, &SuccessfulKeptCandidates, &FailedRewardCandidates](const EARScrapyardRewardType RewardTypeFilter)
	{
		for (FResolvedCandidateState& State : CandidateStates)
		{
			if (State.bProcessed)
			{
				continue;
			}

			if (!State.bHasValidDefinition)
			{
				FailedRewardCandidates.Add(State.Candidate);
				State.bProcessed = true;
				continue;
			}

			if (State.ItemDef.RewardType != RewardTypeFilter)
			{
				continue;
			}

			State.bProcessed = true;
			if (RewardTypeFilter == EARScrapyardRewardType::None)
			{
				SuccessfulKeptCandidates.Add(State.Candidate);
				continue;
			}

			FARScrapyardRewardGrant RewardGrant;
			if (GrantRewardForCandidate(State.Candidate, RewardGrant))
			{
				GrantedRewards.Add(MoveTemp(RewardGrant));
				SuccessfulKeptCandidates.Add(State.Candidate);
			}
			else
			{
				FailedRewardCandidates.Add(State.Candidate);
			}
		}
	};

	// Reward order is deterministic: unlocks first, then consumables, so unlock-gated
	// routing (for example energy drink storage) is consistent regardless of item order.
	ProcessCandidatesByRewardType(EARScrapyardRewardType::LicenseUnlock);
	ProcessCandidatesByRewardType(EARScrapyardRewardType::UnlockTag);
	ProcessCandidatesByRewardType(EARScrapyardRewardType::ProgressionTag);
	ProcessCandidatesByRewardType(EARScrapyardRewardType::EnergyDrink);
	ProcessCandidatesByRewardType(EARScrapyardRewardType::None);

	if (FailedRewardCandidates.Num() > 0)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Scrapyard] Reclassified %d kept extraction candidates as trimmed due to reward delivery failures."),
			FailedRewardCandidates.Num());
	}

	KeptCandidates = MoveTemp(SuccessfulKeptCandidates);
	TrimmedCandidates.Append(FailedRewardCandidates);

	for (const FScrapyardExtractionCandidate& Candidate : KeptCandidates)
	{
		CleanupCandidateActor(Candidate);
	}
	for (const FScrapyardExtractionCandidate& Candidate : TrimmedCandidates)
	{
		CleanupCandidateActor(Candidate);
	}

	int32 PurchasedCostTotal = 0;
	for (const FScrapyardExtractionCandidate& Candidate : KeptCandidates)
	{
		PurchasedCostTotal += FMath::Max(0, Candidate.ScrapCost);
	}

	int32 TrimmedCostTotal = 0;
	for (const FScrapyardExtractionCandidate& Candidate : TrimmedCandidates)
	{
		TrimmedCostTotal += FMath::Max(0, Candidate.ScrapCost);
	}

	const int32 LeftoverScrap = FMath::Max(0, InitialScrapBudget - PurchasedCostTotal);
	const int32 WastedScrap = TrimmedCostTotal;

	ReservedCostByItem.Reset();
	SetRunLedgerScrap(LeftoverScrap);
	SetScrapyardSharedScrap(0);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* CurrentSave = SaveSubsystem->GetCurrentSaveGame())
			{
				CurrentSave->ShopTransientCarryables.Reset();
				CurrentSave->bClearShopTransientCarryablesOnNextShopLoad = true;
				SaveSubsystem->MarkSaveDirty();
			}
		}
	}

	const bool bOldRunActive = bScrapyardRunActive;
	bScrapyardRunActive = false;

	FARScrapyardExtractionSummary NewSummary;
	NewSummary.CurrentScrap = GetScrap();
	NewSummary.ReservedCostTotal = 0;
	NewSummary.ReservedItemCount = 0;
	NewSummary.RemainingTimeSeconds = 0.0f;
	NewSummary.bRunActive = false;
	NewSummary.KeptItemCount = KeptCandidates.Num();
	NewSummary.TrimmedItemCount = TrimmedCandidates.Num();
	NewSummary.LeftoverScrap = LeftoverScrap;
	NewSummary.TrimmedScrap = TrimmedCostTotal;
	NewSummary.WastedScrap = WastedScrap;
	NewSummary.PurchasedItemCount = KeptCandidates.Num();
	NewSummary.DiscardedItemCount = TrimmedCandidates.Num();
	NewSummary.ConvertedMoney = 0;
	NewSummary.GrantedRewards = MoveTemp(GrantedRewards);

	const FARScrapyardExtractionSummary OldSummary = ExtractionSummary;
	ExtractionSummary = MoveTemp(NewSummary);
	OnRep_ExtractionSummary(OldSummary);
	OnRep_ScrapyardRunActive(bOldRunActive);
	BroadcastTimerIfNeeded(true);
	ForceNetUpdate();
	return true;
}

bool AARScrapyardGameState::FinalizeScrapyardRunAndTravelToShop(const FString& InShopTravelURL)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FString ShopTravelURL = InShopTravelURL.IsEmpty() ? DefaultShopTravelURL : InShopTravelURL;
	if (ShopTravelURL.IsEmpty())
	{
		return FinalizeScrapyardRun();
	}

	UGameInstance* GameInstance = GetGameInstance();
	UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization skipped: SaveSubsystem missing."));
		return false;
	}

	if (!FinalizeScrapyardRun())
	{
		return false;
	}

	FARSaveResult SaveResult;
	if (!SaveSubsystem->SaveCurrentGameUnthrottled(NAME_None, true, SaveResult))
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization save failed before travel: %s"), *SaveResult.Error);
		return false;
	}

	// Scrapyard finalization now commits a canonical save before travel so rewards/economy survive
	// transition-map crashes and immediate host quits after re-entry.
	FString FinalTravelURL = ShopTravelURL;
	if (const AARGameModeBase* GameMode = GetWorld() ? Cast<AARGameModeBase>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		FinalTravelURL = GameMode->BuildModeTravelURL(ShopTravelURL);
	}

	if (FinalTravelURL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization travel aborted: destination URL is empty."));
		return false;
	}

	UARTravelSubsystem* TravelSubsystem = GameInstance ? GameInstance->GetSubsystem<UARTravelSubsystem>() : nullptr;
	if (!TravelSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization travel failed: TravelSubsystem missing."));
		return false;
	}

	if (!TravelSubsystem->RequestServerTravel(FinalTravelURL, true, false, false, false))
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization travel failed for URL '%s'."), *FinalTravelURL);
		return false;
	}

	return true;
}

bool AARScrapyardGameState::ReserveScrapForItem(AARCarryItemBase* ItemActor, int32& OutReservedCost)
{
	OutReservedCost = 0;
	if (!HasAuthority() || !ItemActor)
	{
		return false;
	}

	PruneInvalidReservedItems();
	if (ReservedCostByItem.Contains(ItemActor))
	{
		return false;
	}

	const int32 ItemCost = ResolveItemCostForActor(ItemActor);
	ReservedCostByItem.Add(ItemActor, ItemCost);
	SetScrapyardSharedScrap(GetScrap() - ItemCost);
	OutReservedCost = ItemCost;
	RefreshExtractionSummary(true);
	return true;
}

bool AARScrapyardGameState::RefundScrapForItem(AARCarryItemBase* ItemActor, int32& OutRefundCost)
{
	OutRefundCost = 0;
	if (!HasAuthority() || !ItemActor)
	{
		return false;
	}

	int32* FoundReservedCost = ReservedCostByItem.Find(ItemActor);
	if (!FoundReservedCost)
	{
		return false;
	}

	const int32 ItemCost = FMath::Max(0, *FoundReservedCost);
	ReservedCostByItem.Remove(ItemActor);
	SetScrapyardSharedScrap(GetScrap() + ItemCost);
	OutRefundCost = ItemCost;
	RefreshExtractionSummary(true);
	return true;
}

bool AARScrapyardGameState::ResolveItemDefinitionForActor(AARCarryItemBase* ItemActor, FARScrapyardItemDefRow& OutDef) const
{
	OutDef = FARScrapyardItemDefRow();
	if (!ItemActor)
	{
		return false;
	}

	return ResolveItemDefinitionForTag(ItemActor->GetScrapyardItemTag(), OutDef);
}

int32 AARScrapyardGameState::ResolveItemCostForActor(AARCarryItemBase* ItemActor) const
{
	if (!ItemActor)
	{
		return 0;
	}

	FARScrapyardItemDefRow ItemDef;
	if (ResolveItemDefinitionForActor(ItemActor, ItemDef))
	{
		return FMath::Max(0, ItemDef.ScrapCost);
	}

	return FMath::Max(0, ItemActor->GetFallbackScrapCost());
}

bool AARScrapyardGameState::IsItemReservedForExtraction(AARCarryItemBase* ItemActor) const
{
	return ItemActor && ReservedCostByItem.Contains(ItemActor);
}

bool AARScrapyardGameState::TryGetReservedScrapForItem(AARCarryItemBase* ItemActor, int32& OutReservedCost) const
{
	OutReservedCost = 0;
	if (!ItemActor)
	{
		return false;
	}

	const int32* FoundCost = ReservedCostByItem.Find(ItemActor);
	if (!FoundCost)
	{
		return false;
	}

	OutReservedCost = FMath::Max(0, *FoundCost);
	return true;
}

void AARScrapyardGameState::RegisterExitZone(AARScrapyardExitZoneActor* ExitZone)
{
	if (!HasAuthority() || !ExitZone)
	{
		return;
	}

	RegisteredExitZones.AddUnique(ExitZone);
	RefreshExtractionSummary(true);
}

void AARScrapyardGameState::UnregisterExitZone(AARScrapyardExitZoneActor* ExitZone)
{
	if (!HasAuthority() || !ExitZone)
	{
		return;
	}

	RegisteredExitZones.RemoveSwap(ExitZone);
	RefreshExtractionSummary(true);
}

void AARScrapyardGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BindRunBuffSubsystem();
		StartScrapyardRun(DefaultRunDurationSeconds, 0);
		RefreshRunBuffStateSnapshot(true);
	}
}

void AARScrapyardGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindRunBuffSubsystem();
	Super::EndPlay(EndPlayReason);
}

void AARScrapyardGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	(void)DeltaSeconds;

	BroadcastTimerIfNeeded(false);
	if (!HasAuthority())
	{
		return;
	}

	if (bScrapyardRunActive)
	{
		SetScrapyardRunTimerPaused(IsEffectivePauseStateActive());
	}

	PruneInvalidReservedItems();
	if (bScrapyardRunActive && !bScrapyardRunTimerPaused && GetScrapyardRunRemainingSeconds() <= 0.0f)
	{
		FinalizeScrapyardRunAndTravelToShop(DefaultShopTravelURL);
	}
}

void AARScrapyardGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARScrapyardGameState, bScrapyardRunActive);
	DOREPLIFETIME(AARScrapyardGameState, ScrapyardRunStartServerTime);
	DOREPLIFETIME(AARScrapyardGameState, ScrapyardRunDurationSeconds);
	DOREPLIFETIME(AARScrapyardGameState, ScrapyardRunSeed);
	DOREPLIFETIME(AARScrapyardGameState, bScrapyardRunTimerPaused);
	DOREPLIFETIME(AARScrapyardGameState, ScrapyardRunPauseStartServerTime);
	DOREPLIFETIME(AARScrapyardGameState, ScrapyardRunAccumulatedPauseSeconds);
	DOREPLIFETIME(AARScrapyardGameState, ExtractionSummary);
	DOREPLIFETIME(AARScrapyardGameState, RunBuffStateSnapshot);
}

void AARScrapyardGameState::OnRep_ScrapyardRunActive(bool bOldRunActive)
{
	if (bOldRunActive != bScrapyardRunActive)
	{
		OnScrapyardRunActiveChanged.Broadcast(bScrapyardRunActive);
	}

	BroadcastTimerIfNeeded(true);
}

void AARScrapyardGameState::OnRep_ExtractionSummary(const FARScrapyardExtractionSummary& OldSummary)
{
	(void)OldSummary;
	OnScrapyardExtractionSummaryChanged.Broadcast(ExtractionSummary);
}

void AARScrapyardGameState::OnRep_ScrapyardRunTimerPaused(bool bOldPaused)
{
	if (bOldPaused != bScrapyardRunTimerPaused)
	{
		OnScrapyardRunTimerPausedChanged.Broadcast(bScrapyardRunTimerPaused);
	}
}

void AARScrapyardGameState::OnRep_RunBuffStateSnapshot(const FARRunBuffStateSnapshot& OldSnapshot)
{
	(void)OldSnapshot;
	OnScrapyardRunBuffSnapshotChanged.Broadcast(RunBuffStateSnapshot);
}

void AARScrapyardGameState::SetScrapyardSharedScrap(int32 NewScrapValue)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Scrap == NewScrapValue)
	{
		return;
	}

	const int32 OldScrap = Scrap;
	Scrap = NewScrapValue;
	OnRep_Scrap(OldScrap);
	ForceNetUpdate();
	if (UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
	{
		if (SaveSubsystem->GetCurrentSaveGame())
		{
			SaveSubsystem->MarkSaveDirty();
		}
	}
}

void AARScrapyardGameState::RefreshExtractionSummary(bool bBroadcast)
{
	FARScrapyardExtractionSummary NewSummary;
	NewSummary.CurrentScrap = GetScrap();

	int32 ReservedTotal = 0;
	for (const TPair<TWeakObjectPtr<AARCarryItemBase>, int32>& Pair : ReservedCostByItem)
	{
		ReservedTotal += FMath::Max(0, Pair.Value);
	}
	NewSummary.ReservedCostTotal = ReservedTotal;
	NewSummary.ReservedItemCount = ReservedCostByItem.Num();
	NewSummary.RemainingTimeSeconds = GetScrapyardRunRemainingSeconds();
	NewSummary.bRunActive = bScrapyardRunActive;
	if (!bScrapyardRunActive)
	{
		NewSummary.KeptItemCount = ExtractionSummary.KeptItemCount;
		NewSummary.TrimmedItemCount = ExtractionSummary.TrimmedItemCount;
		NewSummary.LeftoverScrap = ExtractionSummary.LeftoverScrap;
		NewSummary.TrimmedScrap = ExtractionSummary.TrimmedScrap;
		NewSummary.WastedScrap = ExtractionSummary.WastedScrap;
		NewSummary.PurchasedItemCount = ExtractionSummary.PurchasedItemCount;
		NewSummary.DiscardedItemCount = ExtractionSummary.DiscardedItemCount;
		NewSummary.ConvertedMoney = ExtractionSummary.ConvertedMoney;
		NewSummary.GrantedRewards = ExtractionSummary.GrantedRewards;
	}

	const FARScrapyardExtractionSummary OldSummary = ExtractionSummary;
	ExtractionSummary = MoveTemp(NewSummary);
	if (bBroadcast)
	{
		OnRep_ExtractionSummary(OldSummary);
	}
}

void AARScrapyardGameState::PruneInvalidReservedItems()
{
	int32 RefundFromInvalidEntries = 0;
	for (auto It = ReservedCostByItem.CreateIterator(); It; ++It)
	{
		if (It->Key.IsValid())
		{
			continue;
		}

		RefundFromInvalidEntries += FMath::Max(0, It->Value);
		It.RemoveCurrent();
	}

	if (RefundFromInvalidEntries > 0)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Scrapyard] Refunded %d scrap from invalid reserved entries."),
			RefundFromInvalidEntries);
		SetScrapyardSharedScrap(GetScrap() + RefundFromInvalidEntries);
		RefreshExtractionSummary(true);
	}
}

void AARScrapyardGameState::BroadcastTimerIfNeeded(bool bForceBroadcast)
{
	const int32 WholeSecondsRemaining = FMath::Max(0, FMath::CeilToInt(GetScrapyardRunRemainingSeconds()));
	if (bForceBroadcast || WholeSecondsRemaining != LastBroadcastWholeRemainingSeconds)
	{
		LastBroadcastWholeRemainingSeconds = WholeSecondsRemaining;
		OnScrapyardRunTimerChanged.Broadcast(GetScrapyardRunRemainingSeconds());
	}
}

void AARScrapyardGameState::BuildExtractionCandidates(TArray<FScrapyardExtractionCandidate>& OutCandidates) const
{
	OutCandidates.Reset();

	TSet<TWeakObjectPtr<AARCarryItemBase>> SeenItems;
	for (const TWeakObjectPtr<AARScrapyardExitZoneActor>& ExitZoneWeak : RegisteredExitZones)
	{
		AARScrapyardExitZoneActor* ExitZone = ExitZoneWeak.Get();
		if (!ExitZone)
		{
			continue;
		}

		for (AARCarryItemBase* DepositedItem : ExitZone->GetDepositedItems())
		{
			if (!DepositedItem || SeenItems.Contains(DepositedItem))
			{
				continue;
			}

			SeenItems.Add(DepositedItem);

			FScrapyardExtractionCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.ItemActor = DepositedItem;
			Candidate.ItemTag = DepositedItem->GetScrapyardItemTag();
			if (const int32* ReservedCost = ReservedCostByItem.Find(DepositedItem))
			{
				Candidate.ScrapCost = FMath::Max(0, *ReservedCost);
			}
			else
			{
				Candidate.ScrapCost = ResolveItemCostForActor(DepositedItem);
			}
		}

		TArray<AARCarryItemBase*> HeldItemsInZone;
		ExitZone->GatherHeldItemsInZone(HeldItemsInZone);
		for (AARCarryItemBase* HeldItem : HeldItemsInZone)
		{
			if (!HeldItem || SeenItems.Contains(HeldItem))
			{
				continue;
			}

			SeenItems.Add(HeldItem);

			FScrapyardExtractionCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.ItemActor = HeldItem;
			Candidate.ItemTag = HeldItem->GetScrapyardItemTag();
			if (const int32* ReservedCost = ReservedCostByItem.Find(HeldItem))
			{
				Candidate.ScrapCost = FMath::Max(0, *ReservedCost);
			}
			else
			{
				Candidate.ScrapCost = ResolveItemCostForActor(HeldItem);
			}
		}
	}

	OutCandidates.Sort([](const FScrapyardExtractionCandidate& A, const FScrapyardExtractionCandidate& B)
	{
		const FString NameA = GetNameSafe(A.ItemActor.Get());
		const FString NameB = GetNameSafe(B.ItemActor.Get());
		return NameA < NameB;
	});
}

bool AARScrapyardGameState::ResolveItemDefinitionForTag(const FGameplayTag ItemTag, FARScrapyardItemDefRow& OutDef) const
{
	OutDef = FARScrapyardItemDefRow();

	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	return ItemDefinitions && ItemDefinitions->ResolveItemDefinition(ItemTag, OutDef);
}

bool AARScrapyardGameState::GrantRewardForCandidate(const FScrapyardExtractionCandidate& Candidate, FARScrapyardRewardGrant& OutGrantedReward)
{
	OutGrantedReward = FARScrapyardRewardGrant();
	OutGrantedReward.ItemTag = Candidate.ItemTag;

	FARScrapyardItemDefRow ItemDef;
	if (!ResolveItemDefinitionForTag(Candidate.ItemTag, ItemDef))
	{
		return false;
	}

	OutGrantedReward.RewardType = ItemDef.RewardType;
	switch (ItemDef.RewardType)
	{
	case EARScrapyardRewardType::LicenseUnlock:
	case EARScrapyardRewardType::UnlockTag:
		if (!ItemDef.LicenseUnlockTag.IsValid())
		{
			return false;
		}

		AddUnlockTag(ItemDef.LicenseUnlockTag);
		OutGrantedReward.LicenseUnlockTag = ItemDef.LicenseUnlockTag;
		return true;

	case EARScrapyardRewardType::EnergyDrink:
	{
		const FGameplayTag EnergyDrinkTag = ItemDef.EnergyDrinkTag.IsValid() ? ItemDef.EnergyDrinkTag : Candidate.ItemTag;
		if (!EnergyDrinkTag.IsValid())
		{
			return false;
		}

		UGameInstance* GameInstance = GetGameInstance();
		UARRunBuffSubsystem* RunBuffSubsystem = GameInstance ? GameInstance->GetSubsystem<UARRunBuffSubsystem>() : nullptr;
		if (!RunBuffSubsystem || !RunBuffSubsystem->AddExtractedEnergyDrink(EnergyDrinkTag, 1))
		{
			return false;
		}

		OutGrantedReward.EnergyDrinkTag = EnergyDrinkTag;
		return true;
	}

	case EARScrapyardRewardType::ProgressionTag:
	{
		if (!ItemDef.ProgressionRewardTag.IsValid())
		{
			return false;
		}

		UGameInstance* GameInstance = GetGameInstance();
		UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
		if (!SaveSubsystem || !SaveSubsystem->AddProgressionTag(ItemDef.ProgressionRewardTag))
		{
			return false;
		}

		OutGrantedReward.ProgressionRewardTag = ItemDef.ProgressionRewardTag;
		return true;
	}

	default:
		return ItemDef.RewardType == EARScrapyardRewardType::None;
	}
}

void AARScrapyardGameState::CleanupCandidateActor(const FScrapyardExtractionCandidate& Candidate)
{
	if (AARCarryItemBase* ItemActor = Candidate.ItemActor.Get())
	{
		ReservedCostByItem.Remove(ItemActor);

		// Clear any authoritative held reference before release so carry components
		// do not keep stale pointers after extraction finalization.
		for (AARPlayerStateBase* PlayerState : GetPlayerStates())
		{
			APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
			if (CarryComponent && CarryComponent->GetHeldActor() == ItemActor)
			{
				CarryComponent->ReleaseHeldActorForTransfer();
			}
		}

		ItemActor->ReleaseCarryItem();
	}
}

void AARScrapyardGameState::BindRunBuffSubsystem()
{
	if (!HasAuthority())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UARRunBuffSubsystem* RunBuffSubsystem = GameInstance ? GameInstance->GetSubsystem<UARRunBuffSubsystem>() : nullptr;
	if (BoundRunBuffSubsystem.Get() == RunBuffSubsystem)
	{
		return;
	}

	UnbindRunBuffSubsystem();
	BoundRunBuffSubsystem = RunBuffSubsystem;
	if (RunBuffSubsystem)
	{
		RunBuffSubsystem->OnRunBuffStateChanged.AddUniqueDynamic(this, &AARScrapyardGameState::HandleRunBuffStateChanged);
	}
}

void AARScrapyardGameState::UnbindRunBuffSubsystem()
{
	UARRunBuffSubsystem* RunBuffSubsystem = BoundRunBuffSubsystem.Get();
	if (RunBuffSubsystem)
	{
		RunBuffSubsystem->OnRunBuffStateChanged.RemoveDynamic(this, &AARScrapyardGameState::HandleRunBuffStateChanged);
	}

	BoundRunBuffSubsystem.Reset();
}

void AARScrapyardGameState::RefreshRunBuffStateSnapshot(const bool bBroadcast)
{
	if (!HasAuthority())
	{
		return;
	}

	FARRunBuffStateSnapshot NewSnapshot;
	if (UARRunBuffSubsystem* RunBuffSubsystem = BoundRunBuffSubsystem.Get())
	{
		NewSnapshot = RunBuffSubsystem->GetRunBuffStateSnapshot();
	}

	const FARRunBuffStateSnapshot OldSnapshot = RunBuffStateSnapshot;
	RunBuffStateSnapshot = MoveTemp(NewSnapshot);
	if (bBroadcast)
	{
		OnRep_RunBuffStateSnapshot(OldSnapshot);
	}
	ForceNetUpdate();
}

void AARScrapyardGameState::HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot)
{
	if (!HasAuthority())
	{
		return;
	}

	const FARRunBuffStateSnapshot OldSnapshot = RunBuffStateSnapshot;
	RunBuffStateSnapshot = Snapshot;
	OnRep_RunBuffStateSnapshot(OldSnapshot);
	ForceNetUpdate();
}
