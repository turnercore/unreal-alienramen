#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "ParleyDialogueTypes.h"
#include "ParleyPlayerControllerInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI)
class UParleyPlayerControllerInterface : public UInterface
{
	GENERATED_BODY()
};

class PARLEY_API IParleyPlayerControllerInterface
{
	GENERATED_BODY()

public:
	virtual bool IsDialogueAutoAdvanceEnabled() const = 0;
	virtual FGameplayTag GetCharacterTag() const = 0;
	virtual void NotifyDialogueViewUpdated(const FDialogueClientView& View) = 0;
	virtual void NotifyDialogueSessionEnded(const FString& SessionId) = 0;
	virtual void NotifyDialogueAudioRequested(const FDialogueAudioRequest& Request) = 0;
	// Runtime events that should reach HUD widgets through the bound controller's local subsystem instance.
	virtual void NotifyDialogueConversationStarted(FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag) = 0;
	virtual void NotifyDialogueConversationEnded(FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag, bool bCompleted) = 0;
	virtual void NotifyDialogueLineDelivered(FGameplayTag SpeakerTag, FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag) = 0;
	virtual void NotifyDialogueImportantChoiceMade(FGuid ChoiceBranchId, FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag) = 0;
	virtual void NotifyDialogueSpeakerRelationshipLevelChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, int32 OldLevel, int32 NewLevel, float NewTotal) = 0;
	virtual void NotifyDialogueConversationCompleted(FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag, FGameplayTag CharacterTag) = 0;
	virtual void NotifyDialogueSpeakerRelationshipChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, float Delta, float NewTotal) = 0;
	virtual void NotifyDialogueProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag OwnerCharacterTag) = 0;
	virtual void NotifyDialogueProgressionStateMarkedDirty() = 0;
	virtual void NotifyDialogueChoiceLookaheadEmotion(FGameplayTag PrimarySpeakerTag, FGameplayTag PreviewEmotionTag, FGuid ChoiceBranchId) = 0;
	virtual void NotifyDialogueChoiceLookaheadCleared(FGameplayTag OwnerCharacterTag) = 0;
	virtual void NotifyDialogueSignalFired(FGameplayTag SignalTag, FGameplayTagContainer PayloadTags, FGameplayTag ConversationTag, FGameplayTag SpeakerTag, FGameplayTag OwnerCharacterTag) = 0;
	virtual void NotifySpeakerTalkableChanged(FGameplayTag SpeakerTag, bool bNewTalkable) = 0;
	virtual void NotifyFactionPopularityChanged(FGameplayTag FactionTag, float Delta, float NewTotal) = 0;
	virtual void NotifyFactionSpeakerReputationChanged(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float Delta, float NewTotal) = 0;
	virtual void RequestInteractWithActor(AActor* Actor) = 0;
	virtual void RequestStartDialogueBySpeakerTag(const FGameplayTag& SpeakerTag) = 0;
	virtual void RequestAdvanceDialogueInput() = 0;
	virtual void RequestSubmitDialogueChoiceInput(FGuid ChoiceBranchId) = 0;
	virtual void RequestSetDialogueEavesdropInput(bool bEnable, FGameplayTag TargetCharacterTag) = 0;
	virtual void RequestSetDialogueEavesdropOtherPlayerInput(bool bEnable) = 0;
	virtual void RequestToggleDialogueAutoAdvanceInput() = 0;
	virtual void RequestAdvanceOrSubmitDialogueInput() = 0;
	virtual void RequestDialogueChoiceDeltaInput(int32 Delta) = 0;
	virtual bool QueryLocalDialogueView(FDialogueClientView& OutView) const = 0;
	virtual bool GetCachedDialogueView(FDialogueClientView& OutView) const = 0;
};
