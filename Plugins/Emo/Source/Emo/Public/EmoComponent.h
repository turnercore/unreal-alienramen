/**
 * @file EmoComponent.h
 * @brief Replicated emotion display component with slot-aware state.
 */
#pragma once

#include "CoreMinimal.h"
#include "EmoTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
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

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets the shared base emotion tag shown to all viewers unless higher-priority overrides are active."))
	void SetEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets the base emotion tag for a specific player slot viewer context."))
	void SetEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears the shared base emotion tag used when no slot-specific base tag is set."))
	void ClearEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears the base emotion tag override for a specific player slot."))
	void ClearEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears all base emotion tags, including shared and slot-scoped entries."))
	void ClearAllEmotionTags();

	// Dialogue override helpers (higher priority than base state).
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets the dialogue-layer override emotion tag for all viewers."))
	void SetDialogueEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets the dialogue-layer override emotion tag for a specific player slot."))
	void SetDialogueEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears the shared dialogue-layer emotion override."))
	void ClearDialogueEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears the dialogue-layer emotion override for a specific player slot."))
	void ClearDialogueEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears all dialogue-layer overrides, including shared and slot-scoped entries."))
	void ClearAllDialogueEmotionTags();

	// Generic runtime override layer for built-on-top systems (for example ordering, scripted events, mode logic).
	// Highest-priority active source wins; ties resolve by most-recent write.
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets or updates a named system override source with priority for the shared viewer context."))
	void SetSystemEmotionTag(FName SourceId, FGameplayTag NewEmotionTag, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets a timed named system override for the shared viewer context and auto-clears when it expires."))
	void SetSystemEmotionTagForDuration(FName SourceId, FGameplayTag NewEmotionTag, float DurationSeconds = -1.0f, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets or updates a named system override source for a specific player slot."))
	void SetSystemEmotionTagForPlayerSlotTag(FName SourceId, FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets a timed named system override for a specific player slot and auto-clears when it expires."))
	void SetSystemEmotionTagForPlayerSlotTagForDuration(FName SourceId, FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag, float DurationSeconds = -1.0f, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears the shared system override entry for the given source id."))
	void ClearSystemEmotionTag(FName SourceId);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears the system override entry for a source id in one player slot context."))
	void ClearSystemEmotionTagForPlayerSlotTag(FName SourceId, FGameplayTag PlayerSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears every override entry owned by a source id across shared and slot-scoped contexts."))
	void ClearAllSystemEmotionTagsForSource(FName SourceId);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Clears all active system override sources and recomputes the displayed emotion tag."))
	void ClearAllSystemEmotionTags();

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the effective displayed emotion tag for a given player slot after applying priority layers."))
	FGameplayTag GetDisplayedEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the effective displayed emotion tag for a viewer controller using its player slot mapping."))
	FGameplayTag GetDisplayedEmotionTagForController(const APlayerController* ViewerController) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for the currently displayed emotion of a specific player slot."))
	bool TryResolveDisplayedEmotionIconForPlayerSlot(
		FGameplayTag PlayerSlotTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for the currently displayed emotion of a viewer controller."))
	bool TryResolveDisplayedEmotionIconForController(
		const APlayerController* ViewerController,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for an explicit emotion tag using resolver fallback rules."))
	bool TryResolveEmotionIconForTag(
		FGameplayTag EmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Resolves icon content for PreviewEmotionTag used by editor/runtime preview paths."))
	bool TryResolvePreviewEmotionIcon(
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the world-space anchor location used to project this component's emotion icon."))
	FVector GetEmotionAnchorWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Calculates a facing rotation so emotion rendering can billboard toward the viewer controller."))
	bool GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Sets the optional speaker tag associated with this emotion component for dialogue integration."))
	void SetRegisteredSpeakerTag(FGameplayTag NewSpeakerTag);

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the optional speaker tag currently associated with this emotion component."))
	FGameplayTag GetRegisteredSpeakerTag() const { return RegisteredSpeakerTag; }

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the configured icon draw size in screen pixels."))
	float GetIconScreenSize() const { return IconScreenSize; }

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the shared base emotion tag before dialogue/system override layers are applied."))
	FGameplayTag GetBaseEmotionTag() const { return BaseEmotionState.SharedEmotionTag; }

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns the authored preview emotion tag used when no replicated display state is active."))
	FGameplayTag GetPreviewEmotionTag() const;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when displayed emotion state changes and UI should refresh."))
	FEmoOnEmotionDisplayStateChanged OnEmotionDisplayStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when the effective displayed emotion tag changes. Params: NewEmotionTag, OldEmotionTag."))
	FEmoOnEmotionDisplayChanged OnEmotionDisplayChanged;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when the effective displayed emotion tag becomes invalid (cleared)."))
	FEmoOnEmotionDisplayCleared OnEmotionDisplayCleared;

	UPROPERTY(BlueprintAssignable, Category = "Emo|Emotion", meta = (ToolTip = "Broadcast when active system override source count changes. Param: ActiveEntryCount."))
	FEmoOnEmotionQueueChanged OnEmotionQueueChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_BaseEmotionState(FEmoDisplayState OldState);

	UFUNCTION()
	void OnRep_DialogueOverrideState(FEmoDisplayState OldState);

	UFUNCTION()
	void OnRep_SystemOverrideState(FEmoDisplayState OldState);

private:
	struct FSystemEmotionSourceState
	{
		FEmoDisplayState State;
		int32 Priority = 0;
		uint64 LastWriteSerial = 0;
	};

	static FGameplayTag GetStateSlotTag(const FEmoDisplayState& State, const FGameplayTag& PlayerSlotTag);
	static void SetStateSlotTag(FEmoDisplayState& State, const FGameplayTag& PlayerSlotTag, const FGameplayTag& EmotionTag);
	static bool AreDisplayStatesEqual(const FEmoDisplayState& Left, const FEmoDisplayState& Right);
	static bool HasAnyStateTag(const FEmoDisplayState& State);
	static FName MakeTimedSlotKey(FName SourceId, const FGameplayTag& SlotTag);

	bool IsAuthorityOwner() const;
	void ForceOwnerNetUpdate() const;
	bool RebuildSystemOverrideStateFromSources();
	float ResolveTimedSystemOverrideDurationSeconds(float RequestedDurationSeconds) const;
	void SetTimedSystemOverrideClearTimer(FName SourceId, float DurationSeconds);
	void SetTimedSystemOverrideSlotClearTimer(FName SourceId, const FGameplayTag& SlotTag, float DurationSeconds);
	void ClearTimedSystemOverrideTimer(FName SourceId);
	void ClearTimedSystemOverrideSlotTimer(FName SourceId, const FGameplayTag& SlotTag);
	void ClearAllTimedSystemOverrideTimersForSource(FName SourceId);
	UFUNCTION()
	void HandleTimedSystemOverrideClear(FName SourceId);
	UFUNCTION()
	void HandleTimedSystemOverrideSlotClear(FName SourceId, FGameplayTag SlotTag);
	void BroadcastDisplayStateDelta(const FGameplayTag& OldDisplayedTag);
#if WITH_EDITOR
	void RefreshEditorPreviewBillboard();
	void DestroyEditorPreviewBillboard();
#endif

	UPROPERTY(Replicated)
	FGameplayTag RegisteredSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "World-space offset from the actor top-bound anchor."))
	FVector AnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", ToolTip = "Desired icon size for HUD and editor preview rendering."))
	float IconScreenSize = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "Editor/runtime preview emotion tag used when no active replicated emotion state is present."))
	FGameplayTag PreviewEmotionTag;

	UPROPERTY(ReplicatedUsing = OnRep_BaseEmotionState, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "Base Emotion State", ToolTip = "Replicated base emotion state. Dialogue override state can temporarily supersede this."))
	FEmoDisplayState BaseEmotionState;

	UPROPERTY(ReplicatedUsing = OnRep_DialogueOverrideState, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "Dialogue Override State", ToolTip = "Replicated dialogue-scoped override state (higher priority than base emotion state)."))
	FEmoDisplayState DialogueOverrideState;

	UPROPERTY(ReplicatedUsing = OnRep_SystemOverrideState, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "System Override State", ToolTip = "Replicated top-priority runtime override state resolved from active system sources."))
	FEmoDisplayState SystemOverrideState;

	TMap<FName, FSystemEmotionSourceState> SystemEmotionSources;
	uint64 NextSystemEmotionWriteSerial = 1;
	TMap<FName, FTimerHandle> TimedSystemOverrideClearHandles;
	TMap<FName, FTimerHandle> TimedSystemOverrideSlotClearHandles;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<class UBillboardComponent> EditorPreviewBillboardComponent = nullptr;
#endif
};
