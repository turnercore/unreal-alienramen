#include "ARTransitionPlayerController.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARTransitionGameState.h"
#include "Blueprint/UserWidget.h"

AARTransitionPlayerController::AARTransitionPlayerController()
{
}

void AARTransitionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UUserWidget* CreatedWidget = nullptr;
	if (bAutoCreateTransitionWidget)
	{
		CreatedWidget = ShowTransitionWidgetFromContext();
	}

	if (bAutoApplyTransitionInputMode)
	{
		ApplyTransitionInputMode(true, CreatedWidget);
	}
}

void AARTransitionPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAutoApplyTransitionInputMode && bRestoreGameplayInputModeOnEndPlay)
	{
		ApplyTransitionInputMode(false);
	}

	HideTransitionWidget();
	Super::EndPlay(EndPlayReason);
}

void AARTransitionPlayerController::RequestTransitionContinue(const bool bReady)
{
	if (HasAuthority())
	{
		if (AARPlayerStateBase* ARPlayerState = GetPlayerState<AARPlayerStateBase>())
		{
			ARPlayerState->SetReadyForRun(bReady);
		}
		else
		{
			UE_LOG(ARLog, Verbose, TEXT("[Transition] Continue vote ignored: missing AR player state on '%s'."), *GetNameSafe(this));
		}
		return;
	}

	ServerRequestTransitionContinue(bReady);
}

void AARTransitionPlayerController::ServerRequestTransitionContinue_Implementation(const bool bReady)
{
	RequestTransitionContinue(bReady);
}

UUserWidget* AARTransitionPlayerController::ShowTransitionWidgetFromContext()
{
	return ShowTransitionWidget(ResolveTransitionWidgetClassFromContext());
}

UUserWidget* AARTransitionPlayerController::ShowTransitionWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!IsLocalController())
	{
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Transition] ShowTransitionWidget skipped on '%s': widget class is null."), *GetNameSafe(this));
		return nullptr;
	}

	if (TransitionWidget && TransitionWidget->GetClass() != WidgetClass)
	{
		HideTransitionWidget();
		TransitionWidget = nullptr;
	}

	if (!TransitionWidget)
	{
		TransitionWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	}

	if (!TransitionWidget)
	{
		UE_LOG(ARLog, Warning, TEXT("[Transition] ShowTransitionWidget failed on '%s': widget create failed."), *GetNameSafe(this));
		return nullptr;
	}

	if (!TransitionWidget->IsInViewport())
	{
		TransitionWidget->AddToViewport(TransitionWidgetZOrder);
	}

	if (bAutoApplyTransitionInputMode)
	{
		ApplyTransitionInputMode(true, TransitionWidget);
	}

	return TransitionWidget;
}

void AARTransitionPlayerController::HideTransitionWidget()
{
	if (TransitionWidget && TransitionWidget->IsInViewport())
	{
		TransitionWidget->RemoveFromParent();
	}
}

TSubclassOf<UUserWidget> AARTransitionPlayerController::ResolveTransitionWidgetClassFromContext() const
{
	const AARTransitionGameState* TransitionGameState = GetWorld() ? GetWorld()->GetGameState<AARTransitionGameState>() : nullptr;
	if (!TransitionGameState)
	{
		return DefaultTransitionWidgetClass;
	}

	const FARTransitionContext& Context = TransitionGameState->GetTransitionContext();
	if (Context.bFreshLoadEntry && FreshLoadTransitionWidgetClass)
	{
		return FreshLoadTransitionWidgetClass;
	}

	if (const TSubclassOf<UUserWidget>* ReasonClass = TransitionWidgetClassByReason.Find(Context.Reason))
	{
		if (*ReasonClass)
		{
			return *ReasonClass;
		}
	}

	if (const TSubclassOf<UUserWidget>* SourceClass = TransitionWidgetClassBySourceMode.Find(Context.SourceMode))
	{
		if (*SourceClass)
		{
			return *SourceClass;
		}
	}

	return DefaultTransitionWidgetClass;
}

void AARTransitionPlayerController::ApplyTransitionInputMode(const bool bEnable, UUserWidget* FocusWidget)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bEnable)
	{
		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		if (!FocusWidget)
		{
			FocusWidget = TransitionWidget;
		}
		if (FocusWidget)
		{
			InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}
