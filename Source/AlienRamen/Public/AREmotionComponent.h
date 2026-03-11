/**
 * @file AREmotionComponent.h
 * @brief Replicated emotion display component with slot-aware state.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "AREmotionTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "AREmotionComponent.generated.h"

class AARPlayerController;
class AActor;
class APlayerController;
class UTexture2D;
class USceneComponent;
struct FPropertyChangedEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnEmotionDisplayStateChanged);

UCLASS(
	ClassGroup=(AlienRamen),
	BlueprintType,
	Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Dialogue Emotion Component", ToolTip="Replicated dialogue emotion display component with anchor and icon preview authoring helpers."))
class ALIENRAMEN_API UAREmotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAREmotionComponent();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearAllEmotionTags();

	// Dialogue override helpers (higher priority than base state).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetDialogueEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetDialogueEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearDialogueEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearDialogueEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearAllDialogueEmotionTags();

	// Generic runtime override layer for built-on-top systems (for example ordering, scripted events, mode logic).
	// Highest-priority active source wins; ties resolve by most-recent write.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetSystemEmotionTag(FName SourceId, FGameplayTag NewEmotionTag, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetSystemEmotionTagForDuration(FName SourceId, FGameplayTag NewEmotionTag, float DurationSeconds = -1.0f, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetSystemEmotionTagForPlayerSlot(FName SourceId, EARPlayerSlot PlayerSlot, FGameplayTag NewEmotionTag, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetSystemEmotionTagForPlayerSlotForDuration(FName SourceId, EARPlayerSlot PlayerSlot, FGameplayTag NewEmotionTag, float DurationSeconds = -1.0f, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearSystemEmotionTag(FName SourceId);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearSystemEmotionTagForPlayerSlot(FName SourceId, EARPlayerSlot PlayerSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearAllSystemEmotionTagsForSource(FName SourceId);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void ClearAllSystemEmotionTags();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	FGameplayTag GetDisplayedEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	FGameplayTag GetDisplayedEmotionTagForController(const AARPlayerController* ViewerController) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	bool TryResolveDisplayedEmotionIconForPlayerSlot(
		EARPlayerSlot PlayerSlot,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	bool TryResolveDisplayedEmotionIconForController(
		const AARPlayerController* ViewerController,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	bool TryResolveEmotionIconForTag(
		FGameplayTag EmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	bool TryResolvePreviewEmotionIcon(
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	FVector GetEmotionAnchorWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	bool GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Emotion")
	void SetRegisteredSpeakerTag(FGameplayTag NewSpeakerTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	FGameplayTag GetRegisteredSpeakerTag() const { return RegisteredSpeakerTag; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	float GetIconScreenSize() const { return IconScreenSize; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	FGameplayTag GetBaseEmotionTag() const { return BaseEmotionState.SharedEmotionTag; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	FGameplayTag GetPreviewEmotionTag() const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Emotion")
	FAROnEmotionDisplayStateChanged OnEmotionDisplayStateChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_BaseEmotionState(FAREmotionDisplayState OldState);

	UFUNCTION()
	void OnRep_DialogueOverrideState(FAREmotionDisplayState OldState);

	UFUNCTION()
	void OnRep_SystemOverrideState(FAREmotionDisplayState OldState);

private:
	struct FSystemEmotionSourceState
	{
		FAREmotionDisplayState State;
		int32 Priority = 0;
		uint64 LastWriteSerial = 0;
	};

	static FGameplayTag GetStateSlotTag(const FAREmotionDisplayState& State, EARPlayerSlot PlayerSlot);
	static void SetStateSlotTag(FAREmotionDisplayState& State, EARPlayerSlot PlayerSlot, const FGameplayTag& EmotionTag);
	static bool AreDisplayStatesEqual(const FAREmotionDisplayState& Left, const FAREmotionDisplayState& Right);
	static bool HasAnyStateTag(const FAREmotionDisplayState& State);
	static FName MakeTimedSlotKey(FName SourceId, EARPlayerSlot Slot);

	bool IsAuthorityOwner() const;
	void ForceOwnerNetUpdate() const;
	bool RebuildSystemOverrideStateFromSources();
	float ResolveTimedSystemOverrideDurationSeconds(float RequestedDurationSeconds) const;
	void SetTimedSystemOverrideClearTimer(FName SourceId, float DurationSeconds);
	void SetTimedSystemOverrideSlotClearTimer(FName SourceId, EARPlayerSlot Slot, float DurationSeconds);
	void ClearTimedSystemOverrideTimer(FName SourceId);
	void ClearTimedSystemOverrideSlotTimer(FName SourceId, EARPlayerSlot Slot);
	void ClearAllTimedSystemOverrideTimersForSource(FName SourceId);
	UFUNCTION()
	void HandleTimedSystemOverrideClear(FName SourceId);
	UFUNCTION()
	void HandleTimedSystemOverrideSlotClear(FName SourceId, EARPlayerSlot Slot);
#if WITH_EDITOR
	void RefreshEditorPreviewBillboard();
	void DestroyEditorPreviewBillboard();
#endif
	USceneComponent* ResolveExplicitAnchorComponent(const AActor* OwnerActor) const;

	UPROPERTY(Replicated)
	FGameplayTag RegisteredSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", UseComponentPicker, AllowAnyActor = "true", AllowedClasses = "/Script/Engine.SceneComponent", ToolTip = "Optional explicit scene-component anchor. This is the primary authoring path for draggable anchor placement."))
	FComponentReference AnchorComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "Optional actor anchor used when AnchorComponent is unset. Uses the actor root-component transform when available."))
	TObjectPtr<AActor> AnchorActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", ToolTip = "World-space offset from the resolved anchor (component, actor root, or owner-bounds fallback)."))
	FVector AnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", ToolTip = "Desired icon size for HUD and editor preview rendering."))
	float IconScreenSize = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", Categories = "Dialogue", ToolTip = "Editor/runtime preview emotion tag used when no active replicated emotion state is present."))
	FGameplayTag PreviewEmotionTag;

	UPROPERTY(ReplicatedUsing = OnRep_BaseEmotionState, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "Base Emotion State", ToolTip = "Replicated base emotion state. Dialogue override state can temporarily supersede this."))
	FAREmotionDisplayState BaseEmotionState;

	UPROPERTY(ReplicatedUsing = OnRep_DialogueOverrideState, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "Dialogue Override State", ToolTip = "Replicated dialogue-scoped override state (higher priority than base emotion state)."))
	FAREmotionDisplayState DialogueOverrideState;

	UPROPERTY(ReplicatedUsing = OnRep_SystemOverrideState, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Emotion", meta = (AllowPrivateAccess = "true", DisplayName = "System Override State", ToolTip = "Replicated top-priority runtime override state resolved from active system sources."))
	FAREmotionDisplayState SystemOverrideState;

	TMap<FName, FSystemEmotionSourceState> SystemEmotionSources;
	uint64 NextSystemEmotionWriteSerial = 1;
	TMap<FName, FTimerHandle> TimedSystemOverrideClearHandles;
	TMap<FName, FTimerHandle> TimedSystemOverrideSlotClearHandles;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<class UBillboardComponent> EditorPreviewBillboardComponent = nullptr;
#endif
};
