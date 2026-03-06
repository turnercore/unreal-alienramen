#include "ARSaveGame.h"

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

	ClampNonNegative(Money, TEXT("Money"));
	ClampNonNegative(Scrap, TEXT("Scrap"));
	ClampNonNegative(Cycles, TEXT("Cycles"));
	ClampNonNegative(FactionClout, TEXT("FactionClout"));
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

	for (FARNpcRelationshipState& NpcState : NpcRelationshipStates)
	{
		ClampNonNegative(NpcState.LoveRating, TEXT("NpcRelationshipStates.LoveRating"));
	}

	for (int32 Index = NpcRelationshipStates.Num() - 1; Index >= 0; --Index)
	{
		if (!NpcRelationshipStates[Index].NpcTag.IsValid())
		{
			NpcRelationshipStates.RemoveAtSwap(Index);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("NpcRelationshipStates contained an invalid NpcTag and was removed."));
			}
		}
	}

	for (int32 Index = DialogueCanonicalChoiceStates.Num() - 1; Index >= 0; --Index)
	{
		const FARDialogueCanonicalChoiceState& Entry = DialogueCanonicalChoiceStates[Index];
		if (!Entry.NodeTag.IsValid() || !Entry.ChoiceTag.IsValid())
		{
			DialogueCanonicalChoiceStates.RemoveAtSwap(Index);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("DialogueCanonicalChoiceStates contained invalid tags and was removed."));
			}
		}
	}

	TSet<FGameplayTag> SeenChoiceNodes;
	for (int32 Index = DialogueCanonicalChoiceStates.Num() - 1; Index >= 0; --Index)
	{
		const FGameplayTag NodeTag = DialogueCanonicalChoiceStates[Index].NodeTag;
		if (SeenChoiceNodes.Contains(NodeTag))
		{
			DialogueCanonicalChoiceStates.RemoveAtSwap(Index);
			++ClampedCount;
			if (OutWarnings)
			{
				OutWarnings->Add(TEXT("DialogueCanonicalChoiceStates contained duplicate NodeTag entries and extras were removed."));
			}
			continue;
		}
		SeenChoiceNodes.Add(NodeTag);
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
