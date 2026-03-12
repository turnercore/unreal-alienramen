#include "ARTransitionPlayerController.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"

AARTransitionPlayerController::AARTransitionPlayerController()
{
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
