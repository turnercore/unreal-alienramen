#include "ARGameModeBase.h"

#include "ARGameStateBase.h"
#include "ARLoadoutSettings.h"
#include "ARLog.h"
#include "ARNetworkUserSettings.h"
#include "ARTaggedPlayerStart.h"
#include "ParleySpeakerSubsystem.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "ARSessionSubsystem.h"
#include "ARTravelSubsystem.h"
#include "ARTransitionTypes.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"

namespace
{
	static bool IsIdentityRelevantPlayerState(const AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return false;
		}

		const AController* OwnerController = Cast<AController>(PlayerState->GetOwner());
		return OwnerController && OwnerController->PlayerState == PlayerState;
	}

	static bool AreSameLogicalPlayerState(const AARPlayerStateBase* A, const AARPlayerStateBase* B)
	{
		if (!A || !B)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		const int32 APlayerId = A->GetPlayerId();
		const int32 BPlayerId = B->GetPlayerId();
		if (APlayerId > 0 && BPlayerId > 0 && APlayerId == BPlayerId)
		{
			return true;
		}

		const FUniqueNetIdRepl AUniqueId = A->GetUniqueId();
		const FUniqueNetIdRepl BUniqueId = B->GetUniqueId();
		if (AUniqueId.IsValid() && BUniqueId.IsValid() && AUniqueId == BUniqueId)
		{
			// Treat matching online identities as the same logical player when player-id data
			// is unavailable (common in offline/null-subsystem test flows).
			return (APlayerId <= 0 || BPlayerId <= 0);
		}

		return false;
	}

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

	static bool TryGetFirstSegmentUnderRoot(const FGameplayTag& InTag, const FGameplayTag& RootTag, FString& OutSegment)
	{
		OutSegment.Reset();
		if (!InTag.IsValid() || !RootTag.IsValid())
		{
			return false;
		}

		const FString TagString = InTag.ToString();
		const FString RootString = RootTag.ToString();
		const FString RootPrefix = RootString + TEXT(".");
		if (!TagString.StartsWith(RootPrefix))
		{
			return false;
		}

		const FString Suffix = TagString.Mid(RootPrefix.Len());
		if (Suffix.IsEmpty())
		{
			return false;
		}

		FString FirstSegment;
		if (!Suffix.Split(TEXT("."), &FirstSegment, nullptr))
		{
			FirstSegment = Suffix;
		}

		if (FirstSegment.IsEmpty())
		{
			return false;
		}

		OutSegment = FirstSegment;
		return true;
	}

	static void AddSpawnIdentityMirrorTags(const FGameplayTag& CharacterTag, TArray<FGameplayTag>& InOutTags)
	{
		if (!CharacterTag.IsValid())
		{
			return;
		}

		const FGameplayTag ParleySpeakerRoot = FGameplayTag::RequestGameplayTag(TEXT("Parley.Speaker"), false);
		const FGameplayTag ShopCharacterRoot = FGameplayTag::RequestGameplayTag(TEXT("Shop.Character"), false);
		const FGameplayTag ShopCustomerRoot = FGameplayTag::RequestGameplayTag(TEXT("Shop.Customer"), false);

		FString Segment;
		if (!TryGetFirstSegmentUnderRoot(CharacterTag, ParleySpeakerRoot, Segment)
			&& !TryGetFirstSegmentUnderRoot(CharacterTag, ShopCharacterRoot, Segment)
			&& !TryGetFirstSegmentUnderRoot(CharacterTag, ShopCustomerRoot, Segment))
		{
			return;
		}

		auto TryAppendTag = [&InOutTags](const FGameplayTag& TagToAppend)
		{
			if (!TagToAppend.IsValid() || InOutTags.Contains(TagToAppend))
			{
				return;
			}

			InOutTags.Add(TagToAppend);
		};

		if (ParleySpeakerRoot.IsValid())
		{
			TryAppendTag(FGameplayTag::RequestGameplayTag(
				*FString::Printf(TEXT("%s.%s"), *ParleySpeakerRoot.ToString(), *Segment),
				false));
		}

		if (ShopCharacterRoot.IsValid())
		{
			TryAppendTag(FGameplayTag::RequestGameplayTag(
				*FString::Printf(TEXT("%s.%s"), *ShopCharacterRoot.ToString(), *Segment),
				false));
		}

		if (ShopCustomerRoot.IsValid())
		{
			TryAppendTag(FGameplayTag::RequestGameplayTag(
				*FString::Printf(TEXT("%s.%s"), *ShopCustomerRoot.ToString(), *Segment),
				false));
		}
	}

	static FGameplayTag ResolveCharacterTagForSpawnIdentity(const AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return FGameplayTag();
		}

		const FGameplayTag CanonicalCharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
		const FGameplayTag ChoiceCharacterTag = ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());

		// Runtime can briefly carry mismatched mirrors during join/load flows. Prefer explicit
		// picked character when present so start selection and pawn class stay user-intent aligned.
		if (ChoiceCharacterTag.IsValid())
		{
			if (!CanonicalCharacterTag.IsValid() || !CanonicalCharacterTag.MatchesTagExact(ChoiceCharacterTag))
			{
				return ChoiceCharacterTag;
			}
		}

		if (CanonicalCharacterTag.IsValid())
		{
			return CanonicalCharacterTag;
		}

		return ARPlayer::GetDefaultCharacterTagForSlot(PlayerState->GetPlayerSlot());
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
		if (!IsIdentityRelevantPlayerState(Player))
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
		if (!IsIdentityRelevantPlayerState(Player)
			|| Player == IgnorePlayerState
			|| (IgnorePlayerState && AreSameLogicalPlayerState(Player, IgnorePlayerState)))
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
		if (!IsIdentityRelevantPlayerState(Player) || Player == CurrentPlayerState)
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

	EARPlayerSlot AssignedSlot = FindFirstFreePlayerSlot(InGameState, JoinedPlayerState);
	if (AssignedSlot == EARPlayerSlot::Unknown)
	{
		AssignedSlot = EARPlayerSlot::P1;
	}
	JoinedPlayerState->SetPlayerSlot(AssignedSlot);

	bool bHydratedFromSave = false;
	if (SaveSubsystem)
	{
		// First pass: strict identity only. This avoids forcing slot-biased character data (for example
		// P1->Brother) when a later strict sync will resolve to a different explicit character.
		bHydratedFromSave = SaveSubsystem->TryHydratePlayerStateFromCurrentSave(JoinedPlayerState, false);

		if (!bHydratedFromSave)
		{
			// Only allow slot fallback when there is no explicit character intent present on the runtime
			// player state. If a character is already selected/tagged, preserve that intent.
			const bool bHasExplicitCharacterIntent =
				JoinedPlayerState->GetCharacterPicked() != EARCharacterChoice::None
				|| ARPlayer::NormalizeCharacterTag(JoinedPlayerState->GetCurrentCharacterTag()).IsValid();
			if (!bHasExplicitCharacterIntent)
			{
				bHydratedFromSave = SaveSubsystem->TryHydratePlayerStateFromCurrentSave(JoinedPlayerState, true);
				if (bHydratedFromSave)
				{
					UE_LOG(
						ARLog,
						Verbose,
						TEXT("[GameMode] Applied slot-fallback save hydration for '%s' because no explicit runtime character intent was present."),
						*GetNameSafe(JoinedPlayerState));
				}
			}
		}

		// Preserve authoritative join-time slot assignment for this session.
		JoinedPlayerState->SetPlayerSlot(AssignedSlot);
	}

	if (!bHydratedFromSave)
	{
		JoinedPlayerState->InitializeForFirstSessionJoin();

		// Fresh/no-save join policy: assign a random available canonical character.
		TArray<EARCharacterChoice> AvailableChoices;
		if (!IsCharacterChoiceTakenByOther(InGameState, JoinedPlayerState, EARCharacterChoice::Brother))
		{
			AvailableChoices.Add(EARCharacterChoice::Brother);
		}
		if (!IsCharacterChoiceTakenByOther(InGameState, JoinedPlayerState, EARCharacterChoice::Sister))
		{
			AvailableChoices.Add(EARCharacterChoice::Sister);
		}

		if (AvailableChoices.Num() > 0)
		{
			const int32 RandomIndex = FMath::RandHelper(AvailableChoices.Num());
			const EARCharacterChoice RandomChoice = AvailableChoices[RandomIndex];
			if (RandomChoice != EARCharacterChoice::None && RandomChoice != JoinedPlayerState->GetCharacterPicked())
			{
				JoinedPlayerState->SetCharacterPicked(RandomChoice);
			}
		}
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
			if (!IsIdentityRelevantPlayerState(OtherPlayer)
				|| OtherPlayer == JoinedPlayerState
				|| AreSameLogicalPlayerState(OtherPlayer, JoinedPlayerState))
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
		const EARPlayerSlot AppliedSlot = JoinedPlayerState->GetPlayerSlot();
		if (AppliedSlot == ResolvedSlot)
		{
			UE_LOG(ARLog, Log, TEXT("[GameMode] Normalized player slot for '%s': %d -> %d"),
				*GetNameSafe(JoinedPlayerState), static_cast<int32>(CurrentSlot), static_cast<int32>(AppliedSlot));
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[GameMode] Slot normalization could not apply for '%s': requested %d from %d, actual=%d."),
				*GetNameSafe(JoinedPlayerState), static_cast<int32>(ResolvedSlot), static_cast<int32>(CurrentSlot), static_cast<int32>(AppliedSlot));
		}
	}
}

