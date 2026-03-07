#include "ARGameModeBase.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARNetworkUserSettings.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "ARSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"

AARGameModeBase::AARGameModeBase()
{
	bUseSeamlessTravel = true;
	DefaultPlayerName = FText::FromString(TEXT("Tenshu"));
}

void AARGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (const UARNetworkUserSettings* NetworkSettings = GetDefault<UARNetworkUserSettings>())
	{
		if (NetworkSettings->bStayOffline && UniqueId.IsValid())
		{
			const FString NetIdType = UniqueId->GetType().ToString();
			const bool bIsLocalStyleId = NetIdType.Equals(TEXT("NULL"), ESearchCase::IgnoreCase)
				|| NetIdType.Equals(TEXT("INVALID"), ESearchCase::IgnoreCase)
				|| NetIdType.Equals(TEXT("UNSET"), ESearchCase::IgnoreCase);

			if (!bIsLocalStyleId)
			{
				ErrorMessage = TEXT("Server is offline.");
				UE_LOG(ARLog, Warning, TEXT("[GameMode] PreLogin denied connection from '%s': Stay Offline is enabled (NetIdType=%s)."), *Address, *NetIdType);
				return;
			}
		}
	}

	const AARGameStateBase* GS = GetGameState<AARGameStateBase>();
	if (!GS)
	{
		return;
	}

	int32 PlayerCount = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (Cast<AARPlayerStateBase>(PS))
		{
			++PlayerCount;
		}
	}

	if (PlayerCount >= 2)
	{
		ErrorMessage = TEXT("Server full.");
		UE_LOG(ARLog, Warning, TEXT("[GameMode] PreLogin denied connection from '%s': player cap reached (%d)."), *Address, PlayerCount);
		return;
	}
}

void AARGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
	{
		NormalizeConnectedPlayersIdentity(GS);
		GS->SetManualSaveAllowed(bAllowManualSaveInMode);
		GS->SetShareLocalPauseAcrossControllers(bShareLocalPauseAcrossControllersInMode);
	}
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

EARPlayerSlot AARGameModeBase::FindFirstFreePlayerSlot(const AARGameStateBase* GameState, const AARPlayerStateBase* IgnorePlayerState)
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
		if (!Player || Player == IgnorePlayerState)
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

	if (!bHasP2)
	{
		return EARPlayerSlot::P2;
	}

	return EARPlayerSlot::Unknown;
}

EARAffinityColor AARGameModeBase::ResolveExpectedInvaderPlayerColor(const EARCharacterChoice CharacterChoice, const EARPlayerSlot PlayerSlot)
{
	switch (CharacterChoice)
	{
	case EARCharacterChoice::Brother:
		return EARAffinityColor::Blue;
	case EARCharacterChoice::Sister:
		return EARAffinityColor::Red;
	default:
		// Mode-load baseline should still be a valid team color even if character is temporarily unset.
		return (PlayerSlot == EARPlayerSlot::P2) ? EARAffinityColor::Red : EARAffinityColor::Blue;
	}
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

	// Never force unset; keep current assignment if both choices are occupied.
	UE_LOG(ARLog, Warning, TEXT("[GameMode] Character conflict unresolved for '%s'; keeping current assignment %d."),
		*GetNameSafe(CurrentPlayerState), static_cast<int32>(CurrentChoice));
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

void AARGameModeBase::EnsureJoinedPlayerHasUniqueSlot(AARGameStateBase* InGameState, AARPlayerStateBase* JoinedPlayerState) const
{
	if (!InGameState || !JoinedPlayerState)
	{
		return;
	}

	const EARPlayerSlot CurrentSlot = JoinedPlayerState->GetPlayerSlot();
	bool bSlotConflict = false;

	if (CurrentSlot != EARPlayerSlot::Unknown)
	{
		for (APlayerState* PS : InGameState->PlayerArray)
		{
			const AARPlayerStateBase* OtherPlayer = Cast<AARPlayerStateBase>(PS);
			if (!OtherPlayer || OtherPlayer == JoinedPlayerState)
			{
				continue;
			}

			if (OtherPlayer->GetPlayerSlot() == CurrentSlot)
			{
				bSlotConflict = true;
				break;
			}
		}
	}

	if (CurrentSlot != EARPlayerSlot::Unknown && !bSlotConflict)
	{
		return;
	}

	const EARPlayerSlot ResolvedSlot = FindFirstFreePlayerSlot(InGameState, JoinedPlayerState);
	if (ResolvedSlot == EARPlayerSlot::Unknown)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] Could not resolve unique slot for '%s' (CurrentSlot=%d Conflict=%d)."),
			*GetNameSafe(JoinedPlayerState), static_cast<int32>(CurrentSlot), bSlotConflict ? 1 : 0);
		return;
	}

	if (ResolvedSlot != CurrentSlot)
	{
		JoinedPlayerState->SetPlayerSlot(ResolvedSlot);
		UE_LOG(ARLog, Log, TEXT("[GameMode] Normalized player slot for '%s': %d -> %d"),
			*GetNameSafe(JoinedPlayerState), static_cast<int32>(CurrentSlot), static_cast<int32>(ResolvedSlot));
	}
}

