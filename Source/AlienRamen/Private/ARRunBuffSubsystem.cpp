#include "ARRunBuffSubsystem.h"

#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARScrapyardTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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
	return SaveGame ? GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag()) : 0;
}

int32 UARRunBuffSubsystem::GetStoredEnergyDrinkCountForCharacter(const FGameplayTag ItemTag, const FGameplayTag CharacterTag) const
{
	if (!CharacterTag.IsValid())
	{
		return GetStoredEnergyDrinkCount(ItemTag);
	}

	const UARSaveGame* SaveGame = ResolveSave();
	return SaveGame ? GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, CharacterTag) : 0;
}

int32 UARRunBuffSubsystem::GetQueuedEnergyDrinkCount(const FGameplayTag ItemTag) const
{
	const UARSaveGame* SaveGame = ResolveSave();
	return SaveGame ? GetStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, FGameplayTag()) : 0;
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

	TArray<FARRunBuffItemStack>& TargetStacks = SaveGame->StoredEnergyDrinkStacks;

	const int32 MaxStackCount = ResolveMaxStackCountForItem(ItemTag);
	const int32 CurrentCount = GetStackCount(TargetStacks, ItemTag, FGameplayTag());
	const int32 AvailableCapacity = FMath::Max(0, MaxStackCount - CurrentCount);
	if (AvailableCapacity <= 0)
	{
		return false;
	}

	const int32 CountToAdd = FMath::Min(Count, AvailableCapacity);
	if (UpsertStackCount(TargetStacks, ItemTag, FGameplayTag(), CountToAdd) <= 0)
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

	if (GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag()) < Count)
	{
		return false;
	}

	const int32 MaxQueuedCount = ResolveMaxStackCountForItem(ItemTag);
	const int32 QueuedCount = GetStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, FGameplayTag());
	if (QueuedCount + Count > MaxQueuedCount)
	{
		return false;
	}

	UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag(), -Count);
	UpsertStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, FGameplayTag(), Count);
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
	const int32 CurrentQueuedCount = GetStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, FGameplayTag());
	const int32 AvailableCapacity = FMath::Max(0, MaxQueuedCount - CurrentQueuedCount);
	if (AvailableCapacity <= 0)
	{
		return false;
	}

	const int32 CountToQueue = FMath::Min(Count, AvailableCapacity);
	if (UpsertStackCount(SaveGame->QueuedEnergyDrinkStacks, ItemTag, FGameplayTag(), CountToQueue) <= 0)
	{
		return false;
	}

	NormalizeStacks(SaveGame->QueuedEnergyDrinkStacks);
	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::ConsumeEnergyDrinkForCharacter(const FGameplayTag ItemTag, const FGameplayTag CharacterTag)
{
	if (!EnsureAuthorityWorld(TEXT("ConsumeEnergyDrinkForCharacter")))
	{
		return false;
	}

	if (!ItemTag.IsValid() || !CharacterTag.IsValid())
	{
		return false;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return false;
	}

	if (IsEnergyDrinkActiveForCharacter(ItemTag, CharacterTag))
	{
		return false;
	}

	const int32 SharedStoredCount = GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag());
	const int32 CharacterStoredCount = GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, CharacterTag);
	if (SharedStoredCount <= 0 && CharacterStoredCount <= 0)
	{
		return false;
	}

	if (SharedStoredCount > 0)
	{
		UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag(), -1);
	}
	else
	{
		UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, CharacterTag, -1);
	}

	if (!ApplyEnergyDrinkPayloadForCharacter(SaveGame, ItemTag, CharacterTag))
	{
		// Revert inventory mutation when payload application fails.
		if (SharedStoredCount > 0)
		{
			UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag(), 1);
		}
		else
		{
			UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, CharacterTag, 1);
		}
		NormalizeStacks(SaveGame->StoredEnergyDrinkStacks);
		return false;
	}

	NormalizeStacks(SaveGame->StoredEnergyDrinkStacks);
	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::ConsumeEnergyDrinkForPlayerState(const FGameplayTag ItemTag, AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return false;
	}

	return ConsumeEnergyDrinkForCharacter(ItemTag, ResolveCharacterTagFromPlayerState(PlayerState));
}

