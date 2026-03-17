#include "ARShopGameState.h"

#include "ARGameModeBase.h"
#include "ARGameStateModeStructs.h"
#include "ARLog.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AARShopGameState::AARShopGameState()
{
	ClassStateStruct = FARShopGameStateData::StaticStruct();
}

UScriptStruct* AARShopGameState::GetStateStruct_Implementation() const
{
	return ClassStateStruct ? ClassStateStruct.Get() : FARShopGameStateData::StaticStruct();
}

void AARShopGameState::SetBaseBowlPayout(const int32 NewBaseBowlPayout)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 SanitizedPayout = FMath::Max(0, NewBaseBowlPayout);
	if (BaseBowlPayout == SanitizedPayout)
	{
		return;
	}

	const int32 OldBaseBowlPayout = BaseBowlPayout;
	BaseBowlPayout = SanitizedPayout;
	OnRep_BaseBowlPayout(OldBaseBowlPayout);
	ForceNetUpdate();
}

void AARShopGameState::OnRep_BaseBowlPayout(const int32 OldBaseBowlPayout)
{
	(void)OldBaseBowlPayout;
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

void AARShopGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARShopGameState, BaseBowlPayout);
}