void AARGameModeBase::NormalizeConnectedPlayersIdentity(AARGameStateBase* InGameState) const
{
	if (!InGameState)
	{
		return;
	}

	TArray<AARPlayerStateBase*> Players;
	for (APlayerState* PS : InGameState->PlayerArray)
	{
		if (AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS))
		{
			Players.Add(Player);
		}
	}

	if (Players.IsEmpty())
	{
		return;
	}

	// Slot normalization: preserve first valid occupant, reassign only invalid/conflicting slots.
	bool bP1Taken = false;
	bool bP2Taken = false;
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player)
		{
			continue;
		}

		EARPlayerSlot CurrentSlot = Player->GetPlayerSlot();
		const bool bCurrentIsP1 = CurrentSlot == EARPlayerSlot::P1;
		const bool bCurrentIsP2 = CurrentSlot == EARPlayerSlot::P2;
		const bool bCurrentTaken = (bCurrentIsP1 && bP1Taken) || (bCurrentIsP2 && bP2Taken);
		const bool bCurrentValid = bCurrentIsP1 || bCurrentIsP2;

		if (!bCurrentValid || bCurrentTaken)
		{
			EARPlayerSlot NewSlot = EARPlayerSlot::Unknown;
			if (!bP1Taken)
			{
				NewSlot = EARPlayerSlot::P1;
			}
			else if (!bP2Taken)
			{
				NewSlot = EARPlayerSlot::P2;
			}

			if (NewSlot != EARPlayerSlot::Unknown && NewSlot != CurrentSlot)
			{
				Player->SetPlayerSlot(NewSlot);
				UE_LOG(ARLog, Log, TEXT("[GameMode] Identity normalize slot for '%s': %d -> %d"),
					*GetNameSafe(Player), static_cast<int32>(CurrentSlot), static_cast<int32>(NewSlot));
				CurrentSlot = NewSlot;
			}
		}

		if (CurrentSlot == EARPlayerSlot::P1)
		{
			bP1Taken = true;
		}
		else if (CurrentSlot == EARPlayerSlot::P2)
		{
			bP2Taken = true;
		}
	}

	// Character normalization: fill None from slot preference where possible, then resolve conflicts.
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player || Player->GetCharacterPicked() != EARCharacterChoice::None)
		{
			continue;
		}

		const EARCharacterChoice PreferredChoice =
			(Player->GetPlayerSlot() == EARPlayerSlot::P2) ? EARCharacterChoice::Sister : EARCharacterChoice::Brother;
		const EARCharacterChoice AlternateChoice = GetAlternateCharacterChoice(PreferredChoice);

		if (!IsCharacterChoiceTakenByOther(InGameState, Player, PreferredChoice))
		{
			Player->SetCharacterPicked(PreferredChoice);
		}
		else if (AlternateChoice != EARCharacterChoice::None && !IsCharacterChoiceTakenByOther(InGameState, Player, AlternateChoice))
		{
			Player->SetCharacterPicked(AlternateChoice);
		}
		else
		{
			// Never leave character unset after load/join normalization.
			Player->SetCharacterPicked(PreferredChoice);
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Character normalize fallback for '%s': both choices occupied, assigning slot-biased %d."),
				*GetNameSafe(Player),
				static_cast<int32>(PreferredChoice));
		}
	}

	for (AARPlayerStateBase* Player : Players)
	{
		ResolveCharacterChoiceConflict(InGameState, Player);
	}

	// Color normalization: keep player color synchronized with character assignment.
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player)
		{
			continue;
		}

		const EARAffinityColor ExpectedColor = ResolveExpectedInvaderPlayerColor(Player->GetCharacterPicked(), Player->GetPlayerSlot());
		const EARAffinityColor CurrentColor = Player->GetInvaderPlayerColor();
		if (CurrentColor != ExpectedColor)
		{
			Player->SetInvaderPlayerColor(ExpectedColor);
			UE_LOG(ARLog, Verbose, TEXT("[GameMode] Identity normalize color for '%s': %d -> %d"),
				*GetNameSafe(Player), static_cast<int32>(CurrentColor), static_cast<int32>(ExpectedColor));
		}
	}
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

	// Enforce stable unique slot occupancy even when setup is already complete (for example seamless travel/copy paths).
	EnsureJoinedPlayerHasUniqueSlot(GS, JoinedPS);
	NormalizeConnectedPlayersIdentity(GS);

	BP_OnPlayerJoined(JoinedPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player joined: %s (Slot=%d, Setup=%s)"), *GetNameSafe(JoinedPS), static_cast<int32>(JoinedPS->GetPlayerSlot()), JoinedPS->IsSetupComplete() ? TEXT("true") : TEXT("false"));

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UARSessionSubsystem>())
		{
			FARSessionResult SessionResult;
			if (!SessionSubsystem->RefreshJoinability(SessionResult) && SessionResult.ResultCode != EARSessionResultCode::SessionNotFound)
			{
				UE_LOG(ARLog, Verbose, TEXT("[GameMode] Session joinability refresh after join failed: %s"), *SessionResult.Error);
			}
		}
	}
}