bool UARRunBuffSubsystem::ConsumeSpawnedEnergyDrinkForPlayerState(const FGameplayTag ItemTag, AARPlayerStateBase* PlayerState)
{
	if (!EnsureAuthorityWorld(TEXT("ConsumeSpawnedEnergyDrinkForPlayerState")))
	{
		return false;
	}

	if (!PlayerState)
	{
		return false;
	}

	const FGameplayTag CharacterTag = ResolveCharacterTagFromPlayerState(PlayerState);
	if (!CharacterTag.IsValid())
	{
		return false;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return false;
	}

	if (!ApplyEnergyDrinkPayloadForCharacter(SaveGame, ItemTag, CharacterTag))
	{
		return false;
	}

	MarkSaveDirty();
	BroadcastSnapshotChanged();
	return true;
}

bool UARRunBuffSubsystem::IsEnergyDrinkActiveForCharacter(const FGameplayTag ItemTag, const FGameplayTag CharacterTag) const
{
	if (!ItemTag.IsValid() || !CharacterTag.IsValid())
	{
		return false;
	}

	const UARSaveGame* SaveGame = ResolveSave();
	if (!SaveGame)
	{
		return false;
	}

	return SaveGame->ActiveRunBuffPayloads.ContainsByPredicate(
		[ItemTag, CharacterTag](const FARRunBuffActivePayload& Payload)
		{
			return IsMatchingPayloadKey(Payload, ItemTag, CharacterTag);
		});
}

void UARRunBuffSubsystem::ClearRunBuffsForShopEntry()
{
	if (!EnsureAuthorityWorld(TEXT("ClearRunBuffsForShopEntry")))
	{
		return;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return;
	}

	SaveGame->QueuedEnergyDrinkStacks.Reset();
	SaveGame->ActiveRunBuffPayloads.Reset();
	SaveGame->ActiveRunBuffCycleId = FMath::Max(0, SaveGame->ActiveRunBuffCycleId) + 1;
	LastRotationCycleId = INDEX_NONE;
	ResetRuntimeApplications();
	MarkSaveDirty();
	BroadcastSnapshotChanged();
}

void UARRunBuffSubsystem::ClearQueuedRunBuffsAtInvaderEnd()
{
	if (!EnsureAuthorityWorld(TEXT("ClearQueuedRunBuffsAtInvaderEnd")))
	{
		return;
	}

	UARSaveGame* SaveGame = ResolveMutableSave();
	if (!SaveGame)
	{
		return;
	}

	if (SaveGame->QueuedEnergyDrinkStacks.IsEmpty())
	{
		return;
	}

	SaveGame->QueuedEnergyDrinkStacks.Reset();
	MarkSaveDirty();
	BroadcastSnapshotChanged();
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

	if (GetStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag()) < Count)
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

	UpsertStackCount(SaveGame->StoredEnergyDrinkStacks, ItemTag, FGameplayTag(), -Count);
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
	bool bMutatedSave = false;
	for (const FARRunBuffItemStack& QueuedStack : SaveGame->QueuedEnergyDrinkStacks)
	{
		if (!QueuedStack.ItemTag.IsValid() || QueuedStack.Count <= 0)
		{
			continue;
		}

		if (IsEnergyDrinkActiveForCharacter(QueuedStack.ItemTag, QueuedStack.CharacterTag))
		{
			continue;
		}

		FAREnergyDrinkDefRow DrinkDef;
		if (!ResolveEnergyDrinkDefinition(QueuedStack.ItemTag, DrinkDef))
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[RunBuff] Invader-init skipped unresolved queued energy drink '%s'."),
				*QueuedStack.ItemTag.ToString());
			continue;
		}

		FARRunBuffActivePayload& Payload = SaveGame->ActiveRunBuffPayloads.AddDefaulted_GetRef();
		Payload.CharacterTag = QueuedStack.CharacterTag;
		Payload.ItemTag = QueuedStack.ItemTag;
		Payload.AppliedCount = FMath::Clamp(QueuedStack.Count, 1, ResolveMaxStackCountForItem(QueuedStack.ItemTag));
		Payload.GameplayEffects = DrinkDef.RunBuffGameplayEffects;
		Payload.GrantedTags = DrinkDef.RunBuffGrantedTags;
		bMutatedSave = true;
	}

	if (SaveGame->QueuedEnergyDrinkStacks.Num() > 0)
	{
		SaveGame->QueuedEnergyDrinkStacks.Reset();
		bMutatedSave = true;
	}

	NormalizePayloads(SaveGame->ActiveRunBuffPayloads);
	if (bMutatedSave)
	{
		SaveGame->ActiveRunBuffCycleId = FMath::Max(0, SaveGame->ActiveRunBuffCycleId) + 1;
		MarkSaveDirty();
	}

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

	const FGameplayTag PlayerCharacterTag = ResolveCharacterTagFromPlayerState(PlayerState);
	RemoveRuntimeApplicationsFromPlayer(PlayerState);
	for (const FARRunBuffActivePayload& Payload : SaveGame->ActiveRunBuffPayloads)
	{
		if (Payload.CharacterTag.IsValid() && Payload.CharacterTag != PlayerCharacterTag)
		{
			continue;
		}

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

	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	return ItemDefinitions && ItemDefinitions->ResolveItemDefinition(ItemTag, OutDef);
}

