#include "ARRunBuffSubsystem.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARScrapyardTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"
#include "TagContentResolverSubsystem.h"

namespace
{
	static bool IsAuthorityWorld_RunBuff(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		return World->GetNetMode() == NM_Standalone || World->GetAuthGameMode() != nullptr;
	}
}

void UARRunBuffSubsystem::Deinitialize()
{
	ResetRuntimeApplications();
	LastRotationWorld.Reset();
	LastRotationCycleId = INDEX_NONE;
	Super::Deinitialize();
}

FARRunBuffStateSnapshot UARRunBuffSubsystem::GetRunBuffStateSnapshot() const
{
	FARRunBuffStateSnapshot Snapshot;

	const UARSaveGame* SaveGame = ResolveSave();
	if (!SaveGame)
	{
		return Snapshot;
	}

	Snapshot.StoredEnergyDrinkStacks = SaveGame->StoredEnergyDrinkStacks;
	Snapshot.QueuedEnergyDrinkStacks = SaveGame->QueuedEnergyDrinkStacks;
	Snapshot.ActiveRunBuffPayloads = SaveGame->ActiveRunBuffPayloads;
	Snapshot.ActiveRunBuffCycleId = SaveGame->ActiveRunBuffCycleId;
	return Snapshot;
}

int32 UARRunBuffSubsystem::GetStoredEnergyDrinkCount(const FGameplayTag ItemTag) const
{
	const UARSaveGame* SaveGame = ResolveSave();
	return SaveGame ? GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag) : 0;
}

int32 UARRunBuffSubsystem::GetQueuedEnergyDrinkCount(const FGameplayTag ItemTag) const
{
	const UARSaveGame* SaveGame = ResolveSave();
	return SaveGame ? GetStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag) : 0;
}

bool UARRunBuffSubsystem::HasEnergyDrinkStorageUnlock() const
{
	const FGameplayTag StorageUnlockTag = ResolveEnergyDrinkStorageUnlockTag();
	const AARGameStateBase* GameState = ResolveGameState();
	return GameState && StorageUnlockTag.IsValid() && GameState->HasUnlockTag(StorageUnlockTag);
}

