#include "ARScrapyardGameState.h"

#include "ARGameStateModeStructs.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffSubsystem.h"
#include "ARScrapyardCarryItemBase.h"
#include "ARScrapyardExitZoneActor.h"
#include "ARSaveSubsystem.h"
#include "ARShopCarryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "StructUtils/InstancedStruct.h"
#include "TagContentResolverSubsystem.h"

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

	const float Elapsed = GetServerWorldTimeSeconds() - ScrapyardRunStartServerTime;
	return FMath::Max(0.0f, ScrapyardRunDurationSeconds - Elapsed);
}

void AARScrapyardGameState::StartScrapyardRun(float RunDurationSeconds, int32 InRunSeed)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bOldRunActive = bScrapyardRunActive;
	bScrapyardRunActive = true;
	ScrapyardRunDurationSeconds = FMath::Max(1.0f, RunDurationSeconds);
	ScrapyardRunStartServerTime = GetServerWorldTimeSeconds();
	ScrapyardRunSeed = (InRunSeed != 0) ? InRunSeed : FMath::Rand();
	ReservedCostByItem.Reset();
	LastBroadcastWholeRemainingSeconds = INDEX_NONE;

	RefreshExtractionSummary(true);
	OnRep_ScrapyardRunActive(bOldRunActive);
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
	for (const TPair<TWeakObjectPtr<AARScrapyardCarryItemBase>, int32>& Pair : ReservedCostByItem)
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

	auto GrantKeptByRewardType = [this, &KeptCandidates, &GrantedRewards](const EARScrapyardRewardType RewardTypeFilter)
	{
		for (const FScrapyardExtractionCandidate& Candidate : KeptCandidates)
		{
			FARScrapyardItemDefRow ItemDef;
			if (!ResolveItemDefinitionForTag(Candidate.ItemTag, ItemDef))
			{
				continue;
			}

			if (ItemDef.RewardType != RewardTypeFilter)
			{
				continue;
			}

			FARScrapyardRewardGrant RewardGrant;
			if (GrantRewardForCandidate(Candidate, RewardGrant))
			{
				GrantedRewards.Add(MoveTemp(RewardGrant));
			}
		}
	};

	// Reward order is deterministic: unlocks first, then consumables, so unlock-gated
	// routing (for example energy drink storage) is consistent regardless of item order.
	GrantKeptByRewardType(EARScrapyardRewardType::LicenseUnlock);
	GrantKeptByRewardType(EARScrapyardRewardType::EnergyDrink);

	for (const FScrapyardExtractionCandidate& Candidate : KeptCandidates)
	{
		FARScrapyardItemDefRow ItemDef;
		if (!ResolveItemDefinitionForTag(Candidate.ItemTag, ItemDef))
		{
			continue;
		}

		if (ItemDef.RewardType == EARScrapyardRewardType::None)
		{
			FARScrapyardRewardGrant RewardGrant;
			if (GrantRewardForCandidate(Candidate, RewardGrant))
			{
				GrantedRewards.Add(MoveTemp(RewardGrant));
			}
		}
	}

	for (const FScrapyardExtractionCandidate& Candidate : KeptCandidates)
	{
		CleanupCandidateActor(Candidate);
	}
	for (const FScrapyardExtractionCandidate& Candidate : TrimmedCandidates)
	{
		CleanupCandidateActor(Candidate);
	}

	ReservedCostByItem.Reset();
	SetScrapyardSharedScrap(0);

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

	if (!FinalizeScrapyardRun())
	{
		return false;
	}

	const FString TravelURL = InShopTravelURL.IsEmpty() ? DefaultShopTravelURL : InShopTravelURL;
	if (TravelURL.IsEmpty())
	{
		return true;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization could not travel: SaveSubsystem missing."));
		return false;
	}

	if (!SaveSubsystem->RequestServerTravel(TravelURL, true, false, false, true))
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard] Finalization travel failed for URL '%s'."), *TravelURL);
		return false;
	}

	return true;
}

bool AARScrapyardGameState::ReserveScrapForItem(AARScrapyardCarryItemBase* ItemActor, int32& OutReservedCost)
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

bool AARScrapyardGameState::RefundScrapForItem(AARScrapyardCarryItemBase* ItemActor, int32& OutRefundCost)
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

bool AARScrapyardGameState::ResolveItemDefinitionForActor(AARScrapyardCarryItemBase* ItemActor, FARScrapyardItemDefRow& OutDef) const
{
	OutDef = FARScrapyardItemDefRow();
	if (!ItemActor)
	{
		return false;
	}

	return ResolveItemDefinitionForTag(ItemActor->GetScrapyardItemTag(), OutDef);
}

int32 AARScrapyardGameState::ResolveItemCostForActor(AARScrapyardCarryItemBase* ItemActor) const
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

bool AARScrapyardGameState::IsItemReservedForExtraction(AARScrapyardCarryItemBase* ItemActor) const
{
	return ItemActor && ReservedCostByItem.Contains(ItemActor);
}

bool AARScrapyardGameState::TryGetReservedScrapForItem(AARScrapyardCarryItemBase* ItemActor, int32& OutReservedCost) const
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
		StartScrapyardRun(DefaultRunDurationSeconds, 0);
	}
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

	PruneInvalidReservedItems();
	if (bScrapyardRunActive && GetScrapyardRunRemainingSeconds() <= 0.0f)
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
	DOREPLIFETIME(AARScrapyardGameState, ExtractionSummary);
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
}

void AARScrapyardGameState::RefreshExtractionSummary(bool bBroadcast)
{
	FARScrapyardExtractionSummary NewSummary;
	NewSummary.CurrentScrap = GetScrap();

	int32 ReservedTotal = 0;
	for (const TPair<TWeakObjectPtr<AARScrapyardCarryItemBase>, int32>& Pair : ReservedCostByItem)
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

	TSet<TWeakObjectPtr<AARScrapyardCarryItemBase>> SeenItems;
	for (const TWeakObjectPtr<AARScrapyardExitZoneActor>& ExitZoneWeak : RegisteredExitZones)
	{
		AARScrapyardExitZoneActor* ExitZone = ExitZoneWeak.Get();
		if (!ExitZone)
		{
			continue;
		}

		for (AARScrapyardCarryItemBase* DepositedItem : ExitZone->GetDepositedItems())
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

		TArray<AARScrapyardCarryItemBase*> HeldItemsInZone;
		ExitZone->GatherHeldItemsInZone(HeldItemsInZone);
		for (AARScrapyardCarryItemBase* HeldItem : HeldItemsInZone)
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

	if (!ItemTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagContentResolverSubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagContentResolverSubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct ResolvedRow;
	FString ResolveError;
	if (!Resolver->TryResolveRowForTag(ItemTag, ResolvedRow, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Scrapyard] Failed to resolve item row for '%s': %s"),
			*ItemTag.ToString(),
			*ResolveError);
		return false;
	}

	const FARScrapyardItemDefRow* TypedRow = ResolvedRow.GetPtr<FARScrapyardItemDefRow>();
	if (!TypedRow)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Scrapyard] Item row for '%s' was not FARScrapyardItemDefRow."),
			*ItemTag.ToString());
		return false;
	}

	OutDef = *TypedRow;
	return true;
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

	default:
		return false;
	}
}

void AARScrapyardGameState::CleanupCandidateActor(const FScrapyardExtractionCandidate& Candidate)
{
	if (AARScrapyardCarryItemBase* ItemActor = Candidate.ItemActor.Get())
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
