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
class APlayerState;
class AActor;
class UAREmotionComponent;
class UARHUDEmotionViewComponent;
class UTexture2D;

UCLASS()
class ALIENRAMEN_API AARHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	AARHUDBase();

	virtual void DrawHUD() override;

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

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	UARHUDEmotionViewComponent* GetEmotionViewComponent() const { return EmotionViewComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	UARHUDEmotionViewComponent* GetEmotionRenderComponent() const { return EmotionViewComponent; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void SetEmotionRenderingSuppressed(bool bSuppressed, FName Reason = NAME_None);

protected:
	// Local-only BP hook for HUD/widget creation/rebind.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|HUD")
	void BP_OnHUDInitializationRequested(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UARHUDEmotionViewComponent> EmotionViewComponent;
};

