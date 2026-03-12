#include "ARScrapyardHUDWidgetBase.h"

#include "ARPlayerController.h"
#include "ARScrapyardGameState.h"
#include "ARScrapyardHUD.h"

void UARScrapyardHUDWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoBindOwningScrapyardHUDOnConstruct && !BoundScrapyardHUD.IsValid())
	{
		TryBindOwningScrapyardHUD();
	}
}

void UARScrapyardHUDWidgetBase::NativeDestruct()
{
	DeinitializeScrapyardHUDWidget();
	Super::NativeDestruct();
}

void UARScrapyardHUDWidgetBase::InitializeScrapyardHUDWidget(AARScrapyardHUD* InScrapyardHUD)
{
	if (!InScrapyardHUD)
	{
		DeinitializeScrapyardHUDWidget();
		return;
	}

	if (BoundScrapyardHUD.Get() == InScrapyardHUD)
	{
		RefreshCachedStateFromHUD();
		return;
	}

	UnbindScrapyardHUDDelegates();
	BoundScrapyardHUD = InScrapyardHUD;
	BindScrapyardHUDDelegates();
	RefreshCachedStateFromHUD();
	BP_OnScrapyardHUDWidgetInitialized(
		InScrapyardHUD,
		InScrapyardHUD->GetBoundPlayerController(),
		InScrapyardHUD->GetBoundScrapyardGameState());
}

void UARScrapyardHUDWidgetBase::DeinitializeScrapyardHUDWidget()
{
	const bool bHadAnyBindingOrState =
		BoundScrapyardHUD.IsValid()
		|| bHasCurrentExtractionSummary
		|| bHasCurrentRunTimer
		|| bHasCurrentRunActive
		|| bHasCurrentRunBuffStateSnapshot;

	UnbindScrapyardHUDDelegates();
	BoundScrapyardHUD.Reset();
	CurrentExtractionSummary = FARScrapyardExtractionSummary();
	CurrentRunRemainingSeconds = 0.0f;
	bCurrentRunActive = false;
	CurrentRunBuffStateSnapshot = FARRunBuffStateSnapshot();
	bHasCurrentExtractionSummary = false;
	bHasCurrentRunTimer = false;
	bHasCurrentRunActive = false;
	bHasCurrentRunBuffStateSnapshot = false;

	if (bHadAnyBindingOrState)
	{
		BP_OnScrapyardHUDWidgetDeinitialized();
	}
}

bool UARScrapyardHUDWidgetBase::TryBindOwningScrapyardHUD()
{
	APlayerController* OwningController = GetOwningPlayer();
	AARScrapyardHUD* OwningScrapyardHUD = OwningController ? Cast<AARScrapyardHUD>(OwningController->GetHUD()) : nullptr;
	if (!OwningScrapyardHUD)
	{
		return false;
	}

	InitializeScrapyardHUDWidget(OwningScrapyardHUD);
	return BoundScrapyardHUD.Get() == OwningScrapyardHUD;
}

bool UARScrapyardHUDWidgetBase::GetCurrentExtractionSummary(FARScrapyardExtractionSummary& OutSummary) const
{
	OutSummary = bHasCurrentExtractionSummary ? CurrentExtractionSummary : FARScrapyardExtractionSummary();
	return bHasCurrentExtractionSummary;
}

bool UARScrapyardHUDWidgetBase::GetCurrentRunRemainingSeconds(float& OutRemainingSeconds) const
{
	OutRemainingSeconds = bHasCurrentRunTimer ? CurrentRunRemainingSeconds : 0.0f;
	return bHasCurrentRunTimer;
}

bool UARScrapyardHUDWidgetBase::GetCurrentRunActive(bool& OutRunActive) const
{
	OutRunActive = bHasCurrentRunActive ? bCurrentRunActive : false;
	return bHasCurrentRunActive;
}

bool UARScrapyardHUDWidgetBase::GetCurrentRunBuffStateSnapshot(FARRunBuffStateSnapshot& OutSnapshot) const
{
	OutSnapshot = bHasCurrentRunBuffStateSnapshot ? CurrentRunBuffStateSnapshot : FARRunBuffStateSnapshot();
	return bHasCurrentRunBuffStateSnapshot;
}