bool UARRunBuffSubsystem::AddExtractedEnergyDrink(const FGameplayTag ItemTag, int32 Count)
{
	if (!EnsureAuthorityWorld(TEXT("AddExtractedEnergyDrink")))
	{
		return false;
	}

	Count = FMath::Max(0, Count);
	if (!ItemTag.IsValid() || Count <= 0)
	{
		return false;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return false;
	}

	TArray<FARRunBuffItemStack>& TargetStacks = HasEnergyDrinkStorageUnlock()
		? SaveGame->StoredEnergyDrinkStacks
		: SaveGame->QueuedEnergyDrinkStacks;

	const int32 MaxStackCount = ResolveMaxStackCountForItem(ItemTag);
	const int32 CurrentCount = GetStackCount(TargetStacks, ItemTag);
	const int32 AvailableCapacity = FMath::Max(0, MaxStackCount - CurrentCount);
	if (AvailableCapacity <= 0)
	{
		return false;
	}

	const int32 CountToAdd = FMath::Min(Count, AvailableCapacity);
	if (UpsertStackCount(TargetStacks, ItemTag, CountToAdd) <= 0)
	{
		return false;
	}

	NormalizeStacks(TargetStacks);
	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::UseStoredEnergyDrink(const FGameplayTag ItemTag, int32 Count)
{
	if (!EnsureAuthorityWorld(TEXT("UseStoredEnergyDrink")))
	{
		return false;
	}

	Count = FMath::Max(0, Count);
	if (!ItemTag.IsValid() || Count <= 0)
	{
		return false;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return false;
	}

	if (GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag) < Count)
	{
		return false;
	}

	const int32 MaxQueuedCount = ResolveMaxStackCountForItem(ItemTag);
	const int32 QueuedCount = GetStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag);
	if (QueuedCount + Count > MaxQueuedCount)
	{
		return false;
	}

	UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, -Count);
	UpsertStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, Count);
	NormalizeStacks(SaveGame->StoredEnergyDrinkStacks);
	NormalizeStacks(SaveGame->QueuedEnergyDrinkStacks);
	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::QueueEnergyDrinkForNextRun(const FGameplayTag ItemTag, int32 Count)
{
	if (!EnsureAuthorityWorld(TEXT("QueueEnergyDrinkForNextRun")))
	{
		return false;
	}

	Count = FMath::Max(0, Count);
	if (!ItemTag.IsValid() || Count <= 0)
	{
		return false;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return false;
	}

	const int32 MaxQueuedCount = ResolveMaxStackCountForItem(ItemTag);
	const int32 CurrentQueuedCount = GetStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag);
	const int32 AvailableCapacity = FMath::Max(0, MaxQueuedCount - CurrentQueuedCount);
	if (AvailableCapacity <= 0)
	{
		return false;
	}

	const int32 CountToQueue = FMath::Min(Count, AvailableCapacity);
	if (UpsertStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, CountToQueue) <= 0)
	{
		return false;
	}

	NormalizeStacks(SaveGame->QueuedEnergyDrinkStacks);
	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::SellStoredEnergyDrink(const FGameplayTag ItemTag, int32 Count, int32& OutMoneyAwarded)
{
	OutMoneyAwarded = 0;
	if (!EnsureAuthorityWorld(TEXT("SellStoredEnergyDrink")))
	{
		return false;
	}

	Count = FMath::Max(0, Count);
	if (!ItemTag.IsValid() || Count <= 0)
	{
		return false;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return false;
	}

	if (GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag) < Count)
	{
		return false;
	}

	FARScrapyardItemDefRow ItemDef;
	if (!ResolveScrapyardItemDefinition(ItemTag, ItemDef))
	{
		return false;
	}

	const int32 SellValue = FMath::Max(0, ItemDef.SellMoneyValue);
	if (SellValue <= 0)
	{
		return false;
	}

	UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, -Count);
	NormalizeStacks(SaveGame->StoredEnergyDrinkStacks);

	OutMoneyAwarded = SellValue * Count;
	if (AARGameStateBase* GameState = ResolveGameState())
	{
		GameState->SetMoneyFromSave(GameState->GetMoney() + OutMoneyAwarded);
		SaveGame->Money = GameState->GetMoney();
	}
	else
	{
		SaveGame->Money = FMath::Max(0, SaveGame->Money + OutMoneyAwarded);
	}

	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::RotateRunBuffsAtInvaderInit()
{
	if (!EnsureAuthorityWorld(TEXT("RotateRunBuffsAtInvaderInit")))
	{
		return false;
	}

	UWorld* World = GetWorld();
	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!World || !SaveGame)
	{
		return false;
	}

	if (LastRotationWorld.Get() == World && LastRotationCycleId == SaveGame->ActiveRunBuffCycleId)
	{
		return true;
	}

	ResetRuntimeApplications();
	SaveGame->ActiveRunBuffPayloads.Reset();

	for (const FARRunBuffItemStack& QueuedStack : SaveGame->QueuedEnergyDrinkStacks)
	{
		if (!QueuedStack.ItemTag.IsValid() || QueuedStack.Count <= 0)
		{
			continue;
		}

		FARScrapyardItemDefRow ItemDef;
		if (!ResolveScrapyardItemDefinition(QueuedStack.ItemTag, ItemDef))
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[RunBuff] Invader-init rotation skipped unresolved queued energy drink '%s'."),
				*QueuedStack.ItemTag.ToString());
			continue;
		}

		FARRunBuffActivePayload Payload;
		Payload.ItemTag = ItemDef.EnergyDrinkTag.IsValid() ? ItemDef.EnergyDrinkTag : QueuedStack.ItemTag;
		Payload.AppliedCount = FMath::Clamp(QueuedStack.Count, 1, ResolveMaxStackCountForItem(QueuedStack.ItemTag));
		Payload.GameplayEffects = ItemDef.RunBuffGameplayEffects;
		Payload.GrantedTags = ItemDef.RunBuffGrantedTags;
		SaveGame->ActiveRunBuffPayloads.Add(MoveTemp(Payload));
	}

	SaveGame->QueuedEnergyDrinkStacks.Reset();
	SaveGame->ActiveRunBuffCycleId = FMath::Max(0, SaveGame->ActiveRunBuffCycleId) + 1;
	MarkSaveDirty();

	if (AARGameStateBase* GameState = ResolveGameState())
	{
		for (AARPlayerStateBase* PlayerState : GameState->GetPlayerStates())
		{
			if (PlayerState)
			{
				ApplyActiveRunBuffsToPlayerState(PlayerState);
			}
		}
	}

	LastRotationWorld = World;
	LastRotationCycleId = SaveGame->ActiveRunBuffCycleId;
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::ApplyActiveRunBuffsToPlayerState(AARPlayerStateBase* PlayerState)
{
	if (!EnsureAuthorityWorld(TEXT("ApplyActiveRunBuffsToPlayerState")))
	{
		return false;
	}

	if (!PlayerState)
	{
		return false;
	}

	const UARSaveGame* SaveGame = ResolveSave();
	if (!SaveGame)
	{
		return false;
	}

	RemoveRuntimeApplicationsFromPlayer(PlayerState);
	for (const FARRunBuffActivePayload& Payload : SaveGame->ActiveRunBuffPayloads)
	{
		ApplyPayloadToPlayer(PlayerState, Payload);
	}

	return true;
}

