/**
 * @file ARHUDBase.h
 * @brief ARHUDBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFramework/HUD.h"
#include "ARHUDBase.generated.h"

class AARPlayerController;
class AGameStateBase;
class AActor;
class UAREmotionComponent;
class UTexture2D;

UCLASS()
class ALIENRAMEN_API AARHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	// Local-only HUD init entrypoint called by AARPlayerController.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD")
	virtual void RequestHUDInitialization(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	bool TryProjectEmotionForActor(
		AActor* TargetActor,
		FVector2D& OutScreenPosition,
		FGameplayTag& OutDisplayedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	bool TryProjectEmotionForComponent(
		const UAREmotionComponent* EmotionComponent,
		FVector2D& OutScreenPosition,
		FGameplayTag& OutDisplayedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const;

protected:
	// Local-only BP hook for HUD/widget creation/rebind.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|HUD")
	void BP_OnHUDInitializationRequested(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);
};

