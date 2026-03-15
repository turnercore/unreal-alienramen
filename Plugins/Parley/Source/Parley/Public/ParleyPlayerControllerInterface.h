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
	virtual FGameplayTag GetPlayerSlotTag() const = 0;
	virtual bool IsDialogueAutoAdvanceEnabled() const = 0;
	virtual FGameplayTag GetCharacterTag() const = 0;
	virtual void NotifyDialogueViewUpdated(const FDialogueClientView& View) = 0;
	virtual void NotifyDialogueSessionEnded(const FString& SessionId) = 0;
	virtual void NotifyDialogueAudioRequested(const FDialogueAudioRequest& Request) = 0;
	virtual void RequestInteractWithActor(AActor* Actor) = 0;
	virtual void RequestStartDialogueBySpeakerTag(const FGameplayTag& SpeakerTag) = 0;
	virtual void RequestAdvanceDialogueInput() = 0;
	virtual void RequestSubmitDialogueChoiceInput(FGuid ChoiceBranchId) = 0;
	virtual void RequestSetDialogueEavesdropInput(bool bEnable, FGameplayTag TargetSlotTag) = 0;
	virtual void RequestSetDialogueEavesdropOtherPlayerInput(bool bEnable) = 0;
	virtual void RequestToggleDialogueAutoAdvanceInput() = 0;
	virtual void RequestAdvanceOrSubmitDialogueInput() = 0;
	virtual void RequestDialogueChoiceDeltaInput(int32 Delta) = 0;
	virtual bool QueryLocalDialogueView(FDialogueClientView& OutView) const = 0;
	virtual bool GetCachedDialogueView(FDialogueClientView& OutView) const = 0;
};
