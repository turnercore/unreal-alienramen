#include "ARDialogueWidgetBase.h"

#include "ARNPCCharacterBase.h"
#include "ARPlayerController.h"

void UARDialogueWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bAutoBindOwningARPlayerControllerOnConstruct && !BoundController)
	{
		InitializeDialogueWidget(Cast<AARPlayerController>(GetOwningPlayer()));
	}
}

void UARDialogueWidgetBase::NativeDestruct()
{
	DeinitializeDialogueWidget();
	Super::NativeDestruct();
}

void UARDialogueWidgetBase::InitializeDialogueWidget(AARPlayerController* InOwningController)
{
	if (BoundController == InOwningController)
	{
		PushInitialViewFromController();
		return;
	}

	UnbindControllerDelegates();
	BoundController = InOwningController;
	BindControllerDelegates();
	PushInitialViewFromController();
	BP_OnDialogueWidgetInitialized(BoundController);
}

void UARDialogueWidgetBase::DeinitializeDialogueWidget()
{
	UnbindControllerDelegates();
	BoundController = nullptr;
	CurrentDialogueView = FDialogueClientView();
	bHasActiveDialogueView = false;
	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
	BP_OnDialogueWidgetDeinitialized();
}

void UARDialogueWidgetBase::AdvanceDialogue()
{
	if (BoundController)
	{
		BoundController->RequestAdvanceDialogue();
	}
}

void UARDialogueWidgetBase::SubmitChoice(FGuid ChoiceBranchId)
{
	if (BoundController && ChoiceBranchId.IsValid())
	{
		BoundController->RequestSubmitDialogueChoice(ChoiceBranchId);
	}
}

void UARDialogueWidgetBase::SetEavesdrop(bool bEnable, EARPlayerSlot TargetSlot)
{
	if (BoundController)
	{
		BoundController->RequestSetDialogueEavesdrop(bEnable, TargetSlot);
	}
}

void UARDialogueWidgetBase::SetEavesdropOtherPlayer(bool bEnable)
{
	if (BoundController)
	{
		BoundController->RequestSetDialogueEavesdropOtherPlayer(bEnable);
	}
}

void UARDialogueWidgetBase::StartDialogueWithNpcTag(FGameplayTag NpcTag)
{
	if (BoundController && NpcTag.IsValid())
	{
		BoundController->RequestStartDialogue(NpcTag);
	}
}

void UARDialogueWidgetBase::InteractWithNpc(AARNPCCharacterBase* NpcActor)
{
	if (BoundController && NpcActor)
	{
		BoundController->RequestInteractWithNpc(NpcActor);
	}
}

void UARDialogueWidgetBase::ToggleAutoAdvance()
{
	if (BoundController)
	{
		BoundController->RequestToggleDialogueAutoAdvance();
	}
}

void UARDialogueWidgetBase::AdvanceOrSubmitDialogue()
{
	if (BoundController)
	{
		BoundController->RequestAdvanceOrSubmitDialogue();
	}
}

void UARDialogueWidgetBase::ChoiceDelta(const int32 Delta)
{
	if (BoundController)
	{
		BoundController->RequestDialogueChoiceDelta(Delta);
	}
}

bool UARDialogueWidgetBase::GetCurrentDialogueView(FDialogueClientView& OutView) const
{
	OutView = bHasActiveDialogueView ? CurrentDialogueView : FDialogueClientView();
	return bHasActiveDialogueView;
}

void UARDialogueWidgetBase::HandleControllerDialogueViewUpdated(const FDialogueClientView& View)
{
	CurrentDialogueView = View;
	bHasActiveDialogueView = true;
	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
	BP_OnDialogueViewUpdated(View);
}

void UARDialogueWidgetBase::HandleControllerDialogueSessionEnded(const FString& SessionId)
{
	if (bHasActiveDialogueView && CurrentDialogueView.SessionId == SessionId)
	{
		CurrentDialogueView = FDialogueClientView();
		bHasActiveDialogueView = false;
		if (bAutoToggleVisibilityFromSessionState)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	BP_OnDialogueSessionEnded(SessionId);
}

void UARDialogueWidgetBase::BindControllerDelegates()
{
	if (!BoundController)
	{
		return;
	}

	BoundController->OnDialogueViewUpdated.AddUniqueDynamic(this, &UARDialogueWidgetBase::HandleControllerDialogueViewUpdated);
	BoundController->OnDialogueSessionEndedSignal.AddUniqueDynamic(this, &UARDialogueWidgetBase::HandleControllerDialogueSessionEnded);
}

void UARDialogueWidgetBase::UnbindControllerDelegates()
{
	if (!BoundController)
	{
		return;
	}

	BoundController->OnDialogueViewUpdated.RemoveDynamic(this, &UARDialogueWidgetBase::HandleControllerDialogueViewUpdated);
	BoundController->OnDialogueSessionEndedSignal.RemoveDynamic(this, &UARDialogueWidgetBase::HandleControllerDialogueSessionEnded);
}

void UARDialogueWidgetBase::PushInitialViewFromController()
{
	if (!BoundController)
	{
		return;
	}

	FDialogueClientView CurrentView;
	if (BoundController->QueryLocalDialogueView(CurrentView))
	{
		HandleControllerDialogueViewUpdated(CurrentView);
		return;
	}

	if (BoundController->GetCachedDialogueView(CurrentView))
	{
		HandleControllerDialogueViewUpdated(CurrentView);
	}
}
