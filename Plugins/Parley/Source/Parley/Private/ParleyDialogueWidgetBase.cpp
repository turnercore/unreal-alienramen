#include "ParleyDialogueWidgetBase.h"

#include "ParleyDialogueSubsystem.h"
#include "ParleyPlayerControllerInterface.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

namespace
{
	static IParleyPlayerControllerInterface* ResolveParleyControllerInterface(APlayerController* Controller)
	{
		if (!Controller || !Controller->GetClass()->ImplementsInterface(UParleyPlayerControllerInterface::StaticClass()))
		{
			return nullptr;
		}

		return Cast<IParleyPlayerControllerInterface>(Controller);
	}

	static const IParleyPlayerControllerInterface* ResolveParleyControllerInterface(const APlayerController* Controller)
	{
		if (!Controller || !Controller->GetClass()->ImplementsInterface(UParleyPlayerControllerInterface::StaticClass()))
		{
			return nullptr;
		}

		return Cast<IParleyPlayerControllerInterface>(Controller);
	}
}

void UParleyDialogueWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bAutoBindOwningPlayerControllerOnConstruct && !BoundController)
	{
		InitializeDialogueWidget(GetOwningPlayer());
	}
}

void UParleyDialogueWidgetBase::NativeDestruct()
{
	DeinitializeDialogueWidget();
	Super::NativeDestruct();
}

void UParleyDialogueWidgetBase::InitializeDialogueWidget(APlayerController* InOwningController)
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

void UParleyDialogueWidgetBase::DeinitializeDialogueWidget()
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

void UParleyDialogueWidgetBase::AdvanceDialogue()
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestAdvanceDialogueInput();
	}
}

void UParleyDialogueWidgetBase::SubmitChoice(FGuid ChoiceBranchId)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		if (ChoiceBranchId.IsValid())
		{
			ControllerInterface->RequestSubmitDialogueChoiceInput(ChoiceBranchId);
		}
	}
}

void UParleyDialogueWidgetBase::SetEavesdrop(bool bEnable, FGameplayTag TargetSlotTag)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestSetDialogueEavesdropInput(bEnable, TargetSlotTag);
	}
}

void UParleyDialogueWidgetBase::SetEavesdropOtherPlayer(bool bEnable)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestSetDialogueEavesdropOtherPlayerInput(bEnable);
	}
}

void UParleyDialogueWidgetBase::StartDialogueWithSpeakerTag(FGameplayTag SpeakerTag)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		if (SpeakerTag.IsValid())
		{
			ControllerInterface->RequestStartDialogueBySpeakerTag(SpeakerTag);
		}
	}
}

void UParleyDialogueWidgetBase::InteractWithCharacter(AActor* CharacterActor)
{
	if (!CharacterActor)
	{
		return;
	}

	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestInteractWithActor(CharacterActor);
	}
}

void UParleyDialogueWidgetBase::ToggleAutoAdvance()
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestToggleDialogueAutoAdvanceInput();
	}
}

void UParleyDialogueWidgetBase::AdvanceOrSubmitDialogue()
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestAdvanceOrSubmitDialogueInput();
	}
}

void UParleyDialogueWidgetBase::ChoiceDelta(const int32 Delta)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestDialogueChoiceDeltaInput(Delta);
	}
}

bool UParleyDialogueWidgetBase::GetCurrentDialogueView(FDialogueClientView& OutView) const
{
	OutView = bHasActiveDialogueView ? CurrentDialogueView : FDialogueClientView();
	return bHasActiveDialogueView;
}

void UParleyDialogueWidgetBase::HandleControllerDialogueViewUpdated(const FDialogueClientView& View)
{
	(void)View;
	if (!IsValid(BoundController))
	{
		DeinitializeDialogueWidget();
		return;
	}

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController);
	if (!ControllerInterface)
	{
		DeinitializeDialogueWidget();
		return;
	}

	FDialogueClientView LocalView;
	if (!ControllerInterface->QueryLocalDialogueView(LocalView) && !ControllerInterface->GetCachedDialogueView(LocalView))
	{
		CurrentDialogueView = FDialogueClientView();
		bHasActiveDialogueView = false;
		if (bAutoToggleVisibilityFromSessionState)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	CurrentDialogueView = LocalView;
	bHasActiveDialogueView = true;
	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
	BP_OnDialogueViewUpdated(LocalView);
}

void UParleyDialogueWidgetBase::HandleControllerDialogueSessionEnded(const FString& SessionId)
{
	if (!IsValid(BoundController))
	{
		DeinitializeDialogueWidget();
		return;
	}

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

void UParleyDialogueWidgetBase::BindControllerDelegates()
{
	if (!IsValid(BoundController))
	{
		return;
	}

	if (UGameInstance* GameInstance = BoundController->GetGameInstance())
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			DialogueSubsystem->OnDialogueSessionUpdated.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueViewUpdated);
			DialogueSubsystem->OnDialogueSessionEnded.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueSessionEnded);
		}
	}
}

void UParleyDialogueWidgetBase::UnbindControllerDelegates()
{
	if (!BoundController)
	{
		return;
	}

	if (!IsValid(BoundController))
	{
		BoundController = nullptr;
		return;
	}

	if (UGameInstance* GameInstance = BoundController->GetGameInstance())
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			DialogueSubsystem->OnDialogueSessionUpdated.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueViewUpdated);
			DialogueSubsystem->OnDialogueSessionEnded.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueSessionEnded);
		}
	}
}

void UParleyDialogueWidgetBase::PushInitialViewFromController()
{
	if (!IsValid(BoundController))
	{
		return;
	}

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController);
	if (!ControllerInterface)
	{
		return;
	}

	FDialogueClientView CurrentView;
	if (ControllerInterface->QueryLocalDialogueView(CurrentView) || ControllerInterface->GetCachedDialogueView(CurrentView))
	{
		HandleControllerDialogueViewUpdated(CurrentView);
	}
}
