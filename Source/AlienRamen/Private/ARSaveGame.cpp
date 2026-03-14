#include "ARSaveGame.h"

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

	static bool ContainsChoiceRecord(
		const TArray<FDialogueChoiceMemoryRecord>& Records,
		const FDialogueChoiceMemoryRecord& Candidate)
	{
		for (const FDialogueChoiceMemoryRecord& Existing : Records)
		{
			if (Existing.ConversationTag == Candidate.ConversationTag
				&& Existing.ChoiceNodeId == Candidate.ChoiceNodeId
				&& Existing.SelectedBranchId == Candidate.SelectedBranchId)
			{
				return true;
			}
		}

		return false;
	}

	static int32 MergeDialoguePersistentState(
		FDialoguePlayerPersistentState& Target,
		const FDialoguePlayerPersistentState& Source)
	{
		int32 ChangeCount = 0;

		const int32 OldProgressionCount = Target.ProgressionTags.Num();
		Target.ProgressionTags.AppendTags(Source.ProgressionTags);
		ChangeCount += (Target.ProgressionTags.Num() != OldProgressionCount) ? 1 : 0;

		const int32 OldCompletedCount = Target.CompletedConversationTags.Num();
		Target.CompletedConversationTags.AppendTags(Source.CompletedConversationTags);
		ChangeCount += (Target.CompletedConversationTags.Num() != OldCompletedCount) ? 1 : 0;

		const int32 OldSeenCount = Target.SeenConversationTagsThisCycle.Num();
		Target.SeenConversationTagsThisCycle.AppendTags(Source.SeenConversationTagsThisCycle);
		ChangeCount += (Target.SeenConversationTagsThisCycle.Num() != OldSeenCount) ? 1 : 0;

		const int32 OldSkippedCount = Target.SkippedConversationTagsThisCycle.Num();
		Target.SkippedConversationTagsThisCycle.AppendTags(Source.SkippedConversationTagsThisCycle);
		ChangeCount += (Target.SkippedConversationTagsThisCycle.Num() != OldSkippedCount) ? 1 : 0;

		for (const FDialogueChoiceMemoryRecord& Record : Source.CompletedChoiceRecords)
		{
			if (!ContainsChoiceRecord(Target.CompletedChoiceRecords, Record))
			{
				Target.CompletedChoiceRecords.Add(Record);
				++ChangeCount;
			}
		}

		for (const FDialogueSpeakerCycleOfferCount& SourceCount : Source.SpeakerOfferCountsThisCycle)
		{
			if (!SourceCount.SpeakerTag.IsValid() || SourceCount.OfferCount <= 0)
			{
				continue;
			}

			bool bFound = false;
			for (FDialogueSpeakerCycleOfferCount& TargetCount : Target.SpeakerOfferCountsThisCycle)
			{
				if (!TargetCount.SpeakerTag.MatchesTagExact(SourceCount.SpeakerTag))
				{
					continue;
				}

				bFound = true;
				const int32 OldCount = TargetCount.OfferCount;
				TargetCount.OfferCount = FMath::Max(OldCount, SourceCount.OfferCount);
				ChangeCount += (TargetCount.OfferCount != OldCount) ? 1 : 0;
				break;
			}

			if (!bFound)
			{
				Target.SpeakerOfferCountsThisCycle.Add(SourceCount);
				++ChangeCount;
			}
		}

		return ChangeCount;
	}

	static bool IsValidCharacterTag(const FGameplayTag CharacterTag)
	{
		return ARPlayer::GetCharacterChoiceForTag(CharacterTag) != EARCharacterChoice::None;
	}

	static FString ResolveDefaultMapPathForModeTag(const FGameplayTag ModeTag)
	{
		if (!ModeTag.IsValid())
		{
			return FString();
		}

		const FGameplayTag LobbyModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Lobby"), false);
		if (LobbyModeTag.IsValid() && ModeTag.MatchesTagExact(LobbyModeTag))
		{
			return TEXT("/Game/Maps/Lvl_MultiplayerLobby");
		}

		const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
		if (ShopModeTag.IsValid() && ModeTag.MatchesTagExact(ShopModeTag))
		{
			return TEXT("/Game/Maps/Lvl_RamenShop");
		}

		const FGameplayTag InvaderModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
		if (InvaderModeTag.IsValid() && ModeTag.MatchesTagExact(InvaderModeTag))
		{
			return TEXT("/Game/Maps/Lvl_Invader");
		}

		const FGameplayTag ScrapyardModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
		if (ScrapyardModeTag.IsValid() && ModeTag.MatchesTagExact(ScrapyardModeTag))
		{
			return TEXT("/Game/Maps/Lvl_Scrapyard");
		}

		const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
		if (TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag))
		{
			return TEXT("/Game/Maps/Lvl_Loading");
		}

		return FString();
	}

