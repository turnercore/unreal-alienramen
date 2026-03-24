/**
 * @file EmoComponent.h
 * @brief Replicated emotion display component with generic viewer-tag registrations.
 */
#pragma once

#include "CoreMinimal.h"
#include "EmoTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EmoComponent.generated.h"

class APlayerController;
class UTexture2D;
struct FPropertyChangedEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEmoOnEmotionDisplayStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEmoOnEmotionDisplayChanged, FGameplayTag, NewEmotionTag, FGameplayTag, OldEmotionTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEmoOnEmotionDisplayCleared);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEmoOnEmotionQueueChanged, int32, ActiveEntryCount);

UCLASS(
	ClassGroup=(Emo),
	BlueprintType,
	Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Dialogue Emotion Component", ToolTip="Replicated dialogue emotion display component with anchor and icon preview authoring helpers."))
class EMO_API UEmoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEmoComponent();

	/**
	 * Sets or updates a generic emotion registration.
	 * Reusing the same source id and exact target viewer tags overwrites the existing entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets or updates a named emotion registration. Empty target viewer tags create a global fallback registration."))
	void SetEmotionRegistration(FName SourceId, FGameplayTag NewEmotionTag, int32 Priority = 0, FGameplayTagContainer TargetViewerTags = FGameplayTagContainer());

	/**
	 * Sets a timed generic emotion registration.
	 * The entry auto-clears using the same SourceId + exact TargetViewerTags key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets or updates a timed emotion registration. Empty target viewer tags create a global fallback registration."))
	void SetEmotionRegistrationForDuration(
		FName SourceId,
		FGameplayTag NewEmotionTag,
		float DurationSeconds = -1.0f,
		int32 Priority = 0,
		FGameplayTagContainer TargetViewerTags = FGameplayTagContainer());

	/** Clears one registration entry identified by SourceId + exact TargetViewerTags. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears one emotion registration entry identified by source id and exact target viewer tags. Empty target viewer tags address the global entry for that source."))
	void ClearEmotionRegistration(FName SourceId, FGameplayTagContainer TargetViewerTags = FGameplayTagContainer());

	/** Clears every registration entry owned by SourceId across global and targeted contexts. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears every emotion registration owned by the provided source id."))
	void ClearAllEmotionRegistrationsForSource(FName SourceId);

	/** Clears every active emotion registration on this component. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears every active emotion registration on this component."))
	void ClearAllEmotionRegistrations();

	/** Resolves the effective displayed emotion tag for an explicit HUD viewer tag container. */
	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the effective displayed emotion tag for the provided HUD viewer tags. Empty viewer tags only resolve global registrations."))
	FGameplayTag GetDisplayedEmotionTagForViewerTags(FGameplayTagContainer ViewerTags) const;

	/** Resolves icon content for the current displayed emotion of a specific HUD viewer tag container. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for the current displayed emotion of the provided HUD viewer tags."))
	bool TryResolveDisplayedEmotionIconForViewerTags(
		FGameplayTagContainer ViewerTags,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	/** Resolves icon content for an explicit emotion tag using resolver fallback rules. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for an explicit emotion tag using resolver fallback rules."))
	bool TryResolveEmotionIconForTag(
		FGameplayTag EmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	/** Resolves icon content for PreviewEmotionTag used by editor/runtime preview paths. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for PreviewEmotionTag used by editor/runtime preview paths."))
	bool TryResolvePreviewEmotionIcon(
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	/** Returns the world-space anchor location used to project this component's emotion icon. */
	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the world-space anchor location used to project this component's emotion icon."))
	FVector GetEmotionAnchorWorldLocation() const;

	/** Calculates a facing rotation so emotion rendering can billboard toward the viewer controller. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Calculates a facing rotation so emotion rendering can billboard toward the viewer controller."))
	bool GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const;

	/** Sets the optional speaker tag associated with this emotion component for dialogue integration. */
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets the optional speaker tag associated with this emotion component for dialogue integration."))
	void SetRegisteredSpeakerTag(FGameplayTag NewSpeakerTag);

	/** Returns the optional speaker tag currently associated with this emotion component. */
	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the optional speaker tag currently associated with this emotion component."))
	FGameplayTag GetRegisteredSpeakerTag() const { return RegisteredSpeakerTag; }

	/** Returns the configured icon draw size in screen pixels. */
	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the configured icon draw size in screen pixels."))
	float GetIconScreenSize() const { return IconScreenSize; }

	/** Returns the authored preview emotion tag used when no replicated display state is active. */
	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the authored preview emotion tag used when no replicated display state is active."))
	FGameplayTag GetPreviewEmotionTag() const;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when displayed emotion state changes and UI should refresh."))
	FEmoOnEmotionDisplayStateChanged OnEmotionDisplayStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when the empty-viewer/global displayed emotion tag changes. Params: NewEmotionTag, OldEmotionTag."))
	FEmoOnEmotionDisplayChanged OnEmotionDisplayChanged;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when the effective empty-viewer/global displayed emotion tag becomes invalid (cleared)."))
	FEmoOnEmotionDisplayCleared OnEmotionDisplayCleared;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when active emotion registration count changes. Param: ActiveEntryCount."))
	FEmoOnEmotionQueueChanged OnEmotionQueueChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_EmotionRegistrations(const TArray<FEmoEmotionRegistration>& OldRegistrations);

private:
	static bool AreTagsEqual(const FGameplayTag& Left, const FGameplayTag& Right);
	static bool AreViewerTagContainersEquivalent(const FGameplayTagContainer& Left, const FGameplayTagContainer& Right);
	static bool AreRegistrationsEquivalent(const FEmoEmotionRegistration& Left, const FEmoEmotionRegistration& Right);
	static FGameplayTagContainer SanitizeViewerTags(const FGameplayTagContainer& ViewerTags);
	static FName MakeRegistrationTimerKey(FName SourceId, const FGameplayTagContainer& TargetViewerTags);

	int32 FindEmotionRegistrationIndex(FName SourceId, const FGameplayTagContainer& TargetViewerTags) const;
	bool IsAuthorityOwner() const;
	void ForceOwnerNetUpdate() const;
	float ResolveTimedEmotionRegistrationDurationSeconds(float RequestedDurationSeconds) const;
	void SetTimedEmotionRegistrationClearTimer(FName SourceId, const FGameplayTagContainer& TargetViewerTags, float DurationSeconds);
	void ClearTimedEmotionRegistrationTimer(FName SourceId, const FGameplayTagContainer& TargetViewerTags);
	void ClearAllTimedEmotionRegistrationTimersForSource(FName SourceId);

	UFUNCTION()
	void HandleTimedEmotionRegistrationClear(FName SourceId, FGameplayTagContainer TargetViewerTags);

	/**
	 * Broadcasts display-state change delegates after recomputing effective display tags for the
	 * empty-viewer/global context. Tag-scoped-only changes still raise the state-change delegate.
	 */
	void BroadcastRegistrationDelta(const TArray<FEmoEmotionRegistration>& OldRegistrations);

#if WITH_EDITOR
	void RefreshEditorPreviewBillboard();
	void DestroyEditorPreviewBillboard();
#endif

	UPROPERTY(Replicated)
	FGameplayTag RegisteredSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "World-space offset from the actor top-bound anchor."))
	FVector AnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);

	/** When true, a zero anchor offset uses the configured default offset from UEmoSettings. Disable to allow a literal zero override. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "When enabled, a zero component offset falls back to the configured default anchor offset. Disable to allow a literal zero offset."))
	bool bUseSettingsDefaultAnchorWorldOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", ToolTip = "Desired icon size for HUD and editor preview rendering."))
	float IconScreenSize = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "Editor/runtime preview emotion tag used when no active replicated emotion state is present."))
	FGameplayTag PreviewEmotionTag;

	/** Server-authoritative generic registration pool resolved against HUD viewer tags. */
	UPROPERTY(ReplicatedUsing = OnRep_EmotionRegistrations, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "Emotion Registrations", ToolTip = "Replicated emotion registrations resolved against HUD viewer tags. Empty target viewer tags act as global fallback entries."))
	TArray<FEmoEmotionRegistration> EmotionRegistrations;

	uint64 NextEmotionWriteSerial = 1;
	TMap<FName, FTimerHandle> TimedEmotionRegistrationClearHandles;

#if WITH_EDITORONLY_DATA
	/** Editor-only preview billboard kept on the owner actor so preview emotion changes replace the existing icon instead of stacking duplicates. */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<class UBillboardComponent> EditorPreviewBillboardComponent = nullptr;
#endif
};
