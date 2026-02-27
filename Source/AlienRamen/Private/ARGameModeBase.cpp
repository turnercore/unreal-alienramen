#include "ARGameModeBase.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "GameFramework/PlayerController.h"

EARPlayerSlot AARGameModeBase::DetermineNextPlayerSlot(const AARGameStateBase* GameState)
{
	if (!GameState)
	{
		return EARPlayerSlot::P1;
	}

	bool bHasP1 = false;
	bool bHasP2 = false;
	for (AARPlayerStateBase* Player : GameState->GetTrackedPlayers())
	{
		if (!Player)
		{
			continue;
		}

		if (Player->GetPlayerSlot() == EARPlayerSlot::P1)
		{
			bHasP1 = true;
		}
		else if (Player->GetPlayerSlot() == EARPlayerSlot::P2)
		{
			bHasP2 = true;
		}
	}

	if (!bHasP1)
	{
		return EARPlayerSlot::P1;
	}

	return EARPlayerSlot::P2;
}

void AARGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}

	AARPlayerStateBase* JoinedPS = NewPlayer->GetPlayerState<AARPlayerStateBase>();
	AARGameStateBase* GS = GetGameState<AARGameStateBase>();
	if (!JoinedPS || !GS)
	{
		return;
	}

	GS->AddTrackedPlayer(JoinedPS);

	if (!JoinedPS->IsSetupComplete())
	{
		JoinedPS->SetPlayerSlot(DetermineNextPlayerSlot(GS));
		JoinedPS->SetIsSetupComplete(true);
	}

	BP_OnPlayerJoined(JoinedPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player joined: %s (Slot=%d, Setup=%s)"), *GetNameSafe(JoinedPS), static_cast<int32>(JoinedPS->GetPlayerSlot()), JoinedPS->IsSetupComplete() ? TEXT("true") : TEXT("false"));
}

void AARGameModeBase::Logout(AController* Exiting)
{
	AARPlayerStateBase* LeavingPS = Exiting ? Exiting->GetPlayerState<AARPlayerStateBase>() : nullptr;
	if (HasAuthority())
	{
		if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
		{
			GS->RemoveTrackedPlayer(LeavingPS);
		}
	}

	BP_OnPlayerLeft(LeavingPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player left: %s"), *GetNameSafe(LeavingPS));

	Super::Logout(Exiting);
}

void AARGameModeBase::BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState)
{
}

void AARGameModeBase::BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState)
{
}