void AARGameModeBase::Logout(AController* Exiting)
{
	AARPlayerStateBase* LeavingPS = Exiting ? Exiting->GetPlayerState<AARPlayerStateBase>() : nullptr;

	BP_OnPlayerLeft(LeavingPS);
	UE_LOG(ARLog, Log, TEXT("[GameMode] Player left: %s"), *GetNameSafe(LeavingPS));

	Super::Logout(Exiting);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UARSessionSubsystem>())
		{
			FARSessionResult SessionResult;
			if (!SessionSubsystem->RefreshJoinability(SessionResult) && SessionResult.ResultCode != EARSessionResultCode::SessionNotFound)
			{
				UE_LOG(ARLog, Verbose, TEXT("[GameMode] Session joinability refresh after leave failed: %s"), *SessionResult.Error);
			}
		}
	}
}

void AARGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Quit && bAutosaveOnQuit)
	{
		if (UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
		{
			FARSaveResult SaveResult;
			const bool bSaved = SaveSubsystem->RequestAutosaveIfDirty(true, SaveResult);
			if (bSaved)
			{
				UE_LOG(ARLog, Log, TEXT("[GameMode] Autosave-on-quit succeeded for '%s' (Slot=%s Rev=%d)."),
					*GetNameSafe(this), *SaveResult.SlotName.ToString(), SaveResult.SlotNumber);
			}
			else if (!SaveResult.Error.IsEmpty())
			{
				UE_LOG(ARLog, Warning, TEXT("[GameMode] Autosave-on-quit failed for '%s': %s"),
					*GetNameSafe(this), *SaveResult.Error);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AARGameModeBase::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel ignored: not authority."));
		return false;
	}

	if (!PreStartTravel(URL, Options, bSkipReadyChecks))
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel blocked by PreStartTravel hook."));
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

#if WITH_EDITOR
	if (bUseOpenLevelInPIE && GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		FString LevelName = URL;
		FString OpenLevelOptions = Options;
		int32 QueryIndex = INDEX_NONE;
		if (URL.FindChar(TEXT('?'), QueryIndex))
		{
			LevelName = URL.Left(QueryIndex);
			if (OpenLevelOptions.IsEmpty())
			{
				OpenLevelOptions = URL.Mid(QueryIndex + 1);
			}
		}

		bool bHasListenOption = false;
		if (!OpenLevelOptions.IsEmpty())
		{
			TArray<FString> OptionTokens;
			OpenLevelOptions.ParseIntoArray(OptionTokens, TEXT("?"), true);
			for (const FString& Token : OptionTokens)
			{
				if (Token.TrimStartAndEnd().Equals(TEXT("listen"), ESearchCase::IgnoreCase))
				{
					bHasListenOption = true;
					break;
				}
			}
		}

		if (!bHasListenOption)
		{
			OpenLevelOptions = OpenLevelOptions.IsEmpty()
				? FString(TEXT("listen"))
				: FString::Printf(TEXT("%s?listen"), *OpenLevelOptions);
		}

		UE_LOG(ARLog, Log, TEXT("[GameMode] TryStartTravel PIE fallback -> OpenLevel Level='%s' Options='%s'"), *LevelName, *OpenLevelOptions);
		return SaveSubsystem->RequestOpenLevel(LevelName, OpenLevelOptions, bSkipReadyChecks, bAbsolute, bSaveOnModeExit);
	}
#endif

	UE_LOG(
		ARLog,
		Log,
		TEXT("[GameMode] TryStartTravel -> URL='%s' Options='%s' SkipReady=%s SaveOnExit=%s"),
		*URL,
		*Options,
		bSkipReadyChecks ? TEXT("true") : TEXT("false"),
		bSaveOnModeExit ? TEXT("true") : TEXT("false"));
	return SaveSubsystem->RequestServerTravel(TravelURL, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bSaveOnModeExit);
}

bool AARGameModeBase::PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks)
{
	(void)URL;
	(void)Options;
	(void)bSkipReadyChecks;
	return true;
}

void AARGameModeBase::BP_OnPlayerJoined_Implementation(AARPlayerStateBase* JoinedPlayerState)
{
}

void AARGameModeBase::BP_OnPlayerLeft_Implementation(AARPlayerStateBase* LeftPlayerState)
{
}
