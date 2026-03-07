#include "ARInvaderFullBlastMenuWidget.h"

#include "ARInvaderPlayerController.h"

void UARInvaderFullBlastMenuWidget::InitializeFullBlastMenu(
	AARInvaderPlayerController* InOwningController,
	const FARInvaderFullBlastSessionState& InSessionState,
	const TArray<FARInvaderUpgradeDefRow>& InOfferDefs,
	const bool bInIsChooser)
{
	OwningInvaderController = InOwningController;
	SessionState = InSessionState;
	OfferDefinitions = InOfferDefs;
	bIsChooser = bInIsChooser;
	BP_OnFullBlastMenuUpdated(SessionState, OfferDefinitions, bIsChooser);
}

void UARInvaderFullBlastMenuWidget::SubmitSelection(const FGameplayTag SelectedUpgradeTag, const int32 DesiredDestinationSlot)
{
	if (AARInvaderPlayerController* Controller = OwningInvaderController.Get())
	{
		Controller->RequestResolveFullBlastSelection(SelectedUpgradeTag, DesiredDestinationSlot);
	}
}

void UARInvaderFullBlastMenuWidget::SubmitSkip()
{
	if (AARInvaderPlayerController* Controller = OwningInvaderController.Get())
	{
		Controller->RequestResolveFullBlastSkip();
	}
}

void UARInvaderFullBlastMenuWidget::PublishOfferPresence(
	const FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	const FVector2D CursorNormalized,
	const bool bHasCursor)
{
	if (AARInvaderPlayerController* Controller = OwningInvaderController.Get())
	{
		Controller->RequestSetOfferPresence(HoveredUpgradeTag, HoveredDestinationSlot, CursorNormalized, bHasCursor);
	}
}

void UARInvaderFullBlastMenuWidget::ClearOfferPresence()
{
	if (AARInvaderPlayerController* Controller = OwningInvaderController.Get())
	{
		Controller->RequestClearOfferPresence();
	}
}

void UARInvaderFullBlastMenuWidget::NotifyMenuClosed()
{
	BP_OnFullBlastMenuClosed();
}

