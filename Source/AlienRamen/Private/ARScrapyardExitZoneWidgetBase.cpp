#include "ARScrapyardExitZoneWidgetBase.h"

#include "ARScrapyardExitZoneActor.h"

void UARScrapyardExitZoneWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoObserveInitialExitZoneOnConstruct && InitialObservedExitZone)
	{
		InitializeScrapyardExitZoneWidget(InitialObservedExitZone);
	}
}

void UARScrapyardExitZoneWidgetBase::NativeDestruct()
{
	DeinitializeScrapyardExitZoneWidget();
	Super::NativeDestruct();
}

void UARScrapyardExitZoneWidgetBase::InitializeScrapyardExitZoneWidget(AARScrapyardExitZoneActor* InObservedExitZone)
{
	if (!InObservedExitZone)
	{
		DeinitializeScrapyardExitZoneWidget();
		return;
	}

	if (ObservedExitZone.Get() == InObservedExitZone)
	{
		RefreshCachedObservedExitZoneValue();
		return;
	}

	UnbindObservedExitZone();
	ObservedExitZone = InObservedExitZone;
	BindObservedExitZone();
	RefreshCachedObservedExitZoneValue();
	BP_OnScrapyardExitZoneWidgetInitialized(InObservedExitZone);
}

void UARScrapyardExitZoneWidgetBase::DeinitializeScrapyardExitZoneWidget()
{
	const bool bHadAnyBindingOrState = ObservedExitZone.IsValid() || bHasCachedObservedExitReservedScrapValue;
	UnbindObservedExitZone();
	ObservedExitZone.Reset();
	CachedObservedExitReservedScrapValue = 0;
	bHasCachedObservedExitReservedScrapValue = false;

	if (bHadAnyBindingOrState)
	{
		BP_OnScrapyardExitZoneWidgetDeinitialized();
	}
}

void UARScrapyardExitZoneWidgetBase::SetObservedExitZone(AARScrapyardExitZoneActor* InObservedExitZone)
{
	InitializeScrapyardExitZoneWidget(InObservedExitZone);
}

void UARScrapyardExitZoneWidgetBase::BindObservedExitZone()
{
	if (AARScrapyardExitZoneActor* CurrentObservedZone = ObservedExitZone.Get())
	{
		CurrentObservedZone->OnExitZoneChanged.AddUniqueDynamic(this, &UARScrapyardExitZoneWidgetBase::HandleObservedExitZoneChanged);
	}
}

void UARScrapyardExitZoneWidgetBase::UnbindObservedExitZone()
{
	if (AARScrapyardExitZoneActor* CurrentObservedZone = ObservedExitZone.Get())
	{
		CurrentObservedZone->OnExitZoneChanged.RemoveDynamic(this, &UARScrapyardExitZoneWidgetBase::HandleObservedExitZoneChanged);
	}
}

void UARScrapyardExitZoneWidgetBase::RefreshCachedObservedExitZoneValue()
{
	AARScrapyardExitZoneActor* CurrentObservedZone = ObservedExitZone.Get();
	if (!CurrentObservedZone)
	{
		return;
	}

	CachedObservedExitReservedScrapValue = CurrentObservedZone->GetDepositedReservedScrapValue();
	bHasCachedObservedExitReservedScrapValue = true;
	OnObservedExitZoneChanged.Broadcast(CurrentObservedZone, CachedObservedExitReservedScrapValue);
	BP_OnObservedExitZoneChanged(CurrentObservedZone, CachedObservedExitReservedScrapValue);
}

void UARScrapyardExitZoneWidgetBase::HandleObservedExitZoneChanged()
{
	RefreshCachedObservedExitZoneValue();
}
