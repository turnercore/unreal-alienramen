#include "ARGameModeBase.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARNetworkUserSettings.h"
#include "ParleySpeakerSubsystem.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "ARSessionSubsystem.h"
#include "ARTransitionTypes.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"

namespace
{
	static bool ShouldRouteViaTransitionMap(const bool bModeDefaultRouteEnabled, const EARTravelRoutePolicy RoutePolicy)
	{
		switch (RoutePolicy)
		{
		case EARTravelRoutePolicy::ForceTransitionMap:
			return true;
		case EARTravelRoutePolicy::ForceDirect:
			return false;
		case EARTravelRoutePolicy::ModeDefault:
		default:
			return bModeDefaultRouteEnabled;
		}
	}

	static FString AppendTravelOptions(const FString& BaseURL, const FString& Options)
	{
		if (Options.IsEmpty())
		{
			return BaseURL;
		}

		FString TrimmedOptions = Options.TrimStartAndEnd();
		if (TrimmedOptions.IsEmpty())
		{
			return BaseURL;
		}

		if (TrimmedOptions.StartsWith(TEXT("?")) || TrimmedOptions.StartsWith(TEXT("&")))
		{
			// Normalize any leading '&' to '?' — UE travel URLs use repeated '?' (Map?A=1?B=2).
			TrimmedOptions[0] = TEXT('?');
			return BaseURL + TrimmedOptions;
		}

		// Always use '?' as the joiner for travel options (Map?A=1?B=2).
		return FString::Printf(TEXT("%s?%s"), *BaseURL, *TrimmedOptions);
	}
}

AARGameModeBase::AARGameModeBase()
{
	bUseSeamlessTravel = true;
	DefaultPlayerName = FText::FromString(TEXT("Tenshu"));
}

FString AARGameModeBase::BuildModeTravelURL(const FString& DestinationURL, const EARTravelRoutePolicy RoutePolicy) const
{
	if (!ShouldRouteViaTransitionMap(bRouteModeTravelThroughTransitionMap, RoutePolicy))
	{
		return DestinationURL;
	}

	if (DestinationURL.IsEmpty() || TransitionTravelMapURL.IsEmpty())
	{
		if (TransitionTravelMapURL.IsEmpty() && RoutePolicy == EARTravelRoutePolicy::ForceTransitionMap)
		{
			UE_LOG(ARLog, Warning, TEXT("[GameMode] BuildModeTravelURL: ForceTransitionMap requested but TransitionTravelMapURL is empty on '%s'; falling back to direct travel."), *GetNameSafe(this));
		}
		return DestinationURL;
	}

	const FString TransitionOptionTokenA = FString::Printf(TEXT("?%s="), ARTransition::OptionDestinationURL);
	const FString TransitionOptionTokenB = FString::Printf(TEXT("&%s="), ARTransition::OptionDestinationURL);
	if (DestinationURL.Contains(TransitionOptionTokenA, ESearchCase::IgnoreCase) || DestinationURL.Contains(TransitionOptionTokenB, ESearchCase::IgnoreCase))
	{
		return DestinationURL;
	}

	auto ExtractMapPath = [](const FString& URL) -> FString
	{
		int32 QueryIndex = INDEX_NONE;
		if (URL.FindChar(TEXT('?'), QueryIndex))
		{
			return URL.Left(QueryIndex);
		}

		return URL;
	};

	const FString DestinationMapPath = ExtractMapPath(DestinationURL);
	const FString TransitionMapPath = ExtractMapPath(TransitionTravelMapURL);
	if (DestinationMapPath.Equals(TransitionMapPath, ESearchCase::IgnoreCase))
	{
		return DestinationURL;
	}

	FARTransitionContext TransitionContext;
	TransitionContext.SourceMode = TransitionSourceMode;
	TransitionContext.Reason = TransitionReason;
	TransitionContext.DestinationURL = DestinationURL;
	const FString WrappedURL = ARTransition::BuildTransitionTravelURL(TransitionTravelMapURL, TransitionContext);
	return WrappedURL.IsEmpty() ? DestinationURL : WrappedURL;
}

bool AARGameModeBase::EndModeAndTravel(const FString& URL, const FString& Options, const bool bSkipReadyChecks, const bool bAbsolute, const bool bSkipGameNotify, const bool bUseOpenLevelInPIE)
{
	return TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, EARTravelRoutePolicy::ForceTransitionMap);
}

bool AARGameModeBase::TravelDirectInMode(const FString& URL, const FString& Options, const bool bSkipReadyChecks, const bool bAbsolute, const bool bSkipGameNotify, const bool bUseOpenLevelInPIE)
{
	return TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, EARTravelRoutePolicy::ForceDirect);
}

TSubclassOf<AGameSession> AARGameModeBase::GetGameSessionClass() const
{
	// Keep runtime independent from BP-configured AdvancedGameSession classes.
	return AGameSession::StaticClass();
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
				ErrorMessage = TEXT("Server has StayOffline enabled.");
				UE_LOG(ARLog, Warning, TEXT("[GameMode] PreLogin denied connection from '%s': StayOffline is enabled (NetIdType=%s)."), *Address, *NetIdType);
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

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
			const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
			if (SaveSubsystem->HasPendingFreshLoadEntry()
				&& ModeTag != TransitionModeTag
				&& ModeTag != ShopModeTag)
			{
				SaveSubsystem->ClearPendingFreshLoadEntry();
			}
		}

		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}

		if (UARSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UARSessionSubsystem>())
		{
			FARSessionResult SessionResult;
			if (!SessionSubsystem->RefreshJoinability(SessionResult) && SessionResult.ResultCode != EARSessionResultCode::SessionNotFound)
			{
				UE_LOG(ARLog, Verbose, TEXT("[GameMode] Session joinability refresh on mode begin failed: %s"), *SessionResult.Error);
			}
		}
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

	if (!bHasP2)
	{
		return EARPlayerSlot::P2;
	}

	return EARPlayerSlot::Unknown;
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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
	}

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
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}

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

bool AARGameModeBase::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE, const EARTravelRoutePolicy RoutePolicy)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel ignored: not authority."));
		return false;
	}

	if (URL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: URL is empty."));
		return false;
	}

	if (!PreStartTravel(URL, Options, bSkipReadyChecks))
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel blocked by PreStartTravel hook."));
		return false;
	}

	if (!GameState)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: missing GameState."));
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

	FString TravelURL = BuildModeTravelURL(URL, RoutePolicy);
	TravelURL = AppendTravelOptions(TravelURL, Options);

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
		FString LevelName = TravelURL;
		FString OpenLevelOptions;
		int32 QueryIndex = INDEX_NONE;
		if (TravelURL.FindChar(TEXT('?'), QueryIndex))
		{
			LevelName = TravelURL.Left(QueryIndex);
			OpenLevelOptions = TravelURL.Mid(QueryIndex + 1);
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
		TEXT("[GameMode] TryStartTravel -> RequestedURL='%s' FinalURL='%s' Options='%s' RoutePolicy=%s SkipReady=%s SaveOnExit=%s"),
		*URL,
		*TravelURL,
		*Options,
		*ARTransition::LexToString(RoutePolicy),
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
