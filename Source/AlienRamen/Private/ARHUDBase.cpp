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
	if (bAutoCreateDialogueWidget)
	{
		EnsureDialogueWidget(SourceController, CurrentPlayerState, CurrentGameState);
	}
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

	if (!DialogueWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue|UI] HUD dialogue widget skipped on '%s': DialogueWidgetClass is not set."), *GetNameSafe(this));
		return;
	}

	bool bWidgetRecreated = false;
	if (!DialogueWidget || DialogueWidget->GetClass() != DialogueWidgetClass)
	{
		RemoveDialogueWidget();
		DialogueWidget = CreateWidget<UParleyDialogueWidgetBase>(SourceController, DialogueWidgetClass);
		bWidgetRecreated = true;
	}

	if (!DialogueWidget)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue|UI] Failed to create HUD dialogue widget for '%s'."), *GetNameSafe(this));
		return;
	}

	if (!DialogueWidget->IsInViewport())
	{
		DialogueWidget->AddToPlayerScreen(DialogueWidgetZOrder);
	}

	const APlayerController* OwningPlayerController = Cast<APlayerController>(DialogueWidget->GetOwningPlayer());
	bool bWidgetInitialized = false;
	if (bWidgetRecreated || OwningPlayerController != SourceController)
	{
		DialogueWidget->InitializeDialogueWidget(SourceController);
		bWidgetInitialized = true;
	}

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(SourceController);
	if (ControllerInterface)
	{
		const FGameplayTag PreviousBoundCharacterTag = DialogueWidget->GetBoundCharacterTag();
		const FGameplayTag NewBoundCharacterTag = ControllerInterface->GetCharacterTag();
		DialogueWidget->SetBoundCharacterTag(NewBoundCharacterTag);
		if (!bWidgetInitialized && PreviousBoundCharacterTag != NewBoundCharacterTag)
		{
			// Character swaps can reuse the same controller/HUD instance; refresh the cached view immediately.
			DialogueWidget->InitializeDialogueWidget(SourceController);
		}
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