static FGameplayTag ResolveLegacyDialogueCharacterTag(
	UARSaveGame* SaveGame,
	const FDialoguePlayerPersistentState& LegacyState)
{
	if (!SaveGame)
	{
		return FGameplayTag();
	}

	if (IsValidCharacterTag(LegacyState.CharacterTag))
	{
		return ARPlayer::NormalizeCharacterTag(LegacyState.CharacterTag);
	}

	FARPlayerStateSaveData MatchedPlayerState;
	const EARPlayerSlot LegacySlot = ARPlayer::GetPlayerSlotForTag(LegacyState.OwnerPlayerSlotTag);
	if (LegacySlot != EARPlayerSlot::Unknown)
	{
		int32 PlayerIndex = INDEX_NONE;
		if (SaveGame->FindPlayerStateDataBySlot(LegacySlot, MatchedPlayerState, PlayerIndex))
		{
			return MatchedPlayerState.ResolveCurrentCharacterTag();
		}

		return ARPlayer::GetDefaultCharacterTagForSlot(LegacySlot);
	}

	return FGameplayTag();
}
}

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

int32 UARSaveGame::MigrateToCurrentSchema(TArray<FString>* OutWarnings)
{
	int32 ChangeCount = 0;
	if (SaveGameVersion >= CurrentSchemaVersion)
	{
		return ChangeCount;
	}

	if (SaveGameVersion <= 13)
	{
		for (FARPlayerStateSaveData& PlayerData : PlayerStates)
		{
			const FGameplayTag OldTag = PlayerData.CurrentCharacterTag;
			const EARCharacterChoice OldChoice = PlayerData.CharacterPicked;
			PlayerData.SyncCharacterSelectionFromCurrentTag();
			ChangeCount += (PlayerData.CurrentCharacterTag != OldTag || PlayerData.CharacterPicked != OldChoice) ? 1 : 0;
		}
	}

	if (SaveGameVersion <= 14)
	{
		const FGameplayTag DialogueProgressionRootTag = FGameplayTag::RequestGameplayTag(TEXT("Progression.Dialogue"), false);
		bool bMigratedDialogueProgressionTags = false;
		if (DialogueProgressionRootTag.IsValid())
		{
			for (const FARPlayerStateSaveData& PlayerData : PlayerStates)
			{
				const FGameplayTag CharacterTag = PlayerData.ResolveCurrentCharacterTag();
				if (!CharacterTag.IsValid())
				{
					continue;
				}

				FARCharacterSaveData& CharacterState = FindOrAddCharacterStateData(CharacterTag);
				const int32 OldProgressionCount = CharacterState.DialogueState.ProgressionTags.Num();
				for (const FGameplayTag Tag : PlayerData.ProgressionTags)
				{
					if (Tag.IsValid() && Tag.MatchesTag(DialogueProgressionRootTag))
					{
						CharacterState.DialogueState.ProgressionTags.AddTag(Tag);
					}
				}

				if (CharacterState.DialogueState.ProgressionTags.Num() != OldProgressionCount)
				{
					bMigratedDialogueProgressionTags = true;
					++ChangeCount;
				}
			}
		}

		if (bMigratedDialogueProgressionTags)
		{
			AddWarning(
				OutWarnings,
				TEXT("Migrated dialogue progression tags from player-owned progression into character dialogue state."));
		}
	}

	if (SaveGameVersion <= 11)
	{
		int32 MigratedDialogueRows = 0;
		int32 MergedDialogueRows = 0;
		int32 DroppedDialogueRows = 0;
		for (const FDialoguePlayerPersistentState& LegacyState : DialoguePlayerPersistentStates)
		{
			const FGameplayTag CharacterTag = ResolveLegacyDialogueCharacterTag(this, LegacyState);
			if (!IsValidCharacterTag(CharacterTag))
			{
				++DroppedDialogueRows;
				continue;
			}

			int32 ExistingIndex = INDEX_NONE;
			FARCharacterSaveData* Existing = FindCharacterStateDataMutable(CharacterTag, ExistingIndex);
			FARCharacterSaveData& CharacterData = Existing ? *Existing : FindOrAddCharacterStateData(CharacterTag);
			if (Existing)
			{
				++MergedDialogueRows;
			}
			else
			{
				++MigratedDialogueRows;
			}

			ChangeCount += MergeDialoguePersistentState(CharacterData.DialogueState, LegacyState);
		}

		if (!DialoguePlayerPersistentStates.IsEmpty())
		{
			DialoguePlayerPersistentStates.Reset();
			++ChangeCount;
		}

		if (MigratedDialogueRows > 0 || MergedDialogueRows > 0 || DroppedDialogueRows > 0)
		{
			AddWarningf(
				OutWarnings,
				FString::Printf(
					TEXT("Migrated legacy dialogue player state rows to character state (created=%d merged=%d dropped=%d)."),
					MigratedDialogueRows,
					MergedDialogueRows,
					DroppedDialogueRows));
		}

		if (!LastSavedModeTag.IsValid())
		{
			const FGameplayTag DefaultShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
			if (DefaultShopModeTag.IsValid())
			{
				LastSavedModeTag = DefaultShopModeTag;
				++ChangeCount;
				AddWarning(OutWarnings, TEXT("Legacy save had no saved mode tag; migration defaulted it to Mode.Shop."));
			}
		}

		if (LastSavedMapPath.IsEmpty())
		{
			FString BackfilledMapPath = ResolveDefaultMapPathForModeTag(LastSavedModeTag);
			if (BackfilledMapPath.IsEmpty())
			{
				BackfilledMapPath = TEXT("/Game/Maps/Lvl_RamenShop");
			}

			LastSavedMapPath = BackfilledMapPath;
			++ChangeCount;
			AddWarningf(
				OutWarnings,
				FString::Printf(
					TEXT("Legacy save had no saved map path; migration defaulted it to '%s'."),
					*LastSavedMapPath));
		}
	}

	SaveGameVersion = CurrentSchemaVersion;
	++ChangeCount;
	AddWarning(OutWarnings, TEXT("Save schema was migrated to the current ownership model."));

	return ChangeCount;
}

