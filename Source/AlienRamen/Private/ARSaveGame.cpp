#include "ARSaveGame.h"

#include "ARLoadoutSettings.h"
#include "GameplayEffect.h"

namespace
{
	static void AddWarning(TArray<FString>* OutWarnings, const TCHAR* Message)
	{
		if (OutWarnings)
		{
			OutWarnings->Add(Message);
		}
	}

	static void AddWarningf(TArray<FString>* OutWarnings, const FString& Message)
	{
		if (OutWarnings)
		{
			OutWarnings->Add(Message);
		}
	}

}

UARSaveGame::UARSaveGame()
{
	SaveGameVersion = CurrentSchemaVersion;
}

bool UARSaveGame::FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	for (int32 i = 0; i < PlayerStates.Num(); ++i)
	{
		const FARPlayerIdentity& ExistingIdentity = PlayerStates[i].Identity;
		if (ExistingIdentity.Matches(Identity))
		{
			OutData = PlayerStates[i];
			OutIndex = i;
			return true;
		}
	}

	return false;
}

bool UARSaveGame::FindCharacterStateDataByTag(const FGameplayTag CharacterTag, FARCharacterSaveData& OutData, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedTag.IsValid())
	{
		return false;
	}

	for (int32 Index = 0; Index < CharacterStates.Num(); ++Index)
	{
		if (CharacterStates[Index].CharacterTag == NormalizedTag)
		{
			OutData = CharacterStates[Index];
			OutIndex = Index;
			return true;
		}
	}

	return false;
}

FARCharacterSaveData* UARSaveGame::FindCharacterStateDataMutable(const FGameplayTag CharacterTag, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedTag.IsValid())
	{
		return nullptr;
	}

	for (int32 Index = 0; Index < CharacterStates.Num(); ++Index)
	{
		if (CharacterStates[Index].CharacterTag == NormalizedTag)
		{
			OutIndex = Index;
			return &CharacterStates[Index];
		}
	}

	return nullptr;
}

FARCharacterSaveData& UARSaveGame::FindOrAddCharacterStateData(const FGameplayTag CharacterTag)
{
	int32 ExistingIndex = INDEX_NONE;
	if (FARCharacterSaveData* Existing = FindCharacterStateDataMutable(CharacterTag, ExistingIndex))
	{
		return *Existing;
	}

	FARCharacterSaveData& Added = CharacterStates.AddDefaulted_GetRef();
	Added.CharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	return Added;
}