FGameplayTag AARGameModeBase::GetPendingSpawnCharacterTagForController(const AController* Controller) const
{
	if (!Controller)
	{
		return FGameplayTag();
	}

	const TWeakObjectPtr<const AController> ControllerKey(Controller);
	if (const FGameplayTag* FoundTag = PendingSpawnCharacterTagsByController.Find(ControllerKey))
	{
		return *FoundTag;
	}

	return FGameplayTag();
}

void AARGameModeBase::CachePendingSpawnCharacterTagForController(const AController* Controller, const FGameplayTag& CharacterTag)
{
	for (auto It = PendingSpawnCharacterTagsByController.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (!Controller)
	{
		return;
	}

	const TWeakObjectPtr<const AController> ControllerKey(Controller);
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		PendingSpawnCharacterTagsByController.Remove(ControllerKey);
		return;
	}

	PendingSpawnCharacterTagsByController.FindOrAdd(ControllerKey) = NormalizedCharacterTag;
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
			if (IsIdentityRelevantPlayerState(Player))
			{
				Players.Add(Player);
			}
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
		EARPlayerSlot TargetSlot = CurrentSlot;
		const bool bCurrentIsP1 = CurrentSlot == EARPlayerSlot::P1;
		const bool bCurrentIsP2 = CurrentSlot == EARPlayerSlot::P2;
		const bool bCurrentTaken = (bCurrentIsP1 && bP1Taken) || (bCurrentIsP2 && bP2Taken);
		const bool bCurrentValid = bCurrentIsP1 || bCurrentIsP2;

		if (!bCurrentValid || bCurrentTaken)
		{
			if (!bP1Taken)
			{
				TargetSlot = EARPlayerSlot::P1;
			}
			else if (!bP2Taken)
			{
				TargetSlot = EARPlayerSlot::P2;
			}
			else
			{
				TargetSlot = EARPlayerSlot::Unknown;
			}
		}

		if (TargetSlot != EARPlayerSlot::Unknown && TargetSlot != CurrentSlot)
		{
			Player->SetPlayerSlot(TargetSlot);
			const EARPlayerSlot AppliedSlot = Player->GetPlayerSlot();
			UE_LOG(
				ARLog,
				Log,
				TEXT("[GameMode] Identity normalize slot for '%s': %d -> %d"),
				*GetNameSafe(Player),
				static_cast<int32>(CurrentSlot),
				static_cast<int32>(AppliedSlot));
			CurrentSlot = AppliedSlot;
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

	// Canonical-tag normalization: never leave CurrentCharacterTag invalid, and keep Brother/Sister unique when possible.
	for (AARPlayerStateBase* Player : Players)
	{
		if (!Player)
		{
			continue;
		}

		const EARPlayerSlot Slot = Player->GetPlayerSlot();
		const EARCharacterChoice PreferredChoice =
			(Slot == EARPlayerSlot::P2) ? EARCharacterChoice::Sister : EARCharacterChoice::Brother;
		const EARCharacterChoice AlternateChoice = GetAlternateCharacterChoice(PreferredChoice);

		EARCharacterChoice ResolvedChoice = ARPlayer::GetCharacterChoiceForTag(
			ARPlayer::NormalizeCharacterTag(Player->GetCurrentCharacterTag()));
		const EARCharacterChoice MirroredChoice = Player->GetCharacterPicked();
		if (MirroredChoice != EARCharacterChoice::None)
		{
			if (ResolvedChoice == EARCharacterChoice::None || ResolvedChoice != MirroredChoice)
			{
				ResolvedChoice = MirroredChoice;
			}
		}
		if (ResolvedChoice == EARCharacterChoice::None)
		{
			ResolvedChoice = PreferredChoice;
		}

		if (IsCharacterChoiceTakenByOther(InGameState, Player, ResolvedChoice))
		{
			if (AlternateChoice != EARCharacterChoice::None && !IsCharacterChoiceTakenByOther(InGameState, Player, AlternateChoice))
			{
				ResolvedChoice = AlternateChoice;
			}
			else
			{
				ResolvedChoice = PreferredChoice;
			}
		}

		const FGameplayTag ResolvedTag = ARPlayer::GetCharacterTagForChoice(ResolvedChoice);
		if (ResolvedTag.IsValid() && !Player->GetCurrentCharacterTag().MatchesTagExact(ResolvedTag))
		{
			Player->SetCurrentCharacterTag(ResolvedTag);
			UE_LOG(
				ARLog,
				Log,
				TEXT("[GameMode] Identity normalize character tag for '%s': -> %s"),
				*GetNameSafe(Player),
				*ResolvedTag.ToString());
		}
	}

	// Loadout normalization: gameplay modes require at least one ship tag in loadout.
	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	const bool bIsTransitionMode = TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag);
	if (!bIsTransitionMode)
	{
		const FGameplayTag ShipRootTag = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Ship"), false);
		const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
		FGameplayTag DefaultShipTag;
		if (ShipRootTag.IsValid() && LoadoutSettings)
		{
			for (const FGameplayTag& DefaultTag : LoadoutSettings->DefaultPlayerLoadoutTags)
			{
				if (DefaultTag.IsValid() && DefaultTag.MatchesTag(ShipRootTag))
				{
					DefaultShipTag = DefaultTag;
					break;
				}
			}
		}

		for (AARPlayerStateBase* Player : Players)
		{
			if (!Player)
			{
				continue;
			}

			bool bHasShipTag = false;
			TArray<FGameplayTag> PlayerLoadoutTags;
			Player->LoadoutTags.GetGameplayTagArray(PlayerLoadoutTags);
			for (const FGameplayTag& LoadoutTag : PlayerLoadoutTags)
			{
				if (LoadoutTag.IsValid() && ShipRootTag.IsValid() && LoadoutTag.MatchesTag(ShipRootTag))
				{
					bHasShipTag = true;
					break;
				}
			}

			if (bHasShipTag)
			{
				continue;
			}

			if (DefaultShipTag.IsValid())
			{
				Player->UpdateLoadoutWithTag(DefaultShipTag);
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[GameMode] Repaired missing ship loadout tag for '%s' by applying default '%s'."),
					*GetNameSafe(Player),
					*DefaultShipTag.ToString());
			}
			else
			{
				UE_LOG(
					ARLog,
					Error,
					TEXT("[GameMode] Player '%s' has no ship loadout tag and no default ship tag is configured in LoadoutSettings."),
					*GetNameSafe(Player));
			}
		}
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
	if (!HasAuthority() || !NewPlayer)
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	AARPlayerStateBase* JoinedPS = NewPlayer->GetPlayerState<AARPlayerStateBase>();
	AARGameStateBase* GS = GetGameState<AARGameStateBase>();
	if (!JoinedPS || !GS)
	{
		CachePendingSpawnCharacterTagForController(NewPlayer, FGameplayTag());
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
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
	const EARPlayerSlot PreSuperSlot = JoinedPS->GetPlayerSlot();
	const EARCharacterChoice PreSuperChoice = JoinedPS->GetCharacterPicked();
	const FGameplayTag PreSuperCharacterTag = ResolveCharacterTagForSpawnIdentity(JoinedPS);
	CachePendingSpawnCharacterTagForController(NewPlayer, PreSuperCharacterTag);

	// Identity must be normalized before Super so spawn class/start selection sees non-unknown slot/character.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Re-run once after Super to catch any mirrored-state drift during spawn/possess setup.
	EnsureJoinedPlayerHasUniqueSlot(GS, JoinedPS);
	NormalizeConnectedPlayersIdentity(GS);
	const EARPlayerSlot PostSuperSlot = JoinedPS->GetPlayerSlot();
	const EARCharacterChoice PostSuperChoice = JoinedPS->GetCharacterPicked();
	const FGameplayTag PostSuperCharacterTag = ResolveCharacterTagForSpawnIdentity(JoinedPS);

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[GameMode] HandleStartingNewPlayer identity after Super for '%s': Slot %d->%d Choice %d->%d CharacterTag %s->%s Pawn=%s."),
		*GetNameSafe(JoinedPS),
		static_cast<int32>(PreSuperSlot),
		static_cast<int32>(PostSuperSlot),
		static_cast<int32>(PreSuperChoice),
		static_cast<int32>(PostSuperChoice),
		*PreSuperCharacterTag.ToString(),
		*PostSuperCharacterTag.ToString(),
		*GetNameSafe(NewPlayer->GetPawn()));

	const bool bIdentityDriftedDuringInitialSpawn = !PostSuperCharacterTag.MatchesTagExact(PreSuperCharacterTag);
	if (bIdentityDriftedDuringInitialSpawn)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] HandleStartingNewPlayer detected spawn identity drift for '%s' (CharacterTag %s -> %s). Correcting spawn with RestartPlayer."),
			*GetNameSafe(JoinedPS),
			*PreSuperCharacterTag.ToString(),
			*PostSuperCharacterTag.ToString());
	}

	// Recovery: if initial spawn failed OR identity drifted after Super normalization,
	// perform one corrective restart using the finalized post-Super character tag.
	if (!NewPlayer->GetPawn() || bIdentityDriftedDuringInitialSpawn)
	{
		CachePendingSpawnCharacterTagForController(NewPlayer, PostSuperCharacterTag);

		if (bIdentityDriftedDuringInitialSpawn)
		{
			if (APawn* ExistingPawn = NewPlayer->GetPawn())
			{
				NewPlayer->UnPossess();
				ExistingPawn->Destroy();
			}
		}

		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] HandleStartingNewPlayer running corrective RestartPlayer for '%s' (HasPawn=%d Slot=%d CharacterTag=%s)."),
			*GetNameSafe(JoinedPS),
			NewPlayer->GetPawn() ? 1 : 0,
			static_cast<int32>(PostSuperSlot),
			*PostSuperCharacterTag.ToString());
		RestartPlayer(NewPlayer);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UParleySpeakerSubsystem* SpeakerSubsystem = GI->GetSubsystem<UParleySpeakerSubsystem>())
		{
			SpeakerSubsystem->RefreshAllSpeakerTalkableStates();
		}
	}

	const EARPlayerSlot SlotBeforeBPJoin = JoinedPS->GetPlayerSlot();
	const FGameplayTag CharacterTagBeforeBPJoin = ResolveCharacterTagForSpawnIdentity(JoinedPS);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[GameMode] Player joined (pre-BP): %s (Slot=%d, CharacterTag=%s, Setup=%s)"),
		*GetNameSafe(JoinedPS),
		static_cast<int32>(SlotBeforeBPJoin),
		*CharacterTagBeforeBPJoin.ToString(),
		JoinedPS->IsSetupComplete() ? TEXT("true") : TEXT("false"));

	BP_OnPlayerJoined(JoinedPS);

	const EARPlayerSlot SlotAfterBPJoin = JoinedPS->GetPlayerSlot();
	const FGameplayTag CharacterTagAfterBPJoin = ResolveCharacterTagForSpawnIdentity(JoinedPS);
	if (SlotAfterBPJoin != SlotBeforeBPJoin || !CharacterTagAfterBPJoin.MatchesTagExact(CharacterTagBeforeBPJoin))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] BP_OnPlayerJoined changed identity for '%s' (Slot %d->%d CharacterTag %s->%s)."),
			*GetNameSafe(JoinedPS),
			static_cast<int32>(SlotBeforeBPJoin),
			static_cast<int32>(SlotAfterBPJoin),
			*CharacterTagBeforeBPJoin.ToString(),
			*CharacterTagAfterBPJoin.ToString());
	}

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

	CachePendingSpawnCharacterTagForController(NewPlayer, FGameplayTag());
}

