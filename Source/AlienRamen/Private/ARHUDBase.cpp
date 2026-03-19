#include "ARHUDBase.h"

#include "AREmotionViewerTags.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ParleyDialogueWidgetBase.h"
#include "ParleyPlayerControllerInterface.h"
#include "GameFramework/Pawn.h"
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
}

void AARHUDBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveDialogueWidget();
	Super::EndPlay(EndPlayReason);
}

void AARHUDBase::RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	SetViewedEmotionTags(AREmotion::BuildEmotionViewerTags(CurrentPlayerState, SourceController ? SourceController->GetPawn() : nullptr));
	EnsureDialogueWidget(SourceController, CurrentPlayerState, CurrentGameState);
	Super::RequestHUDInitialization(SourceController, CurrentPlayerState, CurrentGameState);
}

void AARHUDBase::EnsureDialogueWidget(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	(void)CurrentPlayerState;
	(void)CurrentGameState;

	if (!SourceController || !SourceController->IsLocalController())
	{
		return;
	}

	if (!bAutoCreateDialogueWidget)
	{
		return;
	}

	if (!DialogueWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue|UI] HUD dialogue widget skipped on '%s': DialogueWidgetClass is not set."), *GetNameSafe(this));
		return;
	}

	if (!DialogueWidget || DialogueWidget->GetClass() != DialogueWidgetClass)
	{
		RemoveDialogueWidget();
		DialogueWidget = CreateWidget<UParleyDialogueWidgetBase>(SourceController, DialogueWidgetClass);
	}

	if (!DialogueWidget)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue|UI] Failed to create HUD dialogue widget for '%s'."), *GetNameSafe(this));
		return;
	}

	if (!DialogueWidget->IsInViewport())
	{
		DialogueWidget->AddToViewport(DialogueWidgetZOrder);
	}

	DialogueWidget->InitializeDialogueWidget(SourceController);

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(SourceController);
	if (ControllerInterface)
	{
		DialogueWidget->SetBoundCharacterTag(ControllerInterface->GetCharacterTag());
	}
}

void AARHUDBase::RemoveDialogueWidget()
{
	if (!DialogueWidget)
	{
		return;
	}

	DialogueWidget->RemoveFromParent();
	DialogueWidget = nullptr;
}
