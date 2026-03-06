#include "ARShopGameMode.h"

#include "ARFactionSubsystem.h"
#include "ARLog.h"
#include "Engine/GameInstance.h"

AARShopGameMode::AARShopGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
}

bool AARShopGameMode::PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks)
{
	if (!Super::PreStartTravel(URL, Options, bSkipReadyChecks))
	{
		return false;
	}

	UARFactionSubsystem* FactionSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARFactionSubsystem>() : nullptr;
	if (!FactionSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] PreStartTravel failed: FactionSubsystem missing."));
		return false;
	}

	FGameplayTag WinnerFactionTag;
	EARFactionWinnerReason Reason = EARFactionWinnerReason::NoValidFactions;
	const bool bFinalized = FactionSubsystem->FinalizeElectionForTravel(WinnerFactionTag, Reason);
	if (!bFinalized)
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] PreStartTravel blocked: faction election finalize failed."));
		return false;
	}

	UE_LOG(
		ARLog,
		Log,
		TEXT("[ShopGameMode] Faction election finalized. Winner='%s' Reason=%d"),
		*WinnerFactionTag.ToString(),
		static_cast<int32>(Reason));
	return true;
}