bool UARRunBuffSubsystem::ResolveEnergyDrinkDefinition(const FGameplayTag ItemTag, FAREnergyDrinkDefRow& OutDef) const
{
	OutDef = FAREnergyDrinkDefRow();

	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	return ItemDefinitions && ItemDefinitions->ResolveEnergyDrinkDefinition(ItemTag, OutDef);
}

int32 UARRunBuffSubsystem::ResolveMaxStackCountForItem(const FGameplayTag ItemTag) const
{
	FAREnergyDrinkDefRow DrinkDef;
	if (ResolveEnergyDrinkDefinition(ItemTag, DrinkDef))
	{
		if (DrinkDef.StackRule == EARScrapyardStackRule::Unique)
		{
			return 1;
		}

		return FMath::Max(1, DrinkDef.MaxStackCount);
	}

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

FGameplayTag UARRunBuffSubsystem::ResolveCharacterTagFromPlayerState(const AARPlayerStateBase* PlayerState) const
{
	if (!PlayerState)
	{
		return FGameplayTag();
	}

	static const FGameplayTag CharacterRootTag = FGameplayTag::RequestGameplayTag(TEXT("Player.Character"), false);
	if (CharacterRootTag.IsValid())
	{
		for (const FGameplayTag LoadoutTag : PlayerState->LoadoutTags)
		{
			if (LoadoutTag.IsValid() && LoadoutTag.MatchesTag(CharacterRootTag))
			{
				return LoadoutTag;
			}
		}
	}

	switch (PlayerState->GetCharacterPicked())
	{
	case EARCharacterChoice::Brother:
		return FGameplayTag::RequestGameplayTag(TEXT("Dialogue.Speaker.Brother"), false);
	case EARCharacterChoice::Sister:
		return FGameplayTag::RequestGameplayTag(TEXT("Dialogue.Speaker.Sister"), false);
	default:
		return FGameplayTag();
	}
}

bool UARRunBuffSubsystem::IsMatchingStackKey(const FARRunBuffItemStack& Stack, const FGameplayTag ItemTag, const FGameplayTag CharacterTag)
{
	if (!ItemTag.IsValid() || Stack.ItemTag != ItemTag)
	{
		return false;
	}

	if (!CharacterTag.IsValid())
	{
		return true;
	}

	return Stack.CharacterTag == CharacterTag;
}

bool UARRunBuffSubsystem::IsMatchingPayloadKey(const FARRunBuffActivePayload& Payload, const FGameplayTag ItemTag, const FGameplayTag CharacterTag)
{
	if (!ItemTag.IsValid() || Payload.ItemTag != ItemTag)
	{
		return false;
	}

	if (!CharacterTag.IsValid())
	{
		return true;
	}

	return !Payload.CharacterTag.IsValid() || Payload.CharacterTag == CharacterTag;
}

int32 UARRunBuffSubsystem::GetStackCount(const TArray<FARRunBuffItemStack>& Stacks, const FGameplayTag ItemTag, const FGameplayTag CharacterTag)
{
	if (!ItemTag.IsValid())
	{
		return 0;
	}

	int32 TotalCount = 0;
	for (const FARRunBuffItemStack& Stack : Stacks)
	{
		if (IsMatchingStackKey(Stack, ItemTag, CharacterTag))
		{
			TotalCount += FMath::Max(0, Stack.Count);
			if (CharacterTag.IsValid())
			{
				break;
			}
		}
	}

	return TotalCount;
}

int32 UARRunBuffSubsystem::UpsertStackCount(TArray<FARRunBuffItemStack>& Stacks, const FGameplayTag ItemTag, const FGameplayTag CharacterTag, const int32 Delta)
{
	if (!ItemTag.IsValid() || Delta == 0)
	{
		return 0;
	}

	for (int32 Index = 0; Index < Stacks.Num(); ++Index)
	{
		FARRunBuffItemStack& Existing = Stacks[Index];
		if (Existing.ItemTag != ItemTag || Existing.CharacterTag != CharacterTag)
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
	Added.CharacterTag = CharacterTag;
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
		const FString CharA = A.CharacterTag.ToString();
		const FString CharB = B.CharacterTag.ToString();
		if (CharA == CharB)
		{
			return A.ItemTag.ToString() < B.ItemTag.ToString();
		}

		return CharA < CharB;
	});
}

