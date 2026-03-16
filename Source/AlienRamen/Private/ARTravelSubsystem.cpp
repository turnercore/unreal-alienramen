#include "ARTravelSubsystem.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "StructSerializable.h"
#include "Kismet/GameplayStatics.h"
#include "StructUtils/InstancedStruct.h"

namespace ARTravelInternal
{
	static FString ResolveDefaultMapPathForModeTag(const FGameplayTag& ModeTag)
	{
		if (!ModeTag.IsValid())
		{
			return FString();
		}

		const FGameplayTag LobbyModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Lobby"), false);
		if (LobbyModeTag.IsValid() && ModeTag.MatchesTagExact(LobbyModeTag))
		{
			return TEXT("/Game/Maps/Lvl_MultiplayerLobby");
		}

		const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
		if (ShopModeTag.IsValid() && ModeTag.MatchesTagExact(ShopModeTag))
		{
			return TEXT("/Game/Maps/Lvl_RamenShop");
		}

		const FGameplayTag InvaderModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
		if (InvaderModeTag.IsValid() && ModeTag.MatchesTagExact(InvaderModeTag))
		{
			return TEXT("/Game/Maps/Lvl_Invader");
		}

		const FGameplayTag ScrapyardModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
		if (ScrapyardModeTag.IsValid() && ModeTag.MatchesTagExact(ScrapyardModeTag))
		{
			return TEXT("/Game/Maps/Lvl_Scrapyard");
		}

		const FGameplayTag TransitionModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
		if (TransitionModeTag.IsValid() && ModeTag.MatchesTagExact(TransitionModeTag))
		{
			return TEXT("/Game/Maps/Lvl_Loading");
		}

		return FString();
	}
}

bool UARTravelSubsystem::TravelToLoadedSaveDestination(const bool bUseOpenLevelInPIE, const FString& TransitionMapURL)
{
	UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	if (!SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] TravelToLoadedSaveDestination failed: SaveSubsystem missing."));
		return false;
	}

	UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
	if (!SaveGame)
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] TravelToLoadedSaveDestination failed: no current save is loaded."));
		return false;
	}

	const FString DestinationURL = !SaveGame->LastSavedMapPath.IsEmpty()
		? SaveGame->LastSavedMapPath
		: SaveSubsystem->GetPendingLoadedSaveMapPath();
	const FGameplayTag SavedModeTag = SaveGame->LastSavedModeTag.IsValid()
		? SaveGame->LastSavedModeTag
		: SaveSubsystem->GetPendingLoadedSaveModeTag();

	FARTransitionContext TransitionContext;
	TransitionContext.SourceMode = EARTransitionSourceMode::SaveLoad;
	TransitionContext.Reason = EARTransitionReason::SaveLoadEntry;
	TransitionContext.bFreshLoadEntry = true;
	const FString TravelURL = BuildLoadedSaveTravelURL(DestinationURL, SavedModeTag, TransitionMapURL, TransitionContext);
	if (TravelURL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] TravelToLoadedSaveDestination failed: loaded save has no destination map path."));
		return false;
	}

#if WITH_EDITOR
	if (bUseOpenLevelInPIE && GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		FString LevelName;
		FString TravelOptions;
		if (!SplitTravelURL(TravelURL, LevelName, TravelOptions))
		{
			UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] TravelToLoadedSaveDestination failed: could not parse travel URL '%s'."), *TravelURL);
			return false;
		}

		return RequestOpenLevel(LevelName, TravelOptions, true, false, false);
	}
#endif

	return RequestServerTravel(TravelURL, true, false, false, false);
}

FString UARTravelSubsystem::BuildLoadedSaveTravelURL(
	const FString& PreferredDestinationURL,
	const FGameplayTag SavedModeTag,
	const FString& TransitionMapURL,
	FARTransitionContext& OutTransitionContext)
{
	const FString ResolvedDestinationURL = !PreferredDestinationURL.IsEmpty()
		? PreferredDestinationURL
		: ARTravelInternal::ResolveDefaultMapPathForModeTag(SavedModeTag);
	if (ResolvedDestinationURL.IsEmpty())
	{
		return FString();
	}

	OutTransitionContext.DestinationURL = ResolvedDestinationURL;
	return !TransitionMapURL.IsEmpty()
		? ARTransition::BuildTransitionTravelURL(TransitionMapURL, OutTransitionContext)
		: ARTransition::AppendTransitionContextOptions(ResolvedDestinationURL, OutTransitionContext);
}

