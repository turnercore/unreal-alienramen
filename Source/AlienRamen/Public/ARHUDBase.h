/**
 * @file ARHUDBase.h
 * @brief ARHUDBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFramework/HUD.h"
#include "ARHUDBase.generated.h"

class AARPlayerController;
class AGameStateBase;
class APlayerState;
class APlayerController;
class AActor;
struct FStreamableHandle;
class UAREmotionComponent;
class UCanvas;
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

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void SetEmotionRenderingSuppressed(bool bSuppressed, FName Reason = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|UI|HUD|Emotion")
	void SetEmotionRenderingEnabled(bool bEnabled) { bEnableEmotionView = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	bool IsEmotionRenderingEnabled() const { return bEnableEmotionView; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	bool IsEmotionRenderingSuppressed() const { return SuppressionReasons.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|UI|HUD|Emotion")
	int32 GetEmotionRenderingSuppressionReasonCount() const { return SuppressionReasons.Num(); }

protected:
	// Local-only BP hook for HUD/widget creation/rebind.
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|UI|HUD")
	void BP_OnHUDInitializationRequested(AARPlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true"))
	bool bEnableEmotionView = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", ToolTip = "Additional scale multiplier applied to emotion icon draw size."))
	float EmotionIconRenderScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "Minimum on-screen max dimension in pixels for emotion icons after world projection scaling."))
	float MinimumIconScreenSizePixels = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true"))
	bool bHideOwningPawnEmotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When enabled, emotion icons are hidden if actor body visibility is blocked for the local viewer."))
	bool bHideOccludedEmotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECollisionChannel::ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "How often (seconds) to rebuild the runtime emotion component cache. 0 disables caching and scans every frame."))
	float EmotionComponentCacheRefreshSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When true, unresolved icon soft references are requested asynchronously and skipped until loaded instead of synchronously loading during DrawHUD."))
	bool bAsyncLoadEmotionIcons = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When true, runs extra fallback occlusion traces on Visibility and Camera channels in addition to OcclusionTraceChannel. Disable for lower trace cost."))
	bool bUseOcclusionFallbackChannels = false;

private:
	int32 RenderEmotionView();
	void RefreshEmotionComponentCacheIfNeeded();
	void QueueAsyncIconLoad(const TSoftObjectPtr<UTexture2D>& IconPtr);
	bool IsEmotionVisibleForViewer(const UAREmotionComponent* EmotionComponent, const APlayerController* LocalController) const;
	static bool ShouldLogEmotionRenderVerbose();

	mutable TWeakObjectPtr<UCanvas> ActiveProjectionCanvas;
	mutable TWeakObjectPtr<const APlayerController> ActiveProjectionController;
	TArray<TWeakObjectPtr<UAREmotionComponent>> CachedEmotionComponents;
	TSet<FSoftObjectPath> PendingAsyncIconLoads;
	TArray<TSharedPtr<FStreamableHandle>> ActiveAsyncIconHandles;
	double LastEmotionComponentCacheRefreshTimeSeconds = -1.0;
	mutable uint32 OcclusionTraceCountThisFrame = 0;
	TSet<FName> SuppressionReasons;
};

