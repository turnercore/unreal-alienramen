/**
 * @file ParleyDialogueWidgetBase.h
 * @brief Reusable dialogue UI bridge widget base for Parley.
 */
#pragma once

#include "CoreMinimal.h"
#include "ParleyDialogueTypes.h"
#include "Blueprint/UserWidget.h"
#include "ParleyDialogueWidgetBase.generated.h"

class APlayerController;
class AActor;
class UParleyDialogueSubsystem;
class UParleySpeakerSubsystem;
class UParleyFactionSubsystem;

UCLASS(Abstract, Blueprintable)
class PARLEY_API UParleyDialogueWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds this widget to a specific player controller for dialogue updates and input forwarding. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Binds this widget to a player controller and primes the cached dialogue view."))
	void InitializeDialogueWidget(APlayerController* InOwningController);

	/** Unbinds the current controller and clears local dialogue state cache. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Unbinds the current controller and clears cached dialogue state."))
	void DeinitializeDialogueWidget();

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests an advance input through the bound controller."))
	void AdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests a choice submission through the bound controller."))
	void SubmitChoice(FGuid ChoiceBranchId);

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests eavesdrop state for a target character through the bound controller."))
	void SetEavesdrop(bool bEnable, FGameplayTag TargetCharacterTag);

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests eavesdrop state for the other local player through the bound controller."))
	void SetEavesdropOtherPlayer(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Requests a dialogue start for the specified speaker tag through the bound controller."))
	void StartDialogueWithSpeakerTag(FGameplayTag SpeakerTag);

	UFUNCTION(BlueprintCallable, Category = "Parley|Interaction|UI", meta = (ToolTip = "Forwards interaction input for the specified actor through the bound controller."))
	void InteractWithCharacter(AActor* CharacterActor);

	// Toggles local player's dialogue auto-advance preference through the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Toggles the bound controller's dialogue auto-advance preference."))
	void ToggleAutoAdvance();

	// Submits selected choice when waiting for choice; otherwise advances dialogue.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Advances dialogue or submits the active choice, depending on session state."))
	void AdvanceOrSubmitDialogue();

	// Moves selected choice index by Delta on the bound controller.
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Moves the active dialogue choice selection by the supplied delta."))
	void ChoiceDelta(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Returns cached dialogue widget state without mutating runtime data."))
	bool GetCurrentDialogueView(FDialogueClientView& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Returns cached dialogue widget state without mutating runtime data."))
	bool HasActiveDialogueView() const { return bHasActiveDialogueView; }

	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Returns cached dialogue widget state without mutating runtime data."))
	APlayerController* GetBoundController() const { return BoundController; }

	/** Blueprint hook fired after controller binding is established. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired after controller binding is established."))
	void BP_OnDialogueWidgetInitialized(APlayerController* InOwningController);

	/** Blueprint hook fired when the widget sees the first active dialogue view for the bound controller. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired when a dialogue session becomes active for the bound controller."))
	void BP_OnDialogueSessionStarted(const FDialogueClientView& View);

	/** Blueprint hook fired when the cached dialogue view changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired when the cached dialogue view changes."))
	void BP_OnDialogueViewUpdated(const FDialogueClientView& View);

	/** Blueprint hook fired when the active dialogue session ends. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired when the active dialogue session ends."))
	void BP_OnDialogueSessionEnded(const FString& SessionId);

	/** Blueprint hook fired after controller binding is removed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Blueprint hook fired after controller binding is removed."))
	void BP_OnDialogueWidgetDeinitialized();

	/** Blueprint hook fired when a dialogue conversation starts. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when a dialogue conversation starts."))
	void BP_OnDialogueConversationStarted(FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag);

	/** Blueprint hook fired when a dialogue conversation ends. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when a dialogue conversation ends."))
	void BP_OnDialogueConversationEnded(FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag, bool bCompleted);

	/** Blueprint hook fired when a dialogue line is delivered. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when a dialogue line is delivered."))
	void BP_OnDialogueLineDelivered(FGameplayTag SpeakerTag, FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag);

	/** Blueprint hook fired when an important choice is selected. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when an important dialogue choice is selected."))
	void BP_OnDialogueImportantChoiceMade(FGuid ChoiceBranchId, FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag);

	/** Blueprint hook fired when a speaker relationship bucket changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when a speaker relationship level changes."))
	void BP_OnDialogueSpeakerRelationshipLevelChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, int32 OldLevel, int32 NewLevel, float NewTotal);

	/** Blueprint hook fired when a conversation completes for an owning character. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when a conversation completes for an owning character."))
	void BP_OnDialogueConversationCompleted(FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag, FGameplayTag CharacterTag);

	/** Blueprint hook fired when directed speaker relationship points change. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when directed speaker relationship points change."))
	void BP_OnDialogueSpeakerRelationshipChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, float Delta, float NewTotal);

	/** Blueprint hook fired when dialogue progression tags mutate for a character owner. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when dialogue progression tags mutate for a character owner."))
	void BP_OnDialogueProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag OwnerCharacterTag);

	/** Blueprint hook fired when dialogue progression data should be persisted. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when dialogue progression data mutates and should be persisted."))
	void BP_OnDialogueProgressionStateMarkedDirty();

	/** Blueprint hook fired when choice lookahead resolves a preview emotion. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when choice lookahead resolves a preview emotion."))
	void BP_OnDialogueChoiceLookaheadEmotion(FGameplayTag PrimarySpeakerTag, FGameplayTag PreviewEmotionTag, FGuid ChoiceBranchId);

	/** Blueprint hook fired when choice lookahead is cleared. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when choice lookahead is cleared."))
	void BP_OnDialogueChoiceLookaheadCleared(FGameplayTag OwnerCharacterTag);

	/** Blueprint hook fired when a Signal node executes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Signals", meta = (ToolTip = "Blueprint hook fired when a dialogue Signal node executes."))
	void BP_OnDialogueSignalFired(FGameplayTag SignalTag, FGameplayTagContainer PayloadTags, FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag);

	/** Blueprint hook fired when line audio resolves into a request payload. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Audio", meta = (ToolTip = "Blueprint hook fired when dialogue line audio resolves into a request payload."))
	void BP_OnDialogueAudioRequested(const FDialogueAudioRequest& Request);

	/** Blueprint hook fired when a speaker's talkable state changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Speaker", meta = (ToolTip = "Blueprint hook fired when a speaker's talkable state changes."))
	void BP_OnSpeakerTalkableChanged(FGameplayTag SpeakerTag, bool bNewTalkable);

	/** Blueprint hook fired when faction popularity changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Faction", meta = (ToolTip = "Blueprint hook fired when faction popularity changes."))
	void BP_OnFactionPopularityChanged(FGameplayTag FactionTag, float Delta, float NewTotal);

	/** Blueprint hook fired when faction reputation changes for a speaker. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parley|Dialogue|UI|Faction", meta = (ToolTip = "Blueprint hook fired when faction reputation changes for a speaker."))
	void BP_OnFactionSpeakerReputationChanged(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float Delta, float NewTotal);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Auto-bind to owning AR player controller on construct.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Configures automatic dialogue widget binding and visibility behavior."))
	bool bAutoBindOwningPlayerControllerOnConstruct = true;

	// Auto-toggle visibility from dialogue session state (Visible on update, Collapsed on end/deinit).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (ToolTip = "Configures automatic dialogue widget binding and visibility behavior."))
	bool bAutoToggleVisibilityFromSessionState = true;

private:
	UFUNCTION()
	void HandleControllerDialogueViewUpdated(const FDialogueClientView& View);

	UFUNCTION()
	void HandleControllerDialogueSessionEnded(const FString& SessionId);

	UFUNCTION()
	void HandleDialogueConversationStarted(FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleDialogueConversationEnded(FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag, bool bCompleted);

	UFUNCTION()
	void HandleDialogueLineDelivered(FGameplayTag SpeakerTag, FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleDialogueImportantChoiceMade(FGuid ChoiceBranchId, FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleDialogueSpeakerRelationshipLevelChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, int32 OldLevel, int32 NewLevel, float NewTotal);

	UFUNCTION()
	void HandleDialogueConversationCompleted(FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag, FGameplayTag CharacterTag);

	UFUNCTION()
	void HandleDialogueSpeakerRelationshipChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, float Delta, float NewTotal);

	UFUNCTION()
	void HandleDialogueProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleDialogueProgressionStateMarkedDirty();

	UFUNCTION()
	void HandleDialogueChoiceLookaheadEmotion(FGameplayTag PrimarySpeakerTag, FGameplayTag PreviewEmotionTag, FGuid ChoiceBranchId);

	UFUNCTION()
	void HandleDialogueChoiceLookaheadCleared(FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleDialogueSignalFired(FGameplayTag SignalTag, FGameplayTagContainer PayloadTags, FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleDialogueAudioRequested(const FDialogueAudioRequest& Request);

	UFUNCTION()
	void HandleSpeakerTalkableChanged(FGameplayTag SpeakerTag, bool bNewTalkable);

	UFUNCTION()
	void HandleFactionPopularityChanged(FGameplayTag FactionTag, float Delta, float NewTotal);

	UFUNCTION()
	void HandleFactionSpeakerReputationChanged(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float Delta, float NewTotal);

	void BindParleySubsystemDelegates();
	void UnbindParleySubsystemDelegates();
	void PushInitialViewFromController();
	void ClearCachedDialogueView(bool bCollapseVisibility);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	TObjectPtr<APlayerController> BoundController = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	FDialogueClientView CurrentDialogueView;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Parley|Dialogue|UI", meta = (AllowPrivateAccess = "true", ToolTip = "Runtime dialogue widget state cached for Blueprint UI reads."))
	bool bHasActiveDialogueView = false;

	TWeakObjectPtr<UParleyDialogueSubsystem> BoundDialogueSubsystem;
	TWeakObjectPtr<UParleySpeakerSubsystem> BoundSpeakerSubsystem;
	TWeakObjectPtr<UParleyFactionSubsystem> BoundFactionSubsystem;
};
