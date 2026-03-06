/**
 * @file ARHUDBase.h
 * @brief ARHUDBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ARHUDBase.generated.h"

class AARPlayerController;
class AGameStateBase;

UCLASS()
class ALIENRAMEN_API AARHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	// Local-only HUD init entrypoint called by AARPlayerController.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD")
	virtual void RequestHUDInitialization(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

protected:
	// Local-only BP hook for HUD/widget creation/rebind.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|HUD")
	void BP_OnHUDInitializationRequested(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);
};