bool UARRunBuffSubsystem::EnsureAuthorityWorld(const TCHAR* Context) const
{
	const UWorld* World = GetWorld();
	if (!IsAuthorityWorld_RunBuff(World))
	{
		UE_LOG(ARLog, Verbose, TEXT("[RunBuff] %s ignored on non-authority world."), Context);
		return false;
	}

	return true;
}

UARSaveSubsystem* UARRunBuffSubsystem::ResolveSaveSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
}

UARSaveGame* UARRunBuffSubsystem::ResolveMutableSave() const
{
	UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	return SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
}

const UARSaveGame* UARRunBuffSubsystem::ResolveSave() const
{
	const UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	return SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
}

AARGameStateBase* UARRunBuffSubsystem::ResolveGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<AARGameStateBase>() : nullptr;
}

bool UARRunBuffSubsystem::ResolveScrapyardItemDefinition(const FGameplayTag ItemTag, FARScrapyardItemDefRow& OutDef) const
{
	OutDef = FARScrapyardItemDefRow();

	if (!ItemTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	UTagContentResolverSubsystem* Resolver = GameInstance->GetSubsystem<UTagContentResolverSubsystem>();
	if (!Resolver)
	{
		UE_LOG(ARLog, Warning, TEXT("[RunBuff] TagContentResolverSubsystem unavailable while resolving '%s'."), *ItemTag.ToString());
		return false;
	}

	FInstancedStruct RowData;
	FString ResolveError;
	if (!Resolver->TryResolveRowForTag(ItemTag, RowData, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[RunBuff] Failed resolving scrapyard item row for '%s': %s"),
			*ItemTag.ToString(),
			*ResolveError);
		return false;
	}

	const FARScrapyardItemDefRow* TypedRow = RowData.GetPtr<FARScrapyardItemDefRow>();
	if (!TypedRow)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[RunBuff] Scrapyard item row for '%s' was not FARScrapyardItemDefRow."),
			*ItemTag.ToString());
		return false;
	}

	OutDef = *TypedRow;
	return true;
}

int32 UARRunBuffSubsystem::ResolveMaxStackCountForItem(const FGameplayTag ItemTag) const
{
	FARScrapyardItemDefRow ItemDef;
	if (!ResolveScrapyardItemDefinition(ItemTag, ItemDef))
	{
		return MAX_int32;
	}

	if (ItemDef.StackRule == EARScrapyardStackRule::Unique)
	{
		return 1;
	}

	return FMath::Max(1, ItemDef.MaxStackCount);
}

FGameplayTag UARRunBuffSubsystem::ResolveEnergyDrinkStorageUnlockTag() const
{
	return FGameplayTag::RequestGameplayTag(TEXT("Unlock.Shop.Storage.EnergyDrink"), false);
}

int32 UARRunBuffSubsystem::GetStackCount(const TArray<FARRunBuffItemStack>& Stacks, const FGameplayTag ItemTag)
{
	if (!ItemTag.IsValid())
	{
		return 0;
	}

	for (const FARRunBuffItemStack& Stack : Stacks)
	{
		if (Stack.ItemTag == ItemTag)
		{
			return FMath::Max(0, Stack.Count);
		}
	}

	return 0;
}

int32 UARRunBuffSubsystem::UpsertStackCount(TArray<FARRunBuffItemStack>& Stacks, const FGameplayTag ItemTag, const int32 Delta)
{
	if (!ItemTag.IsValid() || Delta == 0)
	{
		return 0;
	}

	for (int32 Index = 0; Index < Stacks.Num(); ++Index)
	{
		FARRunBuffItemStack& Existing = Stacks[Index];
		if (Existing.ItemTag != ItemTag)
		{
			continue;
		}

		Existing.Count = FMath::Max(0, Existing.Count + Delta);
		if (Existing.Count == 0)
		{
			Stacks.RemoveAtSwap(Index);
			return 0;
		}

		return Existing.Count;
	}

	if (Delta <= 0)
	{
		return 0;
	}

	FARRunBuffItemStack& Added = Stacks.AddDefaulted_GetRef();
	Added.ItemTag = ItemTag;
	Added.Count = Delta;
	return Added.Count;
}

void UARRunBuffSubsystem::NormalizeStacks(TArray<FARRunBuffItemStack>& Stacks)
{
	for (int32 Index = Stacks.Num() - 1; Index >= 0; --Index)
	{
		FARRunBuffItemStack& Stack = Stacks[Index];
		if (!Stack.ItemTag.IsValid() || Stack.Count <= 0)
		{
			Stacks.RemoveAtSwap(Index);
			continue;
		}

		Stack.Count = FMath::Max(0, Stack.Count);
	}

	Stacks.Sort([](const FARRunBuffItemStack& A, const FARRunBuffItemStack& B)
	{
		return A.ItemTag.ToString() < B.ItemTag.ToString();
	});
}