int32 UARSaveGame::ValidateAndSanitize(TArray<FString>* OutWarnings)
{
	int32 ClampedCount = 0;
	ClampedCount += MigrateToCurrentSchema(OutWarnings);

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

		if (bChanged)
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("ShopTransientCarryables contained invalid data and was sanitized."));
		}
	};

	auto SanitizeHeldShopItem =
		[OutWarnings, &ClampedCount](FARCharacterHeldShopItemSnapshot& Snapshot, const TCHAR* FieldName)
	{
		if (Snapshot.MeatAmount < 1)
		{
			Snapshot.MeatAmount = 1;
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.MeatAmount was clamped to 1."), FieldName));
		}

		if (Snapshot.BowlFillStep < 0)
		{
			Snapshot.BowlFillStep = 0;
			++ClampedCount;
			AddWarningf(OutWarnings, FString::Printf(TEXT("%s.BowlFillStep was clamped to 0."), FieldName));
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
		SanitizeTagContainer(PlayerData.ProgressionTags, TEXT("PlayerState.ProgressionTags"));
		const FGameplayTag OldTag = PlayerData.CurrentCharacterTag;
		const EARCharacterChoice OldChoice = PlayerData.CharacterPicked;
		PlayerData.SyncCharacterSelectionFromCurrentTag();
		if (PlayerData.CurrentCharacterTag != OldTag || PlayerData.CharacterPicked != OldChoice)
		{
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("PlayerState character selection fields were normalized from compatibility data."));
		}
	}

	SanitizeTagContainer(Unlocks, TEXT("Unlocks"));
	SanitizeTagContainer(ProgressionTags, TEXT("ProgressionTags"));
	SanitizeTagContainer(ActiveFactionEffectTags, TEXT("ActiveFactionEffectTags"));
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

	for (int32 Index = DialogueRelationshipStates.Num() - 1; Index >= 0; --Index)
	{
		if (!DialogueRelationshipStates[Index].SpeakerTag.IsValid())
		{
			DialogueRelationshipStates.RemoveAtSwap(Index);
			++ClampedCount;
			AddWarning(OutWarnings, TEXT("DialogueRelationshipStates contained an invalid SpeakerTag and was removed."));
		}
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
	}

	// Legacy root retained only for migration compatibility; sanitize and clear if stale rows remain.
	for (FDialoguePlayerPersistentState& PlayerDialogueState : DialoguePlayerPersistentStates)
	{
		SanitizeTagContainer(PlayerDialogueState.ProgressionTags, TEXT("DialoguePlayerPersistentStates.ProgressionTags"));
		SanitizeTagContainer(PlayerDialogueState.CompletedConversationTags, TEXT("DialoguePlayerPersistentStates.CompletedConversationTags"));
		SanitizeTagContainer(PlayerDialogueState.SeenConversationTagsThisCycle, TEXT("DialoguePlayerPersistentStates.SeenConversationTagsThisCycle"));
		SanitizeTagContainer(PlayerDialogueState.SkippedConversationTagsThisCycle, TEXT("DialoguePlayerPersistentStates.SkippedConversationTagsThisCycle"));
		for (int32 OfferIndex = PlayerDialogueState.SpeakerOfferCountsThisCycle.Num() - 1; OfferIndex >= 0; --OfferIndex)
		{
			const FDialogueSpeakerCycleOfferCount& OfferCount = PlayerDialogueState.SpeakerOfferCountsThisCycle[OfferIndex];
			if (!OfferCount.SpeakerTag.IsValid() || OfferCount.OfferCount <= 0)
			{
				PlayerDialogueState.SpeakerOfferCountsThisCycle.RemoveAtSwap(OfferIndex);
				++ClampedCount;
				AddWarning(OutWarnings, TEXT("DialoguePlayerPersistentStates.SpeakerOfferCountsThisCycle contained an invalid entry and it was removed."));
			}
		}

		for (int32 ChoiceIndex = PlayerDialogueState.CompletedChoiceRecords.Num() - 1; ChoiceIndex >= 0; --ChoiceIndex)
		{
			const FDialogueChoiceMemoryRecord& Record = PlayerDialogueState.CompletedChoiceRecords[ChoiceIndex];
			if (!Record.ConversationTag.IsValid() || !Record.ChoiceNodeId.IsValid() || !Record.SelectedBranchId.IsValid())
			{
				PlayerDialogueState.CompletedChoiceRecords.RemoveAtSwap(ChoiceIndex);
				++ClampedCount;
				AddWarning(OutWarnings, TEXT("DialoguePlayerPersistentStates contained an invalid choice-memory record and it was removed."));
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
		FARFactionRuntimeState ActiveEntry;
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
