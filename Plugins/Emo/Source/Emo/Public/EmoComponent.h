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

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearAllEmotionTags();

	// Dialogue override helpers (higher priority than base state).
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetDialogueEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetDialogueEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearDialogueEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearDialogueEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearAllDialogueEmotionTags();

	// Generic runtime override layer for built-on-top systems (for example ordering, scripted events, mode logic).
	// Highest-priority active source wins; ties resolve by most-recent write.
	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetSystemEmotionTag(FName SourceId, FGameplayTag NewEmotionTag, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetSystemEmotionTagForDuration(FName SourceId, FGameplayTag NewEmotionTag, float DurationSeconds = -1.0f, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetSystemEmotionTagForPlayerSlotTag(FName SourceId, FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetSystemEmotionTagForPlayerSlotTagForDuration(FName SourceId, FGameplayTag PlayerSlotTag, FGameplayTag NewEmotionTag, float DurationSeconds = -1.0f, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearSystemEmotionTag(FName SourceId);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearSystemEmotionTagForPlayerSlotTag(FName SourceId, FGameplayTag PlayerSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearAllSystemEmotionTagsForSource(FName SourceId);

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void ClearAllSystemEmotionTags();

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
	FGameplayTag GetDisplayedEmotionTagForPlayerSlotTag(FGameplayTag PlayerSlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
	FGameplayTag GetDisplayedEmotionTagForController(const APlayerController* ViewerController) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	bool TryResolveDisplayedEmotionIconForPlayerSlot(
		FGameplayTag PlayerSlotTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	bool TryResolveDisplayedEmotionIconForController(
		const APlayerController* ViewerController,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	bool TryResolveEmotionIconForTag(
		FGameplayTag EmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	bool TryResolvePreviewEmotionIcon(
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
	FVector GetEmotionAnchorWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	bool GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const;

	UFUNCTION(BlueprintCallable, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Mutates or resolves this component's replicated emotion state."))
	void SetRegisteredSpeakerTag(FGameplayTag NewSpeakerTag);

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
	FGameplayTag GetRegisteredSpeakerTag() const { return RegisteredSpeakerTag; }

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
	float GetIconScreenSize() const { return IconScreenSize; }

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
	FGameplayTag GetBaseEmotionTag() const { return BaseEmotionState.SharedEmotionTag; }

	UFUNCTION(BlueprintPure, Category = "Emo|Dialogue|Emotion", meta = (ToolTip = "Returns computed emotion display state without mutating replicated data."))
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emo|Emotion", meta = (AllowPrivateAccess = "true", Categories = "Dialogue", ToolTip = "Editor/runtime preview emotion tag used when no active replicated emotion state is present."))
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
