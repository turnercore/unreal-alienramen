#include "ARShopGameState.h"

#include "ARGameModeBase.h"
#include "ARGameStateModeStructs.h"
#include "ARLog.h"
#include "Engine/World.h"

AARShopGameState::AARShopGameState()
{
	ClassStateStruct = FARShopGameStateData::StaticStruct();
}

UScriptStruct* AARShopGameState::GetStateStruct_Implementation() const
{
	return ClassStateStruct ? ClassStateStruct.Get() : FARShopGameStateData::StaticStruct();
}

bool AARShopGameState::FinalizeShopRunAndTravelToInvader(const FString& InInvaderTravelURL)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FString InvaderTravelURL = InInvaderTravelURL.IsEmpty() ? DefaultInvaderTravelURL : InInvaderTravelURL;
	if (InvaderTravelURL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[Shop] Finalization travel aborted: destination URL is empty."));
		return false;
	}

	AARGameModeBase* GameMode = GetWorld() ? Cast<AARGameModeBase>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!GameMode)
	{
		UE_LOG(ARLog, Warning, TEXT("[Shop] Finalization travel failed: authority GameMode is not AARGameModeBase."));
		return false;
	}

	if (!GameMode->EndModeAndTravel(InvaderTravelURL))
	{
		UE_LOG(ARLog, Warning, TEXT("[Shop] Finalization travel failed for URL '%s'."), *InvaderTravelURL);
		return false;
	}

	return true;
}
