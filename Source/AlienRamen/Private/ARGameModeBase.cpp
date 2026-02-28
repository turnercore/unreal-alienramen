#include "ARGameModeBase.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"

AARGameModeBase::AARGameModeBase()
{
	bUseSeamlessTravel = true;
	DefaultPlayerName = TEXT("Tenshu");
}

EARPlayerSlot AARGameModeBase::DetermineNextPlayerSlot(const AARGameStateBase* GameState)
{
	if (!GameState)
	{
		return EARPlayerSlot::P1;
	}

	bool bHasP1 = false;
	bool bHasP2 = false;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
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

EARCharacterChoice AARGameModeBase::GetAlternateCharacterChoice(const EARCharacterChoice CurrentChoice)
{
	switch (CurrentChoice)
	{
	case EARCharacterChoice::Brother:
		return EARCharacterChoice::Sister;
	case EARCharacterChoice::Sister:
		return EARCharacterChoice::Brother;
	default:
		return EARCharacterChoice::None;
	}
}

bool AARGameModeBase::IsCharacterChoiceTakenByOther(const AARGameStateBase* InGameState, const AARPlayerStateBase* CurrentPlayerState, const EARCharacterChoice CharacterChoice)
{
	if (!InGameState || !CurrentPlayerState || CharacterChoice == EARCharacterChoice::None)
	{
		return false;
	}

	for (APlayerState* PS : InGameState->PlayerArray)
	{
		AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
		if (!Player || Player == CurrentPlayerState)
		{
			continue;
		}

		if (Player->GetCharacterPicked() == CharacterChoice)
		{
			return true;
		}
	}

	return false;
}

void AARGameModeBase::ResolveCharacterChoiceConflict(const AARGameStateBase* InGameState, AARPlayerStateBase* CurrentPlayerState) const
{
	if (!InGameState || !CurrentPlayerState)
	{
		return;
	}

	const EARCharacterChoice CurrentChoice = CurrentPlayerState->GetCharacterPicked();
	if (CurrentChoice == EARCharacterChoice::None || !IsCharacterChoiceTakenByOther(InGameState, CurrentPlayerState, CurrentChoice))
	{
		return;
	}

	const EARCharacterChoice AlternateChoice = GetAlternateCharacterChoice(CurrentChoice);
	if (AlternateChoice != EARCharacterChoice::None && !IsCharacterChoiceTakenByOther(InGameState, CurrentPlayerState, AlternateChoice))
	{
		CurrentPlayerState->SetCharacterPicked(AlternateChoice);
		UE_LOG(ARLog, Log, TEXT("[GameMode] Character conflict resolved for '%s': %d -> %d"), *GetNameSafe(CurrentPlayerState), static_cast<int32>(CurrentChoice), static_cast<int32>(AlternateChoice));
		return;
	}

	CurrentPlayerState->SetCharacterPicked(EARCharacterChoice::None);
	UE_LOG(ARLog, Warning, TEXT("[GameMode] Character conflict unresolved for '%s'; falling back to None."), *GetNameSafe(CurrentPlayerState));
}

void AARGameModeBase::HandleFirstSessionJoinSetup(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState, UARSaveSubsystem* SaveSubsystem) const
{
	if (!InGameState || !JoinedPlayerState)
	{
		return;
	}

	const EARPlayerSlot AssignedSlot = DetermineNextPlayerSlot(InGameState);
	JoinedPlayerState->SetPlayerSlot(AssignedSlot);

	bool bHydratedFromSave = false;
	if (SaveSubsystem)
	{
		bHydratedFromSave = SaveSubsystem->TryHydratePlayerStateFromCurrentSave(JoinedPlayerState, true);
		// Preserve authoritative join-time slot assignment for this session.
		JoinedPlayerState->SetPlayerSlot(AssignedSlot);
	}

	if (!bHydratedFromSave)
	{
		JoinedPlayerState->InitializeForFirstSessionJoin();
	}

	ResolveCharacterChoiceConflict(InGameState, JoinedPlayerState);
	JoinedPlayerState->SetIsSetupComplete(true);
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

	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>();
	}

	if (!JoinedPS->IsSetupComplete())
	{
		HandleFirstSessionJoinSetup(GS, JoinedPS, SaveSubsystem);
	}

	BP_OnPlayerJoined(JoinedPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player joined: %s (Slot=%d, Setup=%s)"), *GetNameSafe(JoinedPS), static_cast<int32>(JoinedPS->GetPlayerSlot()), JoinedPS->IsSetupComplete() ? TEXT("true") : TEXT("false"));
}

void AARGameModeBase::Logout(AController* Exiting)
{
	AARPlayerStateBase* LeavingPS = Exiting ? Exiting->GetPlayerState<AARPlayerStateBase>() : nullptr;

	BP_OnPlayerLeft(LeavingPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player left: %s"), *GetNameSafe(LeavingPS));

	Super::Logout(Exiting);
}

bool AARGameModeBase::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel ignored: not authority."));
		return false;
	}

	TArray<FString> NotReadyPlayers;
	if (!bSkipReadyChecks)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (const AARPlayerStateBase* ARPS = Cast<AARPlayerStateBase>(PS))
			{
				if (!ARPS->IsTravelReady())
				{
					NotReadyPlayers.Add(GetNameSafe(ARPS));
				}
			}
		}
	}

	if (!bSkipReadyChecks && NotReadyPlayers.Num() > 0)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel blocked; not ready: %s"), *FString::Join(NotReadyPlayers, TEXT(", ")));
		return false;
	}

	FString TravelURL = URL;
	if (!Options.IsEmpty())
	{
		TravelURL += Options;
	}

	UARSaveSubsystem* SaveSubsystem = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>();
	}

	if (!SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: SaveSubsystem missing."));
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[GameMode] TryStartTravel -> URL='%s' Options='%s' SkipReady=%s"), *URL, *Options, bSkipReadyChecks ? TEXT("true") : TEXT("false"));
	return SaveSubsystem->RequestServerTravel(TravelURL, bSkipReadyChecks, bAbsolute, bSkipGameNotify);
}

void AARGameModeBase::BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState)
{
}

void AARGameModeBase::BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState)
{
}
