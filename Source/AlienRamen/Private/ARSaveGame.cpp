#include "ARSaveGame.h"
#include "GameplayEffect.h"

UARSaveGame::UARSaveGame()
{
	SaveGameVersion = CurrentSchemaVersion;
}

bool UARSaveGame::FindPlayerStateDataBySlot(const EARPlayerSlot Slot, FARPlayerStateSaveData& OutData, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	for (int32 i = 0; i < PlayerStates.Num(); ++i)
	{
		if (PlayerStates[i].Identity.PlayerSlot == Slot)
		{
			OutData = PlayerStates[i];
			OutIndex = i;
			return true;
		}
	}
	return false;
}

bool UARSaveGame::FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	int32 FirstIdentityMatchIndex = INDEX_NONE;

	for (int32 i = 0; i < PlayerStates.Num(); ++i)
	{
		if (PlayerStates[i].Identity.Matches(Identity))
		{
			if (FirstIdentityMatchIndex == INDEX_NONE)
			{
				FirstIdentityMatchIndex = i;
			}

			// Prefer slot-consistent match when multiple rows share the same online identity
			// (for example couch coop players on one Steam account).
			if (Identity.PlayerSlot != EARPlayerSlot::Unknown
				&& PlayerStates[i].Identity.PlayerSlot == Identity.PlayerSlot)
			{
				OutData = PlayerStates[i];
				OutIndex = i;
				return true;
			}
		}
	}

	if (FirstIdentityMatchIndex != INDEX_NONE)
	{
		OutData = PlayerStates[FirstIdentityMatchIndex];
		OutIndex = FirstIdentityMatchIndex;
		return true;
	}

	return false;
}