void UARScrapyardHUDWidgetBase::BindScrapyardHUDDelegates()
{
	AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		return;
	}

	ScrapyardHUD->OnScrapyardHUDExtractionSummaryChanged.AddUniqueDynamic(this, &UARScrapyardHUDWidgetBase::HandleScrapyardExtractionSummaryChanged);
	ScrapyardHUD->OnScrapyardHUDRunTimerChanged.AddUniqueDynamic(this, &UARScrapyardHUDWidgetBase::HandleScrapyardRunTimerChanged);
	ScrapyardHUD->OnScrapyardHUDRunActiveChanged.AddUniqueDynamic(this, &UARScrapyardHUDWidgetBase::HandleScrapyardRunActiveChanged);
	ScrapyardHUD->OnScrapyardHUDRunBuffStateChanged.AddUniqueDynamic(this, &UARScrapyardHUDWidgetBase::HandleRunBuffStateChanged);
}

void UARScrapyardHUDWidgetBase::UnbindScrapyardHUDDelegates()
{
	AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		return;
	}

	ScrapyardHUD->OnScrapyardHUDExtractionSummaryChanged.RemoveDynamic(this, &UARScrapyardHUDWidgetBase::HandleScrapyardExtractionSummaryChanged);
	ScrapyardHUD->OnScrapyardHUDRunTimerChanged.RemoveDynamic(this, &UARScrapyardHUDWidgetBase::HandleScrapyardRunTimerChanged);
	ScrapyardHUD->OnScrapyardHUDRunActiveChanged.RemoveDynamic(this, &UARScrapyardHUDWidgetBase::HandleScrapyardRunActiveChanged);
	ScrapyardHUD->OnScrapyardHUDRunBuffStateChanged.RemoveDynamic(this, &UARScrapyardHUDWidgetBase::HandleRunBuffStateChanged);
}

void UARScrapyardHUDWidgetBase::RefreshCachedStateFromHUD()
{
	AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		return;
	}

	FARScrapyardExtractionSummary Summary;
	if (ScrapyardHUD->GetCachedExtractionSummary(Summary))
	{
		HandleScrapyardExtractionSummaryChanged(Summary);
	}

	float RemainingSeconds = 0.0f;
	if (ScrapyardHUD->GetCachedRunRemainingSeconds(RemainingSeconds))
	{
		HandleScrapyardRunTimerChanged(RemainingSeconds);
	}

	bool bIsRunActive = false;
	if (ScrapyardHUD->GetCachedRunActive(bIsRunActive))
	{
		HandleScrapyardRunActiveChanged(bIsRunActive);
	}

	FARRunBuffStateSnapshot RunBuffSnapshot;
	if (ScrapyardHUD->GetCachedRunBuffStateSnapshot(RunBuffSnapshot))
	{
		HandleRunBuffStateChanged(RunBuffSnapshot);
	}
}

void UARScrapyardHUDWidgetBase::HandleScrapyardExtractionSummaryChanged(const FARScrapyardExtractionSummary& Summary)
{
	CurrentExtractionSummary = Summary;
	bHasCurrentExtractionSummary = true;
	OnScrapyardWidgetExtractionSummaryChanged.Broadcast(Summary);
	BP_OnScrapyardWidgetExtractionSummaryChanged(Summary);
}

void UARScrapyardHUDWidgetBase::HandleScrapyardRunTimerChanged(float RemainingSeconds)
{
	CurrentRunRemainingSeconds = RemainingSeconds;
	bHasCurrentRunTimer = true;
	OnScrapyardWidgetRunTimerChanged.Broadcast(RemainingSeconds);
	BP_OnScrapyardWidgetRunTimerChanged(RemainingSeconds);
}

void UARScrapyardHUDWidgetBase::HandleScrapyardRunActiveChanged(bool bIsRunActive)
{
	bCurrentRunActive = bIsRunActive;
	bHasCurrentRunActive = true;
	OnScrapyardWidgetRunActiveChanged.Broadcast(bIsRunActive);
	BP_OnScrapyardWidgetRunActiveChanged(bIsRunActive);
}

void UARScrapyardHUDWidgetBase::HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot)
{
	CurrentRunBuffStateSnapshot = Snapshot;
	bHasCurrentRunBuffStateSnapshot = true;
	OnScrapyardWidgetRunBuffStateChanged.Broadcast(Snapshot);
	BP_OnScrapyardWidgetRunBuffStateChanged(Snapshot);
}
