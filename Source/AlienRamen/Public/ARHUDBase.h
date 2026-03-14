/**
 * @file ARHUDBase.h
 * @brief Game-side HUD base that extends EmoHUDBase.
 */
#pragma once

#include "CoreMinimal.h"
#include "EmoHUDBase.h"
#include "ARHUDBase.generated.h"

class AGameStateBase;
class APlayerState;
class APlayerController;

UCLASS()
class ALIENRAMEN_API AARHUDBase : public AEmoHUDBase
{
	GENERATED_BODY()

public:
	virtual void RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState) override;
};