int32 UARSaveGame::ValidateAndSanitize(TArray<FString>* OutWarnings)
{
	int32 ClampedCount = 0;
	auto ClampNonNegative = [OutWarnings, &ClampedCount](int32& Value, const TCHAR* FieldName)
	{
		if (Value < 0)
		{
			if (OutWarnings)
			{
				OutWarnings->Add(FString::Printf(TEXT("%s was negative and clamped to 0."), FieldName));
			}
			Value = 0;
			++ClampedCount;
		}
	};

	auto SanitizeTagContainer = [OutWarnings, &ClampedCount](FGameplayTagContainer& Container, const TCHAR* FieldName)
	{
		FGameplayTagContainer Sanitized;
		bool bRemovedInvalidTag = false;
		for (const FGameplayTag Tag : Container)
		{
			if (Tag.IsValid())
			{
				Sanitized.AddTag(Tag);
			}
			else
			{
				bRemovedInvalidTag = true;
			}
		}

		if (bRemovedInvalidTag)
		{
			Container = Sanitized;
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(FString::Printf(TEXT("%s contained invalid tags and they were removed."), FieldName));
			}
		}
	};

	auto SanitizeStackArray =
		[OutWarnings, &ClampedCount](TArray<FARRunBuffItemStack>& Stacks, const TCHAR* FieldName)
	{
		TMap<FString, FARRunBuffItemStack> Aggregated;
		bool bChanged = false;

		for (const FARRunBuffItemStack& Stack : Stacks)
		{
			if (!Stack.ItemTag.IsValid())
			{
				bChanged = true;
				continue;
			}

			const int32 Count = FMath::Max(0, Stack.Count);
			if (Count <= 0)
			{
				bChanged = true;
				continue;
			}

			const FString StackKey = FString::Printf(TEXT("%s|%s"), *Stack.CharacterTag.ToString(), *Stack.ItemTag.ToString());
			FARRunBuffItemStack& AggregatedStack = Aggregated.FindOrAdd(StackKey);
			AggregatedStack.CharacterTag = Stack.CharacterTag;
			AggregatedStack.ItemTag = Stack.ItemTag;
			AggregatedStack.Count += Count;
		}

		TArray<FARRunBuffItemStack> Sanitized;
		Sanitized.Reserve(Aggregated.Num());
		for (const TPair<FString, FARRunBuffItemStack>& Pair : Aggregated)
		{
			if (Pair.Value.Count <= 0 || !Pair.Value.ItemTag.IsValid())
			{
				bChanged = true;
				continue;
			}

			Sanitized.Add(Pair.Value);
		}

		Sanitized.Sort([](const FARRunBuffItemStack& A, const FARRunBuffItemStack& B)
		{
			const FString CharA = A.CharacterTag.ToString();
			const FString CharB = B.CharacterTag.ToString();
			if (CharA != CharB)
			{
				return CharA < CharB;
			}

			return A.ItemTag.ToString() < B.ItemTag.ToString();
		});

		if (bChanged || Sanitized.Num() != Stacks.Num())
		{
			Stacks = MoveTemp(Sanitized);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(FString::Printf(TEXT("%s contained invalid entries and was normalized."), FieldName));
			}
		}
	};

	auto SanitizeShopTransientCarryables =
		[OutWarnings, &ClampedCount](TArray<FARShopTransientCarryableSnapshot>& Snapshots)
	{
		bool bChanged = false;
		for (int32 SnapshotIndex = Snapshots.Num() - 1; SnapshotIndex >= 0; --SnapshotIndex)
		{
			FARShopTransientCarryableSnapshot& Snapshot = Snapshots[SnapshotIndex];
			if (Snapshot.ActorClass.IsNull() || !Snapshot.WorldTransform.IsValid())
			{
				Snapshots.RemoveAtSwap(SnapshotIndex);
				bChanged = true;
				continue;
			}

			if (Snapshot.MeatAmount < 1)
			{
				Snapshot.MeatAmount = 1;
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			return;
		}

		++ClampedCount;
		if (OutWarnings)
		{
			OutWarnings->Add(TEXT("ShopTransientCarryables contained invalid data and was sanitized."));
		}
	};

	ClampNonNegative(Money, TEXT("Money"));
	ClampNonNegative(Scrap, TEXT("Scrap"));
	ClampNonNegative(Cycles, TEXT("Cycles"));
	ClampNonNegative(FactionClout, TEXT("FactionClout"));
	ClampNonNegative(ActiveRunBuffCycleId, TEXT("ActiveRunBuffCycleId"));
	ClampNonNegative(Meat.RedAmount, TEXT("Meat.RedAmount"));
	ClampNonNegative(Meat.BlueAmount, TEXT("Meat.BlueAmount"));
	ClampNonNegative(Meat.WhiteAmount, TEXT("Meat.WhiteAmount"));
	ClampNonNegative(Meat.UnspecifiedAmount, TEXT("Meat.UnspecifiedAmount"));

	for (FARMeatTypeAmount& Entry : Meat.AdditionalAmountsByType)
	{
		ClampNonNegative(Entry.Amount, TEXT("Meat.AdditionalAmountsByType.Amount"));
	}
	Meat.NormalizeAdditionalAmounts();

	for (FARPlayerStateSaveData& PlayerData : PlayerStates)
	{
		ClampNonNegative(PlayerData.Identity.LegacyId, TEXT("PlayerState.Identity.LegacyId"));
	}

	SanitizeTagContainer(DialogueCompletedConversationTagsByGame, TEXT("DialogueCompletedConversationTagsByGame"));
	SanitizeStackArray(StoredEnergyDrinkStacks, TEXT("StoredEnergyDrinkStacks"));
	SanitizeStackArray(QueuedEnergyDrinkStacks, TEXT("QueuedEnergyDrinkStacks"));
	SanitizeShopTransientCarryables(ShopTransientCarryables);

	for (int32 PayloadIndex = ActiveRunBuffPayloads.Num() - 1; PayloadIndex >= 0; --PayloadIndex)
	{
		FARRunBuffActivePayload& Payload = ActiveRunBuffPayloads[PayloadIndex];
		if (!Payload.ItemTag.IsValid())
		{
			ActiveRunBuffPayloads.RemoveAtSwap(PayloadIndex);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("ActiveRunBuffPayloads contained an invalid ItemTag and was removed."));
			}
			continue;
		}

		if (Payload.AppliedCount < 1)
		{
			Payload.AppliedCount = 1;
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("ActiveRunBuffPayloads.AppliedCount was clamped to 1."));
			}
		}

		Payload.GameplayEffects.RemoveAll([](const TSubclassOf<UGameplayEffect>& EffectClass)
		{
			return EffectClass == nullptr;
		});
		SanitizeTagContainer(Payload.GrantedTags, TEXT("ActiveRunBuffPayloads.GrantedTags"));
	}

	for (int32 Index = DialogueRelationshipStates.Num() - 1; Index >= 0; --Index)
	{
		if (!DialogueRelationshipStates[Index].SpeakerTag.IsValid())
		{
			DialogueRelationshipStates.RemoveAtSwap(Index);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("DialogueRelationshipStates contained an invalid SpeakerTag and was removed."));
			}
		}
	}

	for (FDialoguePlayerPersistentState& PlayerDialogueState : DialoguePlayerPersistentStates)
	{
		SanitizeTagContainer(PlayerDialogueState.ProgressionTags, TEXT("DialoguePlayerPersistentStates.ProgressionTags"));
		SanitizeTagContainer(PlayerDialogueState.CompletedConversationTags, TEXT("DialoguePlayerPersistentStates.CompletedConversationTags"));
		SanitizeTagContainer(PlayerDialogueState.SeenConversationTagsThisCycle, TEXT("DialoguePlayerPersistentStates.SeenConversationTagsThisCycle"));
		SanitizeTagContainer(PlayerDialogueState.SkippedConversationTagsThisCycle, TEXT("DialoguePlayerPersistentStates.SkippedConversationTagsThisCycle"));

		for (int32 ChoiceIndex = PlayerDialogueState.CompletedChoiceRecords.Num() - 1; ChoiceIndex >= 0; --ChoiceIndex)
		{
			const FDialogueChoiceMemoryRecord& Record = PlayerDialogueState.CompletedChoiceRecords[ChoiceIndex];
			if (!Record.ConversationTag.IsValid() || !Record.ChoiceNodeId.IsValid() || !Record.SelectedBranchId.IsValid())
			{
				PlayerDialogueState.CompletedChoiceRecords.RemoveAtSwap(ChoiceIndex);
				++ClampedCount;
				if (OutWarnings)
				{
					OutWarnings->Add(TEXT("DialoguePlayerPersistentStates contained an invalid choice-memory record and it was removed."));
				}
			}
		}
	}

	TSet<FGameplayTag> SeenFactions;
	for (int32 Index = FactionPopularityStates.Num() - 1; Index >= 0; --Index)
	{
		FARFactionRuntimeState& State = FactionPopularityStates[Index];
		if (!State.FactionTag.IsValid())
		{
			FactionPopularityStates.RemoveAtSwap(Index);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("FactionPopularityStates contained an invalid FactionTag and was removed."));
			}
			continue;
		}

		if (SeenFactions.Contains(State.FactionTag))
		{
			FactionPopularityStates.RemoveAtSwap(Index);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("FactionPopularityStates contained duplicate FactionTag entries and extras were removed."));
			}
			continue;
		}

		SeenFactions.Add(State.FactionTag);
	}

	if (ActiveFactionTag.IsValid() && !SeenFactions.Contains(ActiveFactionTag))
	{
		FARFactionRuntimeState ActiveEntry;
		ActiveEntry.FactionTag = ActiveFactionTag;
		FactionPopularityStates.Add(ActiveEntry);
		++ClampedCount;
		if (OutWarnings)
		{
			OutWarnings->Add(TEXT("ActiveFactionTag was missing from FactionPopularityStates and was auto-added."));
		}
	}

	if (SaveSlotNumber < 0)
	{
		SaveSlotNumber = 0;
		++ClampedCount;
		if (OutWarnings)
		{
			OutWarnings->Add(TEXT("SaveSlotNumber was negative and clamped to 0."));
		}
	}

	return ClampedCount;
}