void UARRunBuffSubsystem::NormalizePayloads(TArray<FARRunBuffActivePayload>& Payloads)
{
	for (int32 Index = Payloads.Num() - 1; Index >= 0; --Index)
	{
		FARRunBuffActivePayload& Payload = Payloads[Index];
		if (!Payload.ItemTag.IsValid())
		{
			Payloads.RemoveAtSwap(Index);
			continue;
		}

		Payload.AppliedCount = FMath::Max(1, Payload.AppliedCount);
		Payload.GameplayEffects.RemoveAll([](const TSubclassOf<UGameplayEffect>& EffectClass)
		{
			return EffectClass == nullptr;
		});
	}

	Payloads.Sort([](const FARRunBuffActivePayload& A, const FARRunBuffActivePayload& B)
	{
		const FString CharA = A.CharacterTag.ToString();
		const FString CharB = B.CharacterTag.ToString();
		if (CharA == CharB)
		{
			return A.ItemTag.ToString() < B.ItemTag.ToString();
		}

		return CharA < CharB;
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

bool UARRunBuffSubsystem::ApplyEnergyDrinkPayloadForCharacter(
	UARSaveGame* SaveGame,
	const FGameplayTag ItemTag,
	const FGameplayTag CharacterTag)
{
	if (!SaveGame || !ItemTag.IsValid() || !CharacterTag.IsValid())
	{
		return false;
	}

	if (IsEnergyDrinkActiveForCharacter(ItemTag, CharacterTag))
	{
		return false;
	}

	FAREnergyDrinkDefRow EnergyDrinkDef;
	if (!ResolveEnergyDrinkDefinition(ItemTag, EnergyDrinkDef))
	{
		return false;
	}

	FARRunBuffActivePayload Payload;
	Payload.CharacterTag = CharacterTag;
	Payload.ItemTag = ItemTag;
	Payload.AppliedCount = 1;
	Payload.GameplayEffects = EnergyDrinkDef.RunBuffGameplayEffects;
	Payload.GrantedTags = EnergyDrinkDef.RunBuffGrantedTags;
	SaveGame->ActiveRunBuffPayloads.Add(MoveTemp(Payload));

	NormalizePayloads(SaveGame->ActiveRunBuffPayloads);
	SaveGame->ActiveRunBuffCycleId = FMath::Max(0, SaveGame->ActiveRunBuffCycleId) + 1;
	LastRotationCycleId = INDEX_NONE;

	if (AARGameStateBase* GameState = ResolveGameState())
	{
		for (AARPlayerStateBase* PlayerState : GameState->GetPlayerStates())
		{
			if (!PlayerState)
			{
				continue;
			}

			if (ResolveCharacterTagFromPlayerState(PlayerState) != CharacterTag)
			{
				continue;
			}

			const FARRunBuffActivePayload* ActivePayload = SaveGame->ActiveRunBuffPayloads.FindByPredicate(
				[ItemTag, CharacterTag](const FARRunBuffActivePayload& Candidate)
				{
					return Candidate.ItemTag == ItemTag && Candidate.CharacterTag == CharacterTag;
				});
			if (ActivePayload)
			{
				ApplyPayloadToPlayer(PlayerState, *ActivePayload);
			}
		}
	}

	return true;
}