void AARGameModeBase::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	if (!HasAuthority())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(C);
	if (!PlayerController)
	{
		return;
	}

	if (PlayerControllerClass && !PlayerController->IsA(PlayerControllerClass))
	{
		const FVector ReplacementSpawnLocation = PlayerController->GetPawn()
			? PlayerController->GetPawn()->GetActorLocation()
			: FVector::ZeroVector;
		// Controller handoff should not author gameplay-facing spawn orientation; destination pawn
		// classes own their own orientation policy during spawn/possess.
		const FRotator ReplacementSpawnRotation = FRotator::ZeroRotator;
		APlayerController* ReplacementController = SpawnPlayerControllerCommon(
			PlayerController->GetRemoteRole(),
			ReplacementSpawnLocation,
			ReplacementSpawnRotation,
			PlayerControllerClass);

		if (ReplacementController)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Seamless travel controller class mismatch in mode '%s': replacing '%s' with '%s'."),
				*ModeTag.ToString(),
				*GetNameSafe(PlayerController->GetClass()),
				*GetNameSafe(PlayerControllerClass));

			SwapPlayerControllers(PlayerController, ReplacementController);
			C = ReplacementController;
			PlayerController = ReplacementController;
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[GameMode] Seamless travel controller class mismatch in mode '%s': expected '%s', got '%s' for '%s', and replacement spawn failed."),
				*ModeTag.ToString(),
				*GetNameSafe(PlayerControllerClass),
				*GetNameSafe(PlayerController->GetClass()),
				*GetNameSafe(PlayerController));
		}
	}

	if (PlayerController->PlayerState)
	{
		PlayerController->PlayerState->SetIsSpectator(false);
	}

	const bool bWasSpectating = PlayerController->IsInState(NAME_Spectating);
	PlayerController->ChangeState(NAME_Playing);

	const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	const bool bIsTransitionMode = TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag);
	APawn* ExistingPawn = PlayerController->GetPawn();
	const bool bHasSpectatorPawn = ExistingPawn && ExistingPawn->IsA(ASpectatorPawn::StaticClass());
	const bool bNeedsGameplayPawn = !bIsTransitionMode && (!ExistingPawn || bHasSpectatorPawn || bWasSpectating);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[GameMode] HandleSeamlessTravelPlayer mode='%s' controller='%s' class='%s' pawn='%s' wasSpectating=%d hasSpectatorPawn=%d needsGameplayPawn=%d."),
		*ModeTag.ToString(),
		*GetNameSafe(PlayerController),
		*GetNameSafe(PlayerController->GetClass()),
		*GetNameSafe(ExistingPawn),
		bWasSpectating ? 1 : 0,
		bHasSpectatorPawn ? 1 : 0,
		bNeedsGameplayPawn ? 1 : 0);
	if (bNeedsGameplayPawn)
	{
		// Ensure slot/character are valid before restart so spawn class/start selection is identity-aware.
		if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
		{
			if (AARPlayerStateBase* JoinedPS = PlayerController->GetPlayerState<AARPlayerStateBase>())
			{
				EnsureJoinedPlayerHasUniqueSlot(GS, JoinedPS);
			}

			NormalizeConnectedPlayersIdentity(GS);
		}

		if (ExistingPawn)
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}

		RestartPlayer(PlayerController);
	}

	// Seamless travel can produce transient duplicate slot mirrors while old/new PlayerState
	// instances overlap during handoff. Re-run authoritative identity normalization immediately.
	if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
	{
		if (AARPlayerStateBase* JoinedPS = PlayerController->GetPlayerState<AARPlayerStateBase>())
		{
			EnsureJoinedPlayerHasUniqueSlot(GS, JoinedPS);
		}

		NormalizeConnectedPlayersIdentity(GS);
	}
}