int32 UARSaveGame::ValidateAndSanitize(TArray<FString>* OutWarnings)
{
	int32 ClampedCount = 0;
	SaveGameVersion = CurrentSchemaVersion;

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
			if (!Tag.IsValid())
			{
				bRemovedInvalidTag = true;
				continue;
			}

			Sanitized.AddTag(Tag);
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

	auto SanitizeMeatQualityTier = [](EARVendingQualityTier& InOutTier) -> bool
	{
		if (StaticEnum<EARVendingQualityTier>()->IsValidEnumValue(static_cast<int64>(InOutTier)))
		{
			return false;
		}

		InOutTier = EARVendingQualityTier::Standard;
		return true;
	};

	auto SanitizeShopTransientCarryables =
		[OutWarnings, &ClampedCount, &SanitizeMeatQualityTier](TArray<FARShopTransientCarryableSnapshot>& Snapshots)
	{
		bool bChanged = false;
		auto SanitizeBowlSlot = [&SanitizeMeatQualityTier, &bChanged](
			FARRamenBowlSlotSpec& Slot,
			const EARRamenStationType ExpectedSlotType)
		{
			const EARAffinityColor OldColor = Slot.Color;
			const EARRamenStationType OldSlotType = Slot.SlotType;
			Slot.Color = Slot.Color == EARAffinityColor::Unknown ? EARAffinityColor::None : Slot.Color;
			Slot.SlotType = ExpectedSlotType;
			bChanged = bChanged || Slot.Color != OldColor || Slot.SlotType != OldSlotType || SanitizeMeatQualityTier(Slot.QualityTier);
		};

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

			if (Snapshot.BowlFillStep < 0)
			{
				Snapshot.BowlFillStep = 0;
				bChanged = true;
			}
			else if (Snapshot.BowlFillStep > 3)
			{
				Snapshot.BowlFillStep = 3;
				bChanged = true;
			}

			const bool bWasQualityTierInvalid = SanitizeMeatQualityTier(Snapshot.MeatQualityTier);
			if (bWasQualityTierInvalid)
			{
				bChanged = true;
			}

			SanitizeBowlSlot(Snapshot.BowlSpec.Noodles, EARRamenStationType::Noodles);
			SanitizeBowlSlot(Snapshot.BowlSpec.Broth, EARRamenStationType::Broth);
			SanitizeBowlSlot(Snapshot.BowlSpec.Toppings, EARRamenStationType::Toppings);
		}

		if (bChanged)
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("ShopTransientCarryables contained invalid data and was sanitized."));
		}
	};

	auto SanitizeHeldShopItem =
		[OutWarnings, &ClampedCount, &SanitizeMeatQualityTier](FARCharacterHeldShopItemSnapshot& Snapshot, const TCHAR* FieldName)
	{
		bool bChanged = false;

		if (Snapshot.MeatAmount < 1)
		{
			Snapshot.MeatAmount = 1;
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.MeatAmount was clamped to 1."), FieldName));
		}

		const bool bWasQualityTierInvalid = SanitizeMeatQualityTier(Snapshot.MeatQualityTier);
		if (bWasQualityTierInvalid)
		{
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.MeatQualityTier was reset to Standard."), FieldName));
		}

		if (Snapshot.BowlFillStep < 0)
		{
			Snapshot.BowlFillStep = 0;
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.BowlFillStep was clamped to 0."), FieldName));
		}
		else if (Snapshot.BowlFillStep > 3)
		{
			Snapshot.BowlFillStep = 3;
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.BowlFillStep was clamped to 3."), FieldName));
		}

		auto SanitizeHeldBowlSlot = [&SanitizeMeatQualityTier, &bChanged](FARRamenBowlSlotSpec& Slot, const EARRamenStationType ExpectedSlotType)
		{
			const EARAffinityColor OldColor = Slot.Color;
			const EARRamenStationType OldSlotType = Slot.SlotType;
			Slot.SlotType = ExpectedSlotType;
			Slot.Color = Slot.Color == EARAffinityColor::Unknown ? EARAffinityColor::None : Slot.Color;
			bChanged = bChanged || Slot.Color != OldColor || Slot.SlotType != OldSlotType || SanitizeMeatQualityTier(Slot.QualityTier);
		};
		SanitizeHeldBowlSlot(Snapshot.BowlSpec.Noodles, EARRamenStationType::Noodles);
		SanitizeHeldBowlSlot(Snapshot.BowlSpec.Broth, EARRamenStationType::Broth);
		SanitizeHeldBowlSlot(Snapshot.BowlSpec.Toppings, EARRamenStationType::Toppings);

		if (bChanged)
		{
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.BowlSpec contained invalid bowl slot data and was normalized."), FieldName));
		}
	};

	auto SanitizeVendingStockedBowls =
		[OutWarnings, &ClampedCount, &SanitizeMeatQualityTier](TArray<FARVendingStockedBowlEntry>& Entries)
	{
		bool bChanged = false;
		auto SanitizeBowlSlot = [&SanitizeMeatQualityTier, &bChanged](
			FARRamenBowlSlotSpec& Slot,
			const EARRamenStationType ExpectedSlotType)
		{
			const EARAffinityColor OldColor = Slot.Color;
			const EARRamenStationType OldSlotType = Slot.SlotType;
			Slot.Color = Slot.Color == EARAffinityColor::Unknown ? EARAffinityColor::None : Slot.Color;
			Slot.SlotType = ExpectedSlotType;
			bChanged = bChanged || Slot.Color != OldColor || Slot.SlotType != OldSlotType || SanitizeMeatQualityTier(Slot.QualityTier);
		};

		for (FARVendingStockedBowlEntry& Entry : Entries)
		{
			bChanged = bChanged || SanitizeMeatQualityTier(Entry.QualityTier);
			SanitizeBowlSlot(Entry.BowlSpec.Noodles, EARRamenStationType::Noodles);
			SanitizeBowlSlot(Entry.BowlSpec.Broth, EARRamenStationType::Broth);
			SanitizeBowlSlot(Entry.BowlSpec.Toppings, EARRamenStationType::Toppings);
		}

		if (bChanged)
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("PendingVendingStockedBowls contained invalid bowl slot data and was normalized."));
		}
	};

	ClampNonNegative(Money, TEXT("Money"));
	ClampNonNegative(Scrap, TEXT("Scrap"));
	ClampNonNegative(Cycles, TEXT("Cycles"));
	ClampNonNegative(FactionClout, TEXT("FactionClout"));
	ClampNonNegative(ActiveRunBuffCycleId, TEXT("ActiveRunBuffCycleId"));
	for (FARMeatTypeAmount& Entry : Meat.AdditionalAmountsByType)
	{
		ClampNonNegative(Entry.Amount, TEXT("Meat.AdditionalAmountsByType.Amount"));
		if (Entry.MeatColor == EARAffinityColor::Unknown)
		{
			Entry.MeatColor = EARAffinityColor::None;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("Meat.AdditionalAmountsByType.MeatColor normalized from Unknown to None."));
		}

		if (SanitizeMeatQualityTier(Entry.MeatQualityTier))
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("Meat.AdditionalAmountsByType.MeatQualityTier reset to Standard."));
		}
	}
	Meat.NormalizeAdditionalAmounts();

	for (FARPlayerStateSaveData& PlayerData : PlayerStates)
	{
		SanitizeTagContainer(PlayerData.ProgressionTags, TEXT("PlayerState.ProgressionTags"));
		const FGameplayTag OldTag = PlayerData.CurrentCharacterTag;
		PlayerData.SyncCharacterSelectionFromCurrentTag();
		if (PlayerData.CurrentCharacterTag != OldTag)
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("PlayerState current character tag was normalized."));
		}
	}

	SanitizeTagContainer(Unlocks, TEXT("Unlocks"));
	if (const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>())
	{
		const FGameplayTagContainer UnlocksBeforeDefaults = Unlocks;
		Unlocks.AppendTags(LoadoutSettings->GetEffectiveDefaultStartingUnlocks());
		if (!(Unlocks == UnlocksBeforeDefaults))
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("Unlocks was missing one or more default starting unlock tags and was normalized."));
		}
	}
	SanitizeTagContainer(ProgressionTags, TEXT("ProgressionTags"));
	SanitizeTagContainer(ActiveFactionEffectTags, TEXT("ActiveFactionEffectTags"));
	SanitizeTagContainer(DialogueCompletedConversationTagsByGame, TEXT("DialogueCompletedConversationTagsByGame"));
	SanitizeStackArray(StoredEnergyDrinkStacks, TEXT("StoredEnergyDrinkStacks"));
	SanitizeStackArray(QueuedEnergyDrinkStacks, TEXT("QueuedEnergyDrinkStacks"));
	SanitizeShopTransientCarryables(ShopTransientCarryables);
	SanitizeVendingStockedBowls(PendingVendingStockedBowls);

	for (int32 PayloadIndex = ActiveRunBuffPayloads.Num() - 1; PayloadIndex >= 0; --PayloadIndex)
	{
		FARRunBuffActivePayload& Payload = ActiveRunBuffPayloads[PayloadIndex];
		if (!Payload.ItemTag.IsValid())
		{
			ActiveRunBuffPayloads.RemoveAtSwap(PayloadIndex);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("ActiveRunBuffPayloads contained an invalid ItemTag and was removed."));
			continue;
		}

		if (Payload.AppliedCount < 1)
		{
			Payload.AppliedCount = 1;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("ActiveRunBuffPayloads.AppliedCount was clamped to 1."));
		}

		Payload.GameplayEffects.RemoveAll([](const TSubclassOf<UGameplayEffect>& EffectClass)
		{
			return EffectClass == nullptr;
		});
		SanitizeTagContainer(Payload.GrantedTags, TEXT("ActiveRunBuffPayloads.GrantedTags"));
	}

	TSet<FString> SeenRelationshipPairs;
	for (int32 Index = DialogueSpeakerRelationshipStates.Num() - 1; Index >= 0; --Index)
	{
		FDialogueSpeakerRelationshipState& State = DialogueSpeakerRelationshipStates[Index];
		if (!State.SourceSpeakerTag.IsValid() || !State.TargetSpeakerTag.IsValid())
		{
			DialogueSpeakerRelationshipStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("DialogueSpeakerRelationshipStates contained an invalid source/target speaker pair and was removed."));
			continue;
		}

		const FString PairKey = FString::Printf(TEXT("%s|%s"), *State.SourceSpeakerTag.ToString(), *State.TargetSpeakerTag.ToString());
		if (SeenRelationshipPairs.Contains(PairKey))
		{
			DialogueSpeakerRelationshipStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("DialogueSpeakerRelationshipStates contained duplicate source/target entries and extras were removed."));
			continue;
		}

		SeenRelationshipPairs.Add(PairKey);
	}

	for (int32 CharacterIndex = CharacterStates.Num() - 1; CharacterIndex >= 0; --CharacterIndex)
	{
		FARCharacterSaveData& CharacterState = CharacterStates[CharacterIndex];
		const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(CharacterState.CharacterTag);
		if (!NormalizedTag.IsValid())
		{
			CharacterStates.RemoveAtSwap(CharacterIndex);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("CharacterStates contained an invalid CharacterTag and it was removed."));
			continue;
		}

		if (CharacterState.CharacterTag != NormalizedTag)
		{
			CharacterState.CharacterTag = NormalizedTag;
			++ClampedCount;
		}

		SanitizeTagContainer(CharacterState.LoadoutTags, TEXT("CharacterStates.LoadoutTags"));
		SanitizeTagContainer(CharacterState.InvaderRuntime.ActivatedUpgradeTags, TEXT("CharacterStates.InvaderRuntime.ActivatedUpgradeTags"));
		SanitizeTagContainer(CharacterState.DialogueState.ProgressionTags, TEXT("CharacterStates.DialogueState.ProgressionTags"));
		SanitizeTagContainer(CharacterState.DialogueState.CompletedConversationTags, TEXT("CharacterStates.DialogueState.CompletedConversationTags"));
		SanitizeTagContainer(CharacterState.DialogueState.SeenConversationTagsThisCycle, TEXT("CharacterStates.DialogueState.SeenConversationTagsThisCycle"));
		SanitizeTagContainer(CharacterState.DialogueState.SkippedConversationTagsThisCycle, TEXT("CharacterStates.DialogueState.SkippedConversationTagsThisCycle"));
		for (int32 OfferIndex = CharacterState.DialogueState.SpeakerOfferCountsThisCycle.Num() - 1; OfferIndex >= 0; --OfferIndex)
		{
			const FDialogueSpeakerCycleOfferCount& OfferCount = CharacterState.DialogueState.SpeakerOfferCountsThisCycle[OfferIndex];
			if (!OfferCount.SpeakerTag.IsValid() || OfferCount.OfferCount <= 0)
			{
				CharacterState.DialogueState.SpeakerOfferCountsThisCycle.RemoveAtSwap(OfferIndex);
				++ClampedCount;
				AddWarning(OutWarnings, TEXT("CharacterStates.DialogueState.SpeakerOfferCountsThisCycle contained an invalid entry and it was removed."));
			}
		}

		for (int32 LastOfferedIndex = CharacterState.DialogueState.LastOfferedConversationBySpeakerThisCycle.Num() - 1; LastOfferedIndex >= 0; --LastOfferedIndex)
		{
			const FDialogueSpeakerCycleLastOfferedConversation& LastOffered = CharacterState.DialogueState.LastOfferedConversationBySpeakerThisCycle[LastOfferedIndex];
			if (!LastOffered.SpeakerTag.IsValid() || !LastOffered.ConversationTag.IsValid())
			{
				CharacterState.DialogueState.LastOfferedConversationBySpeakerThisCycle.RemoveAtSwap(LastOfferedIndex);
				++ClampedCount;
				AddWarning(OutWarnings, TEXT("CharacterStates.DialogueState.LastOfferedConversationBySpeakerThisCycle contained an invalid entry and it was removed."));
			}
		}

		for (int32 ChoiceIndex = CharacterState.DialogueState.CompletedChoiceRecords.Num() - 1; ChoiceIndex >= 0; --ChoiceIndex)
		{
			const FDialogueChoiceMemoryRecord& Record = CharacterState.DialogueState.CompletedChoiceRecords[ChoiceIndex];
			if (!Record.ConversationTag.IsValid() || !Record.ChoiceNodeId.IsValid() || !Record.SelectedBranchId.IsValid())
			{
				CharacterState.DialogueState.CompletedChoiceRecords.RemoveAtSwap(ChoiceIndex);
				++ClampedCount;
				AddWarning(OutWarnings, TEXT("CharacterStates contained an invalid choice-memory record and it was removed."));
			}
		}

		if (CharacterState.ShopSnapshot.bHasCharacterTransform && !CharacterState.ShopSnapshot.CharacterTransform.IsValid())
		{
			CharacterState.ShopSnapshot.bHasCharacterTransform = false;
			CharacterState.ShopSnapshot.CharacterTransform = FTransform::Identity;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("CharacterStates.ShopSnapshot contained an invalid transform and it was cleared."));
		}

		if (CharacterState.ShopSnapshot.bHasHeldItem)
		{
			SanitizeHeldShopItem(CharacterState.ShopSnapshot.HeldItem, TEXT("CharacterStates.ShopSnapshot.HeldItem"));
			if (CharacterState.ShopSnapshot.HeldItem.ActorClass.IsNull())
			{
				CharacterState.ShopSnapshot.bHasHeldItem = false;
				CharacterState.ShopSnapshot.HeldItem = FARCharacterHeldShopItemSnapshot();
				++ClampedCount;
				AddWarning(OutWarnings, TEXT("CharacterStates.ShopSnapshot had bHasHeldItem=true with no actor class and it was cleared."));
			}
		}

		CharacterState.CoreAttributes.MaxHealth = FMath::Max(0.0f, CharacterState.CoreAttributes.MaxHealth);
		CharacterState.CoreAttributes.Health = FMath::Clamp(CharacterState.CoreAttributes.Health, 0.0f, CharacterState.CoreAttributes.MaxHealth);
		CharacterState.CoreAttributes.MaxSpice = FMath::Max(0.0f, CharacterState.CoreAttributes.MaxSpice);
		CharacterState.CoreAttributes.Spice = FMath::Clamp(CharacterState.CoreAttributes.Spice, 0.0f, CharacterState.CoreAttributes.MaxSpice);
		CharacterState.CoreAttributes.MoveSpeed = FMath::Max(0.0f, CharacterState.CoreAttributes.MoveSpeed);
		CharacterState.CoreAttributes.Strength = FMath::Max(0.0f, CharacterState.CoreAttributes.Strength);

		if (CharacterState.bIsDeadState && CharacterState.bIsDowned)
		{
			CharacterState.bIsDowned = false;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("CharacterStates had bIsDeadState and bIsDowned both true; bIsDowned was cleared."));
		}

		if (CharacterState.InvaderRuntime.PlayerColor == EARAffinityColor::Unknown)
		{
			CharacterState.InvaderRuntime.PlayerColor = EARAffinityColor::None;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("CharacterStates.InvaderRuntime.PlayerColor had Unknown and was normalized to None."));
		}

		if (CharacterState.InvaderRuntime.ComboCount < 0)
		{
			CharacterState.InvaderRuntime.ComboCount = 0;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("CharacterStates.InvaderRuntime.ComboCount was negative and clamped to 0."));
		}

		if (CharacterState.InvaderRuntime.SpicyTrackCursorTier < 0)
		{
			CharacterState.InvaderRuntime.SpicyTrackCursorTier = 0;
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("CharacterStates.InvaderRuntime.SpicyTrackCursorTier was negative and clamped to 0."));
		}
	}

	TSet<FGameplayTag> SeenFactions;
	for (int32 Index = FactionPopularityStates.Num() - 1; Index >= 0; --Index)
	{
		FParleyFactionRuntimeState& State = FactionPopularityStates[Index];
		if (!State.FactionTag.IsValid())
		{
			FactionPopularityStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("FactionPopularityStates contained an invalid FactionTag and was removed."));
			continue;
		}

		if (SeenFactions.Contains(State.FactionTag))
		{
			FactionPopularityStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("FactionPopularityStates contained duplicate FactionTag entries and extras were removed."));
			continue;
		}

		SeenFactions.Add(State.FactionTag);
	}

	if (ActiveFactionTag.IsValid() && !SeenFactions.Contains(ActiveFactionTag))
	{
		FParleyFactionRuntimeState ActiveEntry;
		ActiveEntry.FactionTag = ActiveFactionTag;
		FactionPopularityStates.Add(ActiveEntry);
		++ClampedCount;
		AddWarning(OutWarnings, TEXT("ActiveFactionTag was missing from FactionPopularityStates and was auto-added."));
	}

	TSet<FString> SeenFactionSpeakerPairs;
	for (int32 Index = FactionSpeakerReputationStates.Num() - 1; Index >= 0; --Index)
	{
		FParleyFactionSpeakerReputationState& State = FactionSpeakerReputationStates[Index];
		if (!State.FactionTag.IsValid() || !State.SpeakerTag.IsValid())
		{
			FactionSpeakerReputationStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("FactionSpeakerReputationStates contained an invalid tag pair and was removed."));
			continue;
		}

		const FString PairKey = FString::Printf(TEXT("%s|%s"), *State.FactionTag.ToString(), *State.SpeakerTag.ToString());
		if (SeenFactionSpeakerPairs.Contains(PairKey))
		{
			FactionSpeakerReputationStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("FactionSpeakerReputationStates contained duplicate faction/speaker entries and extras were removed."));
			continue;
		}

		SeenFactionSpeakerPairs.Add(PairKey);
	}

	if (SaveSlotNumber < 0)
	{
		SaveSlotNumber = 0;
		++ClampedCount;
		AddWarning(OutWarnings, TEXT("SaveSlotNumber was negative and clamped to 0."));
	}

	return ClampedCount;
}
