/**
 * @file ARHUDEmotionViewComponent.h
 * @brief HUD component that projects and renders replicated emotion icons.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "Components/ActorComponent.h"
#include "ARHUDEmotionViewComponent.generated.h"

class AActor;
class UAREmotionComponent;
class UCanvas;
class UTexture2D;

UCLASS(
	ClassGroup=(AlienRamen),
	BlueprintType,
	Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Emotion View", ToolTip="Projects and renders replicated overhead emotion icons for the owning HUD."))
class ALIENRAMEN_API UARHUDEmotionViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARHUDEmotionViewComponent();

	// Called from the owning HUD DrawHUD path to render overhead emotion icons.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	int32 RenderEmotionView();

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

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void SetEmotionViewEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	bool IsEmotionViewEnabled() const { return bEnableEmotionView; }

	// Suppression is reference-counted by reason key; passing NAME_None to unsuppress clears all reasons.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void SetEmotionViewSuppressed(bool bSuppressed, FName Reason = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void ClearEmotionViewSuppression(FName Reason);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void ClearAllEmotionViewSuppression();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	bool IsEmotionViewSuppressed() const { return SuppressionReasons.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	int32 GetEmotionViewSuppressionReasonCount() const { return SuppressionReasons.Num(); }

private:
	class AHUD* ResolveOwningHUD() const;
	static bool ShouldLogEmotionRenderVerbose();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true"))
	bool bEnableEmotionView = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", ToolTip = "Additional scale multiplier applied to emotion icon draw size."))
	float EmotionIconRenderScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true"))
	bool bHideOwningPawnEmotion = false;

	TSet<FName> SuppressionReasons;
};
