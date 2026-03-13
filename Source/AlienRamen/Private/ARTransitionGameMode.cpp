#include "ARTransitionGameMode.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARTransitionGameState.h"
#include "ARTransitionPlayerController.h"
#include "ARTransitionTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	static void EnterTransitionSpectatorState(APlayerController* PlayerController)
	{
		if (!PlayerController)
		{
			return;
		}

		if (APawn* ExistingPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}

		PlayerController->StartSpectatingOnly();
	}
}

AARTransitionGameMode::AARTransitionGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Transition"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[TransitionGameMode] Required gameplay tag 'Mode.Transition' is missing."));

	bSaveOnModeExit = false;
	bAutosaveOnQuit = false;
	bAllowManualSaveInMode = false;
	bShareLocalPauseAcrossControllersInMode = true;

	bStartPlayersAsSpectators = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AARTransitionPlayerController::StaticClass();
	GameStateClass = AARTransitionGameState::StaticClass();
}

void AARTransitionGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			EnterTransitionSpectatorState(It->Get());
		}
	}

	InitializeTransitionContext();
	if (bResetPlayerReadyOnBeginPlay)
	{
		ResetPlayersReadyState();
	}

	if (AARTransitionGameState* TransitionGameState = GetGameState<AARTransitionGameState>())
	{
		TransitionGameState->OnAllPlayersTravelReadyChanged.AddUniqueDynamic(this, &AARTransitionGameMode::HandleAllPlayersTravelReadyChanged);
	}

	if (bAutoAdvanceWhenAllPlayersReady)
	{
		TryAdvanceToDestination();
	}
}

void AARTransitionGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AARTransitionGameState* TransitionGameState = GetGameState<AARTransitionGameState>())
	{
		TransitionGameState->OnAllPlayersTravelReadyChanged.RemoveDynamic(this, &AARTransitionGameMode::HandleAllPlayersTravelReadyChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AARTransitionGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	EnterTransitionSpectatorState(NewPlayer);

	if (!HasAuthority() || !bResetPlayerReadyOnBeginPlay || !NewPlayer)
	{
		return;
	}

	if (AARPlayerStateBase* PlayerState = NewPlayer->GetPlayerState<AARPlayerStateBase>())
	{
		PlayerState->SetReadyForRun(false);
	}
}

void AARTransitionGameMode::RestartPlayer(AController* NewPlayer)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(NewPlayer))
	{
		EnterTransitionSpectatorState(PlayerController);
	}
}

APawn* AARTransitionGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	(void)NewPlayer;
	(void)StartSpot;
	return nullptr;
}

void AARTransitionGameMode::HandleAllPlayersTravelReadyChanged(const bool bNewAllPlayersTravelReady, const bool bOldAllPlayersTravelReady)
{
	(void)bOldAllPlayersTravelReady;
	if (!HasAuthority() || !bAutoAdvanceWhenAllPlayersReady || !bNewAllPlayersTravelReady)
	{
		return;
	}

	TryAdvanceToDestination();
}

void AARTransitionGameMode::InitializeTransitionContext()
{
	AARTransitionGameState* TransitionGameState = GetGameState<AARTransitionGameState>();
	if (!TransitionGameState)
	{
		return;
	}

	FARTransitionContext TransitionContext = TransitionGameState->GetTransitionContext();
	ARTransition::ApplyTransitionContextFromTravelOptions(OptionsString, TransitionContext);
	if (TransitionContext.DestinationURL.IsEmpty())
	{
		TransitionContext.DestinationURL = FallbackDestinationURL;
	}

	TransitionGameState->SetTransitionContext(TransitionContext);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[Transition] Context initialized Source=%s Reason=%s Destination='%s' FreshLoad=%d"),
		*ARTransition::LexToString(TransitionContext.SourceMode),
		*ARTransition::LexToString(TransitionContext.Reason),
		*TransitionContext.DestinationURL,
		TransitionContext.bFreshLoadEntry ? 1 : 0);
}

void AARTransitionGameMode::ResetPlayersReadyState() const
{
	if (!HasAuthority() || !GameState)
	{
		return;
	}

	for (APlayerState* PlayerStateBase : GameState->PlayerArray)
	{
		if (AARPlayerStateBase* PlayerState = Cast<AARPlayerStateBase>(PlayerStateBase))
		{
			PlayerState->SetReadyForRun(false);
		}
	}
}

bool AARTransitionGameMode::TryAdvanceToDestination()
{
	if (!HasAuthority() || !bAutoAdvanceWhenAllPlayersReady || bTransitionTravelStarted)
	{
		return false;
	}

	AARTransitionGameState* TransitionGameState = GetGameState<AARTransitionGameState>();
	if (!TransitionGameState)
	{
		return false;
	}

	if (!TransitionGameState->AreAllPlayersTravelReady())
	{
		return false;
	}

	const FARTransitionContext& TransitionContext = TransitionGameState->GetTransitionContext();
	if (TransitionContext.DestinationURL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[Transition] Cannot advance: destination URL is empty."));
		return false;
	}

	bTransitionTravelStarted = true;
	const FString DestinationTravelURL = ARTransition::AppendTransitionContextOptions(TransitionContext.DestinationURL, TransitionContext);
	if (!TryStartTravel(DestinationTravelURL, TEXT(""), false, false, false, false, EARTravelRoutePolicy::ForceDirect))
	{
		bTransitionTravelStarted = false;
		UE_LOG(ARLog, Warning, TEXT("[Transition] Advance travel failed to '%s'."), *DestinationTravelURL);
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[Transition] Advancing to destination '%s'."), *DestinationTravelURL);
	return true;
}
