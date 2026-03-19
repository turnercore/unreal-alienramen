#include "ARHUDBase.h"

#include "AREmotionViewerTags.h"
#include "GameFramework/Pawn.h"

void AARHUDBase::RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState)
{
	SetViewedEmotionTags(AREmotion::BuildEmotionViewerTags(CurrentPlayerState, SourceController ? SourceController->GetPawn() : nullptr));
	Super::RequestHUDInitialization(SourceController, CurrentPlayerState, CurrentGameState);
}
