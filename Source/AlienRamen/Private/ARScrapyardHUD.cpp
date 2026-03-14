#include "ARScrapyardHUD.h"

#include "ARPlayerController.h"
#include "ARScrapyardGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

void AARScrapyardHUD::RequestHUDInitialization(
	APlayerController* SourceController,
	APlayerState* CurrentPlayerState,
	AGameStateBase* CurrentGameState)
{
	Super::RequestHUDInitialization(SourceController, CurrentPlayerState, CurrentGameState);

	AARPlayerController* ARController = Cast<AARPlayerController>(SourceController);
	if (!ARController || !ARController->IsLocalController())
	{
		return;
	}

	InitializeScrapyardHUD(ARController, Cast<AARScrapyardGameState>(CurrentGameState));
}

void AARScrapyardHUD::InitializeScrapyardHUD(AARPlayerController* SourceController, AARScrapyardGameState* CurrentScrapyardGameState)
{
	if (!SourceController || !SourceController->IsLocalController() || !CurrentScrapyardGameState)
	{
		DeinitializeScrapyardHUD();
		return;
	}

	BoundPlayerController = SourceController;
	BindScrapyardGameState(CurrentScrapyardGameState);
	RefreshCachedState();
	BP_OnScrapyardHUDInitialized(SourceController, CurrentScrapyardGameState);
}

void AARScrapyardHUD::DeinitializeScrapyardHUD()
{
	const bool bHadAnyBindingOrState =
		BoundPlayerController.IsValid()
		|| BoundScrapyardGameState.IsValid()
		|| bHasCachedExtractionSummary
		|| bHasCachedRunTimer
		|| bHasCachedRunActive
		|| bHasCachedRunBuffStateSnapshot;

	UnbindScrapyardGameState();

	BoundPlayerController.Reset();
	CachedExtractionSummary = FARScrapyardExtractionSummary();
	CachedRunRemainingSeconds = 0.0f;
	bCachedRunActive = false;
	CachedRunBuffStateSnapshot = FARRunBuffStateSnapshot();
	bHasCachedExtractionSummary = false;
	bHasCachedRunTimer = false;
	bHasCachedRunActive = false;
	bHasCachedRunBuffStateSnapshot = false;

	if (bHadAnyBindingOrState)
	{
		BP_OnScrapyardHUDDeinitialized();
	}
}

bool AARScrapyardHUD::GetCachedExtractionSummary(FARScrapyardExtractionSummary& OutSummary) const
{
	OutSummary = bHasCachedExtractionSummary ? CachedExtractionSummary : FARScrapyardExtractionSummary();
	return bHasCachedExtractionSummary;
}

bool AARScrapyardHUD::GetCachedRunRemainingSeconds(float& OutRemainingSeconds) const
{
	OutRemainingSeconds = bHasCachedRunTimer ? CachedRunRemainingSeconds : 0.0f;
	return bHasCachedRunTimer;
}

bool AARScrapyardHUD::GetCachedRunActive(bool& OutRunActive) const
{
	OutRunActive = bHasCachedRunActive ? bCachedRunActive : false;
	return bHasCachedRunActive;
}

bool AARScrapyardHUD::GetCachedRunBuffStateSnapshot(FARRunBuffStateSnapshot& OutSnapshot) const
{
	OutSnapshot = bHasCachedRunBuffStateSnapshot ? CachedRunBuffStateSnapshot : FARRunBuffStateSnapshot();
	return bHasCachedRunBuffStateSnapshot;
}

void AARScrapyardHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeinitializeScrapyardHUD();
	Super::EndPlay(EndPlayReason);
}

void AARScrapyardHUD::BindScrapyardGameState(AARScrapyardGameState* InGameState)
{
	if (BoundScrapyardGameState.Get() == InGameState)
	{
		return;
	}

	UnbindScrapyardGameState();
	BoundScrapyardGameState = InGameState;
	if (!InGameState)
	{
		return;
	}

	InGameState->OnScrapyardExtractionSummaryChanged.AddUniqueDynamic(this, &AARScrapyardHUD::HandleScrapyardExtractionSummaryChanged);
	InGameState->OnScrapyardRunTimerChanged.AddUniqueDynamic(this, &AARScrapyardHUD::HandleScrapyardRunTimerChanged);
	InGameState->OnScrapyardRunActiveChanged.AddUniqueDynamic(this, &AARScrapyardHUD::HandleScrapyardRunActiveChanged);
	InGameState->OnScrapyardRunBuffSnapshotChanged.AddUniqueDynamic(this, &AARScrapyardHUD::HandleRunBuffStateChanged);
}

void AARScrapyardHUD::UnbindScrapyardGameState()
{
	AARScrapyardGameState* CurrentGameState = BoundScrapyardGameState.Get();
	if (!CurrentGameState)
	{
		BoundScrapyardGameState.Reset();
		return;
	}

	CurrentGameState->OnScrapyardExtractionSummaryChanged.RemoveDynamic(this, &AARScrapyardHUD::HandleScrapyardExtractionSummaryChanged);
	CurrentGameState->OnScrapyardRunTimerChanged.RemoveDynamic(this, &AARScrapyardHUD::HandleScrapyardRunTimerChanged);
	CurrentGameState->OnScrapyardRunActiveChanged.RemoveDynamic(this, &AARScrapyardHUD::HandleScrapyardRunActiveChanged);
	CurrentGameState->OnScrapyardRunBuffSnapshotChanged.RemoveDynamic(this, &AARScrapyardHUD::HandleRunBuffStateChanged);
	BoundScrapyardGameState.Reset();
}

void AARScrapyardHUD::RefreshCachedState()
{
	if (AARScrapyardGameState* CurrentGameState = BoundScrapyardGameState.Get())
	{
		HandleScrapyardExtractionSummaryChanged(CurrentGameState->GetExtractionSummary());
		HandleScrapyardRunTimerChanged(CurrentGameState->GetScrapyardRunRemainingSeconds());
		HandleScrapyardRunActiveChanged(CurrentGameState->IsScrapyardRunActive());
		HandleRunBuffStateChanged(CurrentGameState->GetRunBuffStateSnapshot());
	}
}

void AARScrapyardHUD::HandleScrapyardExtractionSummaryChanged(const FARScrapyardExtractionSummary& Summary)
{
	CachedExtractionSummary = Summary;
	bHasCachedExtractionSummary = true;
	OnScrapyardHUDExtractionSummaryChanged.Broadcast(Summary);
	BP_OnScrapyardExtractionSummaryChanged(Summary);
}

void AARScrapyardHUD::HandleScrapyardRunTimerChanged(float RemainingSeconds)
{
	CachedRunRemainingSeconds = RemainingSeconds;
	bHasCachedRunTimer = true;
	OnScrapyardHUDRunTimerChanged.Broadcast(RemainingSeconds);
	BP_OnScrapyardRunTimerChanged(RemainingSeconds);
}

void AARScrapyardHUD::HandleScrapyardRunActiveChanged(bool bIsRunActive)
{
	bCachedRunActive = bIsRunActive;
	bHasCachedRunActive = true;
	OnScrapyardHUDRunActiveChanged.Broadcast(bIsRunActive);
	BP_OnScrapyardRunActiveChanged(bIsRunActive);
}

void AARScrapyardHUD::HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot)
{
	CachedRunBuffStateSnapshot = Snapshot;
	bHasCachedRunBuffStateSnapshot = true;
	OnScrapyardHUDRunBuffStateChanged.Broadcast(Snapshot);
	BP_OnScrapyardRunBuffStateChanged(Snapshot);
}
