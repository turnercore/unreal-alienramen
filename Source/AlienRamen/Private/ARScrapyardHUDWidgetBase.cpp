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
	const bool bHadAnyBinding = BoundScrapyardHUD.IsValid();

	UnbindScrapyardHUDDelegates();
	BoundScrapyardHUD.Reset();

	if (bHadAnyBinding)
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
	const AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		OutSummary = FARScrapyardExtractionSummary();
		return false;
	}

	return ScrapyardHUD->GetCachedExtractionSummary(OutSummary);
}

bool UARScrapyardHUDWidgetBase::GetCurrentRunRemainingSeconds(float& OutRemainingSeconds) const
{
	const AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		OutRemainingSeconds = 0.0f;
		return false;
	}

	return ScrapyardHUD->GetCachedRunRemainingSeconds(OutRemainingSeconds);
}

bool UARScrapyardHUDWidgetBase::GetCurrentRunActive(bool& OutRunActive) const
{
	const AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		OutRunActive = false;
		return false;
	}

	return ScrapyardHUD->GetCachedRunActive(OutRunActive);
}

bool UARScrapyardHUDWidgetBase::GetCurrentRunBuffStateSnapshot(FARRunBuffStateSnapshot& OutSnapshot) const
{
	const AARScrapyardHUD* ScrapyardHUD = BoundScrapyardHUD.Get();
	if (!ScrapyardHUD)
	{
		OutSnapshot = FARRunBuffStateSnapshot();
		return false;
	}

	return ScrapyardHUD->GetCachedRunBuffStateSnapshot(OutSnapshot);
}

bool UARScrapyardHUDWidgetBase::HasCurrentExtractionSummary() const
{
	FARScrapyardExtractionSummary Ignored;
	return GetCurrentExtractionSummary(Ignored);
}

bool UARScrapyardHUDWidgetBase::HasCurrentRunTimer() const
{
	float Ignored = 0.0f;
	return GetCurrentRunRemainingSeconds(Ignored);
}

bool UARScrapyardHUDWidgetBase::HasCurrentRunActive() const
{
	bool bIgnored = false;
	return GetCurrentRunActive(bIgnored);
}

bool UARScrapyardHUDWidgetBase::HasCurrentRunBuffStateSnapshot() const
{
	FARRunBuffStateSnapshot Ignored;
	return GetCurrentRunBuffStateSnapshot(Ignored);
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
	OnScrapyardWidgetExtractionSummaryChanged.Broadcast(Summary);
	BP_OnScrapyardWidgetExtractionSummaryChanged(Summary);
}

void UARScrapyardHUDWidgetBase::HandleScrapyardRunTimerChanged(float RemainingSeconds)
{
	OnScrapyardWidgetRunTimerChanged.Broadcast(RemainingSeconds);
	BP_OnScrapyardWidgetRunTimerChanged(RemainingSeconds);
}

void UARScrapyardHUDWidgetBase::HandleScrapyardRunActiveChanged(bool bIsRunActive)
{
	OnScrapyardWidgetRunActiveChanged.Broadcast(bIsRunActive);
	BP_OnScrapyardWidgetRunActiveChanged(bIsRunActive);
}

void UARScrapyardHUDWidgetBase::HandleRunBuffStateChanged(const FARRunBuffStateSnapshot& Snapshot)
{
	OnScrapyardWidgetRunBuffStateChanged.Broadcast(Snapshot);
	BP_OnScrapyardWidgetRunBuffStateChanged(Snapshot);
}