AActor* AARGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	AARPlayerStateBase* PlayerState = Player->GetPlayerState<AARPlayerStateBase>();
	if (!PlayerState)
	{
		CachePendingSpawnCharacterTagForController(Player, FGameplayTag());
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const FGameplayTag PreNormalizedPlayerSlotTag = ARPlayer::NormalizePlayerSlotTag(PlayerState->GetPlayerSlotTag(), PlayerState->GetPlayerSlot());
	const FGameplayTag PreNormalizedCharacterTag = ResolveCharacterTagForSpawnIdentity(PlayerState);
	const bool bHadUnresolvedIdentityPrePass = !PreNormalizedPlayerSlotTag.IsValid() || !PreNormalizedCharacterTag.IsValid();

	// Spawn-point selection must never run against an unresolved identity; editor/raw-map flows
	// can still reach here before earlier join normalization has finalized character assignment.
	if (HasAuthority())
	{
		if (AARGameStateBase* GS = GetGameState<AARGameStateBase>())
		{
			if (!PlayerState->IsSetupComplete())
			{
				UARSaveSubsystem* SaveSubsystem = nullptr;
				if (UGameInstance* GI = GetGameInstance())
				{
					SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>();
				}

				HandleFirstSessionJoinSetup(GS, PlayerState, SaveSubsystem);
			}

			EnsureJoinedPlayerHasUniqueSlot(GS, PlayerState);
			NormalizeConnectedPlayersIdentity(GS);
		}
	}

	const FGameplayTag PlayerSlotTag = ARPlayer::NormalizePlayerSlotTag(PlayerState->GetPlayerSlotTag(), PlayerState->GetPlayerSlot());
	FGameplayTag CharacterTag = ResolveCharacterTagForSpawnIdentity(PlayerState);
	const FGameplayTag PendingCharacterTag = ARPlayer::NormalizeCharacterTag(GetPendingSpawnCharacterTagForController(Player));
	if (PendingCharacterTag.IsValid() && !CharacterTag.MatchesTagExact(PendingCharacterTag))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[GameMode] ChoosePlayerStart using pending cached character tag for '%s': %s -> %s."),
			*GetNameSafe(PlayerState),
			*CharacterTag.ToString(),
			*PendingCharacterTag.ToString());
		CharacterTag = PendingCharacterTag;
	}
	CachePendingSpawnCharacterTagForController(Player, CharacterTag);
	if (bHadUnresolvedIdentityPrePass)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart saw unresolved pre-normalization identity for '%s' (PreSlot=%s PreCharacter=%s -> PostSlot=%s PostCharacter=%s)."),
			*GetNameSafe(PlayerState),
			*PreNormalizedPlayerSlotTag.ToString(),
			*PreNormalizedCharacterTag.ToString(),
			*PlayerSlotTag.ToString(),
			*CharacterTag.ToString());
	}

	if (!CharacterTag.IsValid())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart still has invalid character identity for '%s' after normalization (Slot=%d SlotTag=%s CurrentCharacterTag=%s CharacterPicked=%d)."),
			*GetNameSafe(PlayerState),
			static_cast<int32>(PlayerState->GetPlayerSlot()),
			*PlayerSlotTag.ToString(),
			*PlayerState->GetCurrentCharacterTag().ToString(),
			static_cast<int32>(PlayerState->GetCharacterPicked()));
	}

	TArray<FGameplayTag> IdentityQueryTags;
	// Character-authored lanes should win when both character and slot starts are present.
	if (CharacterTag.IsValid() && !IdentityQueryTags.Contains(CharacterTag))
	{
		IdentityQueryTags.Add(CharacterTag);
	}
	AddSpawnIdentityMirrorTags(CharacterTag, IdentityQueryTags);
	if (PlayerSlotTag.IsValid() && !IdentityQueryTags.Contains(PlayerSlotTag))
	{
		IdentityQueryTags.Add(PlayerSlotTag);
	}
	if (IdentityQueryTags.IsEmpty())
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] ChoosePlayerStart produced no identity query tags for '%s'; falling back to default PlayerStart."),
			*GetNameSafe(PlayerState));
	}

	TArray<AARTaggedPlayerStart*> TaggedStarts;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AARTaggedPlayerStart> It(World); It; ++It)
		{
			AARTaggedPlayerStart* Start = *It;
			if (IsValid(Start))
			{
				TaggedStarts.Add(Start);
			}
		}
	}
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[GameMode] ChoosePlayerStart evaluating '%s': SlotTag=%s CharacterTag=%s QueryTags=%d TaggedStarts=%d."),
		*GetNameSafe(PlayerState),
		*PlayerSlotTag.ToString(),
		*CharacterTag.ToString(),
		IdentityQueryTags.Num(),
		TaggedStarts.Num());

	auto FindMatchingStart = [&TaggedStarts](const FGameplayTag& QueryTag, const bool bRequireExact) -> AActor*
	{
		if (!QueryTag.IsValid())
		{
			return nullptr;
		}

		for (AARTaggedPlayerStart* Start : TaggedStarts)
		{
			if (IsValid(Start) && Start->MatchesSpawnIdentityTag(QueryTag, bRequireExact))
			{
				return Start;
			}
		}

		return nullptr;
	};

	for (const FGameplayTag& QueryTag : IdentityQueryTags)
	{
		if (AActor* MatchedExact = FindMatchingStart(QueryTag, true))
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[GameMode] ChoosePlayerStart exact match for '%s': QueryTag=%s Start=%s."),
				*GetNameSafe(PlayerState),
				*QueryTag.ToString(),
				*GetNameSafe(MatchedExact));
			return MatchedExact;
		}
	}

	for (const FGameplayTag& QueryTag : IdentityQueryTags)
	{
		if (AActor* MatchedLoose = FindMatchingStart(QueryTag, false))
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[GameMode] ChoosePlayerStart loose match for '%s': QueryTag=%s Start=%s."),
				*GetNameSafe(PlayerState),
				*QueryTag.ToString(),
				*GetNameSafe(MatchedLoose));
			return MatchedLoose;
		}
	}

	UE_LOG(
		ARLog,
		Warning,
		TEXT("[GameMode] ChoosePlayerStart found no tagged match for '%s' (SlotTag=%s CharacterTag=%s). Falling back to default PlayerStart."),
		*GetNameSafe(PlayerState),
		*PlayerSlotTag.ToString(),
		*CharacterTag.ToString());
	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AARGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	if (SpawnedPawn || !HasAuthority() || !NewPlayer)
	{
		return SpawnedPawn;
	}

	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	UWorld* World = GetWorld();
	if (!World || !PawnClass)
	{
		return SpawnedPawn;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* CollisionAdjustedPawn = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (CollisionAdjustedPawn)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[GameMode] Super pawn spawn failed; forced collision-adjusted pawn spawn for controller '%s' using class '%s'."),
			*GetNameSafe(NewPlayer),
			*GetNameSafe(PawnClass));
	}

	return CollisionAdjustedPawn ? CollisionAdjustedPawn : SpawnedPawn;
}

void AARGameModeBase::Logout(AController* Exiting)
{
	CachePendingSpawnCharacterTagForController(Exiting, FGameplayTag());

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
	PendingSpawnCharacterTagsByController.Reset();

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

	const FString RequestedURLWithOptions = ARTransition::AppendTravelOptions(URL, Options);
	FString TravelURL = BuildModeTravelURL(RequestedURLWithOptions, RoutePolicy);

	UARTravelSubsystem* TravelSubsystem = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		TravelSubsystem = GI->GetSubsystem<UARTravelSubsystem>();
	}

	if (!TravelSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[GameMode] TryStartTravel failed: TravelSubsystem missing."));
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

		OpenLevelOptions = ARTransition::EnsureTravelOption(OpenLevelOptions, TEXT("listen"));

		UE_LOG(ARLog, Log, TEXT("[GameMode] TryStartTravel PIE fallback -> OpenLevel Level='%s' Options='%s'"), *LevelName, *OpenLevelOptions);
		return TravelSubsystem->RequestOpenLevel(LevelName, OpenLevelOptions, bSkipReadyChecks, bAbsolute, bSaveOnModeExit);
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
	return TravelSubsystem->RequestServerTravel(TravelURL, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bSaveOnModeExit);
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