bool UARTravelSubsystem::RequestServerTravel(
	const FString& URL,
	const bool bSkipReadyChecks,
	const bool bAbsolute,
	const bool bSkipGameNotify,
	const bool bPersistSaveBeforeTravel)
{
	FString Error;
	if (!ArePlayersReadyForTravel(bSkipReadyChecks, Error))
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] ServerTravel blocked: %s"), *Error);
		return false;
	}

	UWorld* World = GetWorld();
	UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	if (!World || !SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] ServerTravel failed: missing world or SaveSubsystem."));
		return false;
	}

	CaptureGameStateForTravel(World, SaveSubsystem);

	if (bPersistSaveBeforeTravel)
	{
		FARSaveResult SaveResult;
		if (!SaveSubsystem->SaveCurrentGame(NAME_None, true, SaveResult))
		{
			UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] ServerTravel blocked: save failed: %s"), *SaveResult.Error);
			return false;
		}

		// Persisted save supersedes travel-transient overlay.
		SaveSubsystem->ClearPendingTravelGameStateData();
	}

	const FString TravelURL = ARTransition::EnsureTravelOption(URL, TEXT("listen"));
	return World->ServerTravel(TravelURL, bAbsolute, bSkipGameNotify);
}

bool UARTravelSubsystem::RequestOpenLevel(
	const FString& LevelName,
	const FString& Options,
	const bool bSkipReadyChecks,
	const bool bAbsolute,
	const bool bPersistSaveBeforeTravel)
{
	FString Error;
	if (!ArePlayersReadyForTravel(bSkipReadyChecks, Error))
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] OpenLevel blocked: %s"), *Error);
		return false;
	}

	UWorld* World = GetWorld();
	UARSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	if (!World || !SaveSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] OpenLevel failed: missing world or SaveSubsystem."));
		return false;
	}

	CaptureGameStateForTravel(World, SaveSubsystem);

	if (bPersistSaveBeforeTravel)
	{
		FARSaveResult SaveResult;
		if (!SaveSubsystem->SaveCurrentGame(NAME_None, true, SaveResult))
		{
			UE_LOG(ARLog, Warning, TEXT("[TravelSubsystem] OpenLevel blocked: save failed: %s"), *SaveResult.Error);
			return false;
		}

		// Persisted save supersedes travel-transient overlay.
		SaveSubsystem->ClearPendingTravelGameStateData();
	}

	const FString ListenOptions = ARTransition::EnsureTravelOption(Options, TEXT("listen"));
	UGameplayStatics::OpenLevel(World, FName(*LevelName), bAbsolute, ListenOptions);
	return true;
}

UARSaveSubsystem* UARTravelSubsystem::ResolveSaveSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UARSaveSubsystem>();
	}

	return nullptr;
}

bool UARTravelSubsystem::ArePlayersReadyForTravel(const bool bSkipReadyChecks, FString& OutError) const
{
	if (bSkipReadyChecks)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("No world available.");
		return false;
	}

	if (World->GetNetMode() != NM_Standalone && World->GetAuthGameMode() == nullptr)
	{
		OutError = TEXT("Travel readiness check requires authority.");
		return false;
	}

	const AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>();
	if (!GameState)
	{
		OutError = TEXT("No GameState available.");
		return false;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		if (ARPlayerState->GetPlayerSlot() == EARPlayerSlot::Unknown)
		{
			OutError = FString::Printf(TEXT("Player '%s' missing slot."), *GetNameSafe(ARPlayerState));
			return false;
		}

		if (ARPlayerState->GetCharacterPicked() == EARCharacterChoice::None)
		{
			OutError = FString::Printf(TEXT("Player '%s' missing character choice."), *GetNameSafe(ARPlayerState));
			return false;
		}

		if (!ARPlayerState->IsReadyForRun())
		{
			OutError = FString::Printf(TEXT("Player '%s' not ready."), *GetNameSafe(ARPlayerState));
			return false;
		}
	}

	return true;
}

bool UARTravelSubsystem::CaptureGameStateForTravel(UWorld* World, UARSaveSubsystem* SaveSubsystem) const
{
	if (!World || !SaveSubsystem)
	{
		return false;
	}

	AARGameStateBase* GameState = World->GetGameState<AARGameStateBase>();
	if (!GameState || !GameState->GetClass()->ImplementsInterface(UStructSerializable::StaticClass()))
	{
		return false;
	}

	FInstancedStruct GameStatePayload;
	IStructSerializable::Execute_ExtractStateToStruct(GameState, GameStatePayload);
	if (GameStatePayload.IsValid())
	{
		SaveSubsystem->SetPendingTravelGameStateData(GameStatePayload);
		return true;
	}

	return false;
}

bool UARTravelSubsystem::SplitTravelURL(const FString& InTravelURL, FString& OutLevelName, FString& OutOptions)
{
	OutLevelName = InTravelURL;
	OutOptions.Reset();

	int32 QueryIndex = INDEX_NONE;
	if (!InTravelURL.FindChar(TEXT('?'), QueryIndex))
	{
		return !OutLevelName.IsEmpty();
	}

	OutLevelName = InTravelURL.Left(QueryIndex);
	OutOptions = InTravelURL.Mid(QueryIndex + 1);
	return !OutLevelName.IsEmpty();
}
