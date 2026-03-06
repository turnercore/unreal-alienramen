#include "ARHUDBase.h"

#include "ARPlayerController.h"

void AARHUDBase::RequestHUDInitialization(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	if (!SourceController || !SourceController->IsLocalController())
	{
		return;
	}

	BP_OnHUDInitializationRequested(SourceController, CurrentPlayerState, CurrentGameState);
}

