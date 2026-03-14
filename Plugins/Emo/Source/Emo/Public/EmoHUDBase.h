/**
 * @file EmoHUDBase.h
 * @brief EmoHUDBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFramework/HUD.h"
#include "EmoHUDBase.generated.h"

class AGameStateBase;
class APlayerState;
class APlayerController;
class AActor;
struct FStreamableHandle;
class UEmoComponent;
class UCanvas;
class UTexture2D;

UCLASS()
class EMO_API AEmoHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	AEmoHUDBase();

	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Local-only HUD init entrypoint called by the owning local player controller.
	UFUNCTION(BlueprintCallable, Category = "Emo|UI|HUD", meta = (ToolTip = "Runs HUD initialization with controller and state context for widget and cache setup."))
	virtual void RequestHUDInitialization(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	UFUNCTION(BlueprintCallable, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Configures or queries world-space emotion HUD rendering behavior."))
	bool TryProjectEmotionForActor(
		AActor* TargetActor,
		FVector2D& OutScreenPosition,
		FGameplayTag& OutDisplayedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Configures or queries world-space emotion HUD rendering behavior."))
	bool TryProjectEmotionForComponent(
		const UEmoComponent* EmotionComponent,
		FVector2D& OutScreenPosition,
		FGameplayTag& OutDisplayedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutDisplayedIcon) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Configures or queries world-space emotion HUD rendering behavior."))
	void SetEmotionRenderingSuppressed(bool bSuppressed, FName Reason = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Configures or queries world-space emotion HUD rendering behavior."))
	void SetEmotionRenderingEnabled(bool bEnabled) { bEnableEmotionView = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Returns HUD emotion rendering state without side effects."))
	bool IsEmotionRenderingEnabled() const { return bEnableEmotionView; }

	UFUNCTION(BlueprintPure, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Returns HUD emotion rendering state without side effects."))
	bool IsEmotionRenderingSuppressed() const { return SuppressionReasons.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Emo|UI|HUD|Emotion", meta = (ToolTip = "Returns HUD emotion rendering state without side effects."))
	int32 GetEmotionRenderingSuppressionReasonCount() const { return SuppressionReasons.Num(); }

protected:
	// Local-only BP hook for HUD/widget creation/rebind.
	UFUNCTION(BlueprintImplementableEvent, Category = "Emo|UI|HUD", meta = (ToolTip = "Blueprint hook fired when HUD initialization is requested."))
	void BP_OnHUDInitializationRequested(APlayerController* SourceController, APlayerState* CurrentPlayerState, AGameStateBase* CurrentGameState);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "Configures HUD emotion rendering behavior."))
	bool bEnableEmotionView = true;

	/** Additional scale multiplier applied to emotion icon draw size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", ToolTip = "Additional scale multiplier applied to emotion icon draw size."))
	float EmotionIconRenderScale = 1.0f;

	/** Minimum screen size (pixels) after projection; icons smaller than this are culled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "Minimum on-screen max dimension in pixels for emotion icons after world projection scaling."))
	float MinimumIconScreenSizePixels = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "Configures HUD emotion rendering behavior."))
	bool bHideOwningPawnEmotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When enabled, emotion icons are hidden if actor body visibility is blocked for the local viewer."))
	bool bHideOccludedEmotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "Configures HUD emotion rendering behavior."))
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECollisionChannel::ECC_Visibility;

	/** How often (seconds) to rebuild the cached list of emotion components. 0 = scan every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "How often (seconds) to rebuild the runtime emotion component cache. 0 disables caching and scans every frame."))
	float EmotionComponentCacheRefreshSeconds = 0.5f;

	/** Asynchronous icon loads instead of blocking DrawHUD for missing textures. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When true, unresolved icon soft references are requested asynchronously and skipped until loaded instead of synchronously loading during DrawHUD."))
	bool bAsyncLoadEmotionIcons = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When true, runs extra fallback occlusion traces on Visibility and Camera channels in addition to OcclusionTraceChannel. Disable for lower trace cost."))
	bool bUseOcclusionFallbackChannels = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When enabled, icons beyond MaxEmotionRenderDistance are skipped before projection/occlusion work."))
	bool bEnableDistanceCull = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "Maximum camera-to-anchor distance (cm) for HUD emotion rendering. 0 disables distance culling."))
	float MaxEmotionRenderDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When enabled, icons outside MaxEmotionViewAngleDegrees from the view forward vector are skipped before projection/occlusion work."))
	bool bEnableFOVCull = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|UI|HUD|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0", ToolTip = "Maximum off-center angle in degrees from camera forward to the emotion anchor for rendering."))
	float MaxEmotionViewAngleDegrees = 95.0f;

private:
	int32 RenderEmotionView();
	void RefreshEmotionComponentCacheIfNeeded();
	void QueueAsyncIconLoad(const TSoftObjectPtr<UTexture2D>& IconPtr);
	void CleanupAsyncEmotionLoads();
	bool IsEmotionVisibleForViewer(const UEmoComponent* EmotionComponent, const APlayerController* LocalController) const;
	static bool ShouldLogEmotionRenderVerbose();

	mutable TWeakObjectPtr<UCanvas> ActiveProjectionCanvas;
	mutable TWeakObjectPtr<const APlayerController> ActiveProjectionController;
	TArray<TWeakObjectPtr<UEmoComponent>> CachedEmotionComponents;
	TSet<FSoftObjectPath> PendingAsyncIconLoads;
	TArray<TSharedPtr<FStreamableHandle>> ActiveAsyncIconHandles;
	double LastEmotionComponentCacheRefreshTimeSeconds = -1.0;
	mutable uint32 OcclusionTraceCountThisFrame = 0;
	TSet<FName> SuppressionReasons;
};
