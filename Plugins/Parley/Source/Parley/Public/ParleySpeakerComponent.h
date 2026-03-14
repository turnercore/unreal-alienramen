/**
 * @file ParleySpeakerComponent.h
 * @brief Reusable dialogue speaker/talkability component.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ParleySpeakerComponent.generated.h"

class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnSpeakerTalkableStateChanged, bool, bNewTalkable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnSpeakerEmotionRequested, FGameplayTag, EmotionTag, FGameplayTag, PlayerSlotTag, bool, bIsDialogueLine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParleyOnSpeakerEmotionCleared, FGameplayTag, PlayerSlotTag);

UCLASS(
	ClassGroup=(AlienRamen),
	BlueprintType,
	Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Dialogue Speaker Component", ToolTip="Server-authoritative talkability and interaction surface for a dialogue speaker."))
class PARLEY_API UParleySpeakerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UParleySpeakerComponent();

	/** Primary interaction entrypoint: routes to dialogue subsystem using this speaker tag and controller. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Runs a speaker component operation that routes through Parley runtime systems."))
	void InteractByController(APlayerController* InteractingController);

	/** Speaker identity tag used for dialogue lookups (GameplayTag). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (DisplayName = "Get Speaker Tag", ToolTip = "Returns this speaker component's canonical speaker identity tag."))
	FGameplayTag GetSpeakerTag() const { return SpeakerTag; }

	/** Update the speaker tag at runtime (authority only recommended). Also refreshes talkable state. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|Speaker", meta = (DisplayName = "Set Speaker Tag", ToolTip = "Sets this speaker component's canonical speaker identity tag and refreshes talkability state."))
	void SetSpeakerTag(FGameplayTag NewSpeakerTag);

	/** True when any player can currently start a conversation with this speaker. */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Returns current speaker component state without mutating runtime data."))
	bool IsTalkable() const { return bIsTalkable; }

	// StateTree-friendly dialogue-only gate. True when this speaker currently has dialogue available for at least one slot.
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Returns current speaker component state without mutating runtime data."))
	bool HasDialogueToSay() const { return bIsTalkable; }

	/** Slot-tag-specific talkable query (for example Player.Slot.P1 / Player.Slot.P2). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Returns current speaker component state without mutating runtime data."))
	bool IsTalkableForPlayerSlotTag(FGameplayTag PlayerSlotTag) const;

	/** Controller-aware talkable query (uses controller player slot mapping). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Returns current speaker component state without mutating runtime data."))
	bool IsTalkableForController(const APlayerController* QueryController) const;

	// StateTree-friendly gate alias for dialogue availability.
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Returns current speaker component state without mutating runtime data."))
	bool HasSomethingToSay() const;

	/** Ask the dialogue subsystem to recompute talkability (use after unlocking content or clearing blockers). */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Runs a speaker component operation that routes through Parley runtime systems."))
	void RefreshTalkableFromSubsystem();

	UPROPERTY(BlueprintAssignable, Category = "Parley|Talk", meta = (ToolTip = "Broadcast when this speaker's talkable state changes."))
	FParleyOnSpeakerTalkableStateChanged OnSpeakerTalkableStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Emotion", meta = (ToolTip = "Broadcast when Parley requests this speaker to display an emotion. Game module should bridge this to Emo component."))
	FParleyOnSpeakerEmotionRequested OnSpeakerEmotionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Emotion", meta = (ToolTip = "Broadcast when Parley requests this speaker emotion override to clear. Game module should bridge this to Emo component."))
	FParleyOnSpeakerEmotionCleared OnSpeakerEmotionCleared;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsTalkable(bool bOldTalkable);

	UFUNCTION()
	void OnRep_TalkablePlayerSlotMask(uint8 bOldTalkablePlayerSlotMask);

	UFUNCTION()
	void HandleSpeakerTalkableChanged(FGameplayTag ChangedSpeakerTag, bool bNewTalkable);

private:
	bool IsAuthorityOwner() const;
	void ForceOwnerNetUpdate() const;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Parley|Talk",
		meta = (AllowPrivateAccess = "true", Categories = "Dialogue.Speaker", DisplayName = "Speaker Tag", ToolTip = "Primary speaker identity tag used for dialogue lookup and speaker-bound emotion routing."))
	FGameplayTag SpeakerTag;

	UPROPERTY(ReplicatedUsing = OnRep_IsTalkable, BlueprintReadOnly, Category = "Parley|Talk", meta = (AllowPrivateAccess = "true", DisplayName = "Speaker Is Talkable", ToolTip = "Resolved global talkability for this speaker from the dialogue runtime."))
	bool bIsTalkable = false;

	// Bitmask of talkable slots (P1=bit0, P2=bit1).
	UPROPERTY(ReplicatedUsing = OnRep_TalkablePlayerSlotMask, BlueprintReadOnly, Category = "Parley|Talk", meta = (AllowPrivateAccess = "true", DisplayName = "Talkable Player Slot Mask", ToolTip = "Per-player talkability mask (P1 bit 0, P2 bit 1)."))
	uint8 TalkablePlayerSlotMask = 0;
};