void UARRunBuffSubsystem::MarkSaveDirty() const
{
	if (UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem())
	{
		SaveSubsystem->MarkSaveDirty();
	}
}

void UARRunBuffSubsystem::BroadcastSnapshotChanged() const
{
	UARRunBuffSubsystem* MutableThis = const_cast<UARRunBuffSubsystem*>(this);
	MutableThis->OnRunBuffStateChanged.Broadcast(GetRunBuffStateSnapshot());
}

void UARRunBuffSubsystem::ResetRuntimeApplications()
{
	TArray<TWeakObjectPtr<AARPlayerStateBase>> PlayersToClear;
	AppliedEffectHandlesByPlayer.GetKeys(PlayersToClear);
	for (const TWeakObjectPtr<AARPlayerStateBase>& PlayerWeak : PlayersToClear)
	{
		if (AARPlayerStateBase* PlayerState = PlayerWeak.Get())
		{
			RemoveRuntimeApplicationsFromPlayer(PlayerState);
		}
	}

	TArray<TWeakObjectPtr<AARPlayerStateBase>> TagPlayersToClear;
	AppliedTagCountsByPlayer.GetKeys(TagPlayersToClear);
	for (const TWeakObjectPtr<AARPlayerStateBase>& PlayerWeak : TagPlayersToClear)
	{
		if (AARPlayerStateBase* PlayerState = PlayerWeak.Get())
		{
			RemoveRuntimeApplicationsFromPlayer(PlayerState);
		}
	}

	AppliedEffectHandlesByPlayer.Reset();
	AppliedTagCountsByPlayer.Reset();
}

void UARRunBuffSubsystem::RemoveRuntimeApplicationsFromPlayer(AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	UAbilitySystemComponent* ASC = PlayerState->GetASC();
	if (!ASC)
	{
		AppliedEffectHandlesByPlayer.Remove(PlayerState);
		AppliedTagCountsByPlayer.Remove(PlayerState);
		return;
	}

	if (TArray<FActiveGameplayEffectHandle>* EffectHandles = AppliedEffectHandlesByPlayer.Find(PlayerState))
	{
		for (const FActiveGameplayEffectHandle& Handle : *EffectHandles)
		{
			if (Handle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(Handle);
			}
		}
	}
	AppliedEffectHandlesByPlayer.Remove(PlayerState);

	if (TArray<FAppliedTagCount>* TagCounts = AppliedTagCountsByPlayer.Find(PlayerState))
	{
		for (const FAppliedTagCount& TagCount : *TagCounts)
		{
			if (!TagCount.Tag.IsValid() || TagCount.Count <= 0)
			{
				continue;
			}

			FGameplayTagContainer SingleTagContainer;
			SingleTagContainer.AddTag(TagCount.Tag);
			ASC->RemoveLooseGameplayTags(SingleTagContainer, TagCount.Count, EGameplayTagReplicationState::TagOnly);
		}
	}
	AppliedTagCountsByPlayer.Remove(PlayerState);
}

void UARRunBuffSubsystem::ApplyPayloadToPlayer(AARPlayerStateBase* PlayerState, const FARRunBuffActivePayload& Payload)
{
	if (!PlayerState || !Payload.ItemTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = PlayerState->GetASC();
	if (!ASC)
	{
		return;
	}

	const int32 AppliedCount = FMath::Max(1, Payload.AppliedCount);
	TArray<FActiveGameplayEffectHandle>& AppliedHandles = AppliedEffectHandlesByPlayer.FindOrAdd(PlayerState);
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Payload.GameplayEffects)
	{
		if (!EffectClass)
		{
			continue;
		}

		for (int32 StackIndex = 0; StackIndex < AppliedCount; ++StackIndex)
		{
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				AppliedHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
			}
		}
	}

	if (!Payload.GrantedTags.IsEmpty())
	{
		ASC->AddLooseGameplayTags(Payload.GrantedTags, AppliedCount, EGameplayTagReplicationState::TagOnly);

		TArray<FAppliedTagCount>& AppliedTagCounts = AppliedTagCountsByPlayer.FindOrAdd(PlayerState);
		for (const FGameplayTag Tag : Payload.GrantedTags)
		{
			if (!Tag.IsValid())
			{
				continue;
			}

			FAppliedTagCount* ExistingTagCount = AppliedTagCounts.FindByPredicate([Tag](const FAppliedTagCount& Entry)
			{
				return Entry.Tag == Tag;
			});
			if (ExistingTagCount)
			{
				ExistingTagCount->Count += AppliedCount;
			}
			else
			{
				FAppliedTagCount& AddedTagCount = AppliedTagCounts.AddDefaulted_GetRef();
				AddedTagCount.Tag = Tag;
				AddedTagCount.Count = AppliedCount;
			}
		}
	}
}
