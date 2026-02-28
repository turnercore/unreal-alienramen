#include "ARSaveGame.h"

UARSaveGame::UARSaveGame()
{
	SaveGameVersion = CurrentSchemaVersion;
}

FInstancedStruct UARSaveGame::GetPlayerStateDataInstancedStructByIndex(const int32 Index) const
{
	if (!PlayerStates.IsValidIndex(Index))
	{
		return FInstancedStruct();
	}
	return PlayerStates[Index].PlayerStateData;
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
	for (int32 i = 0; i < PlayerStates.Num(); ++i)
	{
		if (PlayerStates[i].Identity.Matches(Identity))
		{
			OutData = PlayerStates[i];
			OutIndex = i;
			return true;
		}
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
	ClampNonNegative(Material, TEXT("Material"));
	ClampNonNegative(Cycles, TEXT("Cycles"));
	ClampNonNegative(Meat.RedAmount, TEXT("Meat.RedAmount"));
	ClampNonNegative(Meat.BlueAmount, TEXT("Meat.BlueAmount"));
	ClampNonNegative(Meat.WhiteAmount, TEXT("Meat.WhiteAmount"));
	ClampNonNegative(Meat.UnspecifiedAmount, TEXT("Meat.UnspecifiedAmount"));

	for (TPair<FGameplayTag, int32>& Pair : Meat.AdditionalAmountsByType)
	{
		ClampNonNegative(Pair.Value, TEXT("Meat.AdditionalAmountsByType"));
	}

	for (FARPlayerStateSaveData& PlayerData : PlayerStates)
	{
		ClampNonNegative(PlayerData.Identity.LegacyId, TEXT("PlayerState.Identity.LegacyId"));
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
