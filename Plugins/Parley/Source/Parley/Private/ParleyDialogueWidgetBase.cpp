#include "ParleyDialogueWidgetBase.h"

#include "ParleyDialogueSubsystem.h"
#include "ParleyFactionSubsystem.h"
#include "ParleyPlayerControllerInterface.h"
#include "ParleySpeakerSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

namespace
{
	static IParleyPlayerControllerInterface* ResolveParleyControllerInterface(APlayerController* Controller)
	{
		if (!Controller || !Controller->GetClass()->ImplementsInterface(UParleyPlayerControllerInterface::StaticClass()))
		{
			return nullptr;
		}

		return Cast<IParleyPlayerControllerInterface>(Controller);
	}

	static const IParleyPlayerControllerInterface* ResolveParleyControllerInterface(const APlayerController* Controller)
	{
		if (!Controller || !Controller->GetClass()->ImplementsInterface(UParleyPlayerControllerInterface::StaticClass()))
		{
			return nullptr;
		}

		return Cast<IParleyPlayerControllerInterface>(Controller);
	}
}

void UParleyDialogueWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bAutoBindOwningPlayerControllerOnConstruct && !BoundController)
	{
		InitializeDialogueWidget(GetOwningPlayer());
	}
}

void UParleyDialogueWidgetBase::NativeDestruct()
{
	DeinitializeDialogueWidget();
	Super::NativeDestruct();
}

void UParleyDialogueWidgetBase::InitializeDialogueWidget(APlayerController* InOwningController)
{
	if (BoundController == InOwningController)
	{
		RefreshBoundCharacterTag();
		PushInitialViewFromController();
		return;
	}

	UnbindParleySubsystemDelegates();
	ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
	BoundController = InOwningController;
	BindParleySubsystemDelegates();
	RefreshBoundCharacterTag();
	PushInitialViewFromController();
	BP_OnDialogueWidgetInitialized(BoundController);
}

void UParleyDialogueWidgetBase::DeinitializeDialogueWidget()
{
	UnbindParleySubsystemDelegates();
	BoundController = nullptr;
	BoundCharacterTag = FGameplayTag();
	ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
	BP_OnDialogueWidgetDeinitialized();
}

void UParleyDialogueWidgetBase::AdvanceDialogue()
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestAdvanceDialogueInput();
	}
}

void UParleyDialogueWidgetBase::SubmitChoice(FGuid ChoiceBranchId)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		if (ChoiceBranchId.IsValid())
		{
			ControllerInterface->RequestSubmitDialogueChoiceInput(ChoiceBranchId);
		}
	}
}

void UParleyDialogueWidgetBase::SetEavesdrop(bool bEnable, FGameplayTag TargetCharacterTag)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestSetDialogueEavesdropInput(bEnable, TargetCharacterTag);
	}
}

void UParleyDialogueWidgetBase::SetEavesdropOtherPlayer(bool bEnable)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestSetDialogueEavesdropOtherPlayerInput(bEnable);
	}
}

void UParleyDialogueWidgetBase::StartDialogueWithSpeakerTag(FGameplayTag SpeakerTag)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		if (SpeakerTag.IsValid())
		{
			ControllerInterface->RequestStartDialogueBySpeakerTag(SpeakerTag);
		}
	}
}

void UParleyDialogueWidgetBase::InteractWithCharacter(AActor* CharacterActor)
{
	if (!CharacterActor)
	{
		return;
	}

	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestInteractWithActor(CharacterActor);
	}
}

void UParleyDialogueWidgetBase::ToggleAutoAdvance()
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestToggleDialogueAutoAdvanceInput();
	}
}

void UParleyDialogueWidgetBase::AdvanceOrSubmitDialogue()
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestAdvanceOrSubmitDialogueInput();
	}
}

void UParleyDialogueWidgetBase::ChoiceDelta(const int32 Delta)
{
	if (IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		ControllerInterface->RequestDialogueChoiceDeltaInput(Delta);
	}
}

bool UParleyDialogueWidgetBase::GetCurrentDialogueView(FDialogueClientView& OutView) const
{
	OutView = bHasActiveDialogueView ? CurrentDialogueView : FDialogueClientView();
	return bHasActiveDialogueView;
}

void UParleyDialogueWidgetBase::HandleControllerDialogueViewUpdated(const FDialogueClientView& View)
{
	(void)View;
	if (!IsValid(BoundController))
	{
		DeinitializeDialogueWidget();
		return;
	}

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController);
	if (!ControllerInterface)
	{
		DeinitializeDialogueWidget();
		return;
	}

	FDialogueClientView LocalView;
	const bool bWasActive = bHasActiveDialogueView;
	if (!ControllerInterface->QueryLocalDialogueView(LocalView) && !ControllerInterface->GetCachedDialogueView(LocalView))
	{
		ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
		return;
	}

	CurrentDialogueView = LocalView;
	bHasActiveDialogueView = true;
	if (!bWasActive)
	{
		BP_OnDialogueSessionStarted(LocalView);
	}
	if (bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
	BP_OnDialogueViewUpdated(LocalView);
}

void UParleyDialogueWidgetBase::HandleControllerDialogueSessionEnded(const FString& SessionId)
{
	if (!IsValid(BoundController))
	{
		DeinitializeDialogueWidget();
		return;
	}

	if (!bHasActiveDialogueView || CurrentDialogueView.SessionId != SessionId)
	{
		return;
	}

	ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
	BP_OnDialogueSessionEnded(SessionId);
}

void UParleyDialogueWidgetBase::HandleDialogueConversationStarted(const FGameplayTag ConversationTag, const FGameplayTag SpeakerTag, const FGameplayTag OwnerCharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueConversationStarted(ConversationTag, SpeakerTag, OwnerCharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueConversationEnded(const FGameplayTag ConversationTag, const FGameplayTag SpeakerTag, const FGameplayTag OwnerCharacterTag, const bool bCompleted)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueConversationEnded(ConversationTag, SpeakerTag, OwnerCharacterTag, bCompleted);
}

void UParleyDialogueWidgetBase::HandleDialogueLineDelivered(const FGameplayTag SpeakerTag, const FGameplayTag ConversationTag, const FGameplayTag OwnerCharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueLineDelivered(SpeakerTag, ConversationTag, OwnerCharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueImportantChoiceMade(const FGuid ChoiceBranchId, const FGameplayTag ConversationTag, const FGameplayTag SpeakerTag, const FGameplayTag OwnerCharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueImportantChoiceMade(ChoiceBranchId, ConversationTag, SpeakerTag, OwnerCharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueSpeakerRelationshipLevelChanged(
	const FGameplayTag SourceSpeakerTag,
	const FGameplayTag TargetSpeakerTag,
	const FGameplayTag OwnerCharacterTag,
	const int32 OldLevel,
	const int32 NewLevel,
	const float NewTotal)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueSpeakerRelationshipLevelChanged(SourceSpeakerTag, TargetSpeakerTag, OwnerCharacterTag, OldLevel, NewLevel, NewTotal);
}

void UParleyDialogueWidgetBase::HandleDialogueConversationCompleted(const FGameplayTag ConversationTag, const FGameplayTag OwnerCharacterTag, const FGameplayTag CharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueConversationCompleted(ConversationTag, OwnerCharacterTag, CharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueSpeakerRelationshipChanged(const FGameplayTag SourceSpeakerTag, const FGameplayTag TargetSpeakerTag, const FGameplayTag OwnerCharacterTag, const float Delta, const float NewTotal)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueSpeakerRelationshipChanged(SourceSpeakerTag, TargetSpeakerTag, OwnerCharacterTag, Delta, NewTotal);
}

void UParleyDialogueWidgetBase::HandleDialogueProgressionTagMutated(const FGameplayTag ProgressionTag, const bool bAdded, const FGameplayTag OwnerCharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueProgressionTagMutated(ProgressionTag, bAdded, OwnerCharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueProgressionStateMarkedDirty()
{
	if (!IsValid(BoundController))
	{
		return;
	}

	BP_OnDialogueProgressionStateMarkedDirty();
}

void UParleyDialogueWidgetBase::HandleDialogueChoiceLookaheadEmotion(const FGameplayTag PrimarySpeakerTag, const FGameplayTag PreviewEmotionTag, const FGuid ChoiceBranchId)
{
	if (!IsValid(BoundController))
	{
		return;
	}

	if (!bHasActiveDialogueView || !CurrentDialogueView.SpeakerTag.IsValid() || CurrentDialogueView.SpeakerTag != PrimarySpeakerTag)
	{
		return;
	}

	BP_OnDialogueChoiceLookaheadEmotion(PrimarySpeakerTag, PreviewEmotionTag, ChoiceBranchId);
}

void UParleyDialogueWidgetBase::HandleDialogueChoiceLookaheadCleared(const FGameplayTag OwnerCharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueChoiceLookaheadCleared(OwnerCharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueSignalFired(
	const FGameplayTag SignalTag,
	const FGameplayTagContainer PayloadTags,
	const FGameplayTag ConversationTag,
	const FGameplayTag SpeakerTag,
	const FGameplayTag OwnerCharacterTag)
{
	if (!ShouldHandleOwnerCharacterTag(OwnerCharacterTag))
	{
		return;
	}

	BP_OnDialogueSignalFired(SignalTag, PayloadTags, ConversationTag, SpeakerTag, OwnerCharacterTag);
}

void UParleyDialogueWidgetBase::HandleDialogueAudioRequested(const FDialogueAudioRequest& Request)
{
	if (!ShouldHandleOwnerCharacterTag(Request.ListenerCharacterTag))
	{
		return;
	}

	BP_OnDialogueAudioRequested(Request);
}

void UParleyDialogueWidgetBase::HandleSpeakerTalkableChanged(const FGameplayTag SpeakerTag, const bool bNewTalkable)
{
	if (!IsValid(BoundController))
	{
		return;
	}

	BP_OnSpeakerTalkableChanged(SpeakerTag, bNewTalkable);
}

void UParleyDialogueWidgetBase::HandleFactionPopularityChanged(const FGameplayTag FactionTag, const float Delta, const float NewTotal)
{
	if (!IsValid(BoundController))
	{
		return;
	}

	BP_OnFactionPopularityChanged(FactionTag, Delta, NewTotal);
}

void UParleyDialogueWidgetBase::HandleFactionSpeakerReputationChanged(const FGameplayTag FactionTag, const FGameplayTag SpeakerTag, const float Delta, const float NewTotal)
{
	if (!IsValid(BoundController))
	{
		return;
	}

	BP_OnFactionSpeakerReputationChanged(FactionTag, SpeakerTag, Delta, NewTotal);
}

void UParleyDialogueWidgetBase::SetBoundCharacterTag(const FGameplayTag NewCharacterTag)
{
	if (BoundCharacterTag == NewCharacterTag)
	{
		return;
	}

	const FGameplayTag OldCharacterTag = BoundCharacterTag;
	BoundCharacterTag = NewCharacterTag;
	BP_OnDialogueWidgetBoundCharacterChanged(NewCharacterTag, OldCharacterTag);
}

bool UParleyDialogueWidgetBase::ShouldHandleOwnerCharacterTag(const FGameplayTag OwnerCharacterTag) const
{
	if (!IsValid(BoundController))
	{
		return false;
	}

	if (!OwnerCharacterTag.IsValid())
	{
		return true;
	}

	if (BoundCharacterTag.IsValid())
	{
		if (OwnerCharacterTag == BoundCharacterTag)
		{
			return true;
		}
	}

	if (bHasActiveDialogueView && CurrentDialogueView.OwnerCharacterTag.IsValid() && OwnerCharacterTag == CurrentDialogueView.OwnerCharacterTag)
	{
		return true;
	}

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController);
	if (!ControllerInterface)
	{
		return false;
	}

	const FGameplayTag ControllerCharacterTag = ControllerInterface->GetCharacterTag();
	return ControllerCharacterTag.IsValid() ? OwnerCharacterTag == ControllerCharacterTag : false;
}

void UParleyDialogueWidgetBase::BindParleySubsystemDelegates()
{
	if (!IsValid(BoundController))
	{
		return;
	}

	if (UGameInstance* GameInstance = BoundController->GetGameInstance())
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UParleyDialogueSubsystem>())
		{
			BoundDialogueSubsystem = DialogueSubsystem;
			DialogueSubsystem->OnDialogueSessionUpdated.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueViewUpdated);
			DialogueSubsystem->OnDialogueSessionEnded.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueSessionEnded);
			DialogueSubsystem->OnConversationStarted.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueConversationStarted);
			DialogueSubsystem->OnConversationEnded.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueConversationEnded);
			DialogueSubsystem->OnLineDelivered.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueLineDelivered);
			DialogueSubsystem->OnImportantChoiceMade.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueImportantChoiceMade);
			DialogueSubsystem->OnSpeakerRelationshipLevelChanged.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueSpeakerRelationshipLevelChanged);
			DialogueSubsystem->OnParleyConversationCompleted.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueConversationCompleted);
			DialogueSubsystem->OnSpeakerRelationshipChanged.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueSpeakerRelationshipChanged);
			DialogueSubsystem->OnProgressionTagMutated.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueProgressionTagMutated);
			DialogueSubsystem->OnProgressionStateMarkedDirty.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueProgressionStateMarkedDirty);
			DialogueSubsystem->OnChoiceLookaheadEmotion.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueChoiceLookaheadEmotion);
			DialogueSubsystem->OnChoiceLookaheadCleared.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueChoiceLookaheadCleared);
			DialogueSubsystem->OnDialogueSignalFired.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueSignalFired);
			DialogueSubsystem->OnDialogueAudioRequested.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueAudioRequested);
		}

		if (UParleySpeakerSubsystem* SpeakerSubsystem = GameInstance->GetSubsystem<UParleySpeakerSubsystem>())
		{
			BoundSpeakerSubsystem = SpeakerSubsystem;
			SpeakerSubsystem->OnSpeakerTalkableChanged.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleSpeakerTalkableChanged);
		}

		if (UParleyFactionSubsystem* FactionSubsystem = GameInstance->GetSubsystem<UParleyFactionSubsystem>())
		{
			BoundFactionSubsystem = FactionSubsystem;
			FactionSubsystem->OnFactionPopularityChanged.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleFactionPopularityChanged);
			FactionSubsystem->OnFactionSpeakerReputationChanged.AddUniqueDynamic(this, &UParleyDialogueWidgetBase::HandleFactionSpeakerReputationChanged);
		}
	}
}

void UParleyDialogueWidgetBase::RefreshBoundCharacterTag()
{
	if (!IsValid(BoundController))
	{
		SetBoundCharacterTag(FGameplayTag());
		return;
	}

	FGameplayTag NewBoundCharacterTag;
	if (const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController))
	{
		NewBoundCharacterTag = ControllerInterface->GetCharacterTag();
	}

	SetBoundCharacterTag(NewBoundCharacterTag);
}

void UParleyDialogueWidgetBase::UnbindParleySubsystemDelegates()
{
	if (UParleyDialogueSubsystem* DialogueSubsystem = BoundDialogueSubsystem.Get())
	{
		DialogueSubsystem->OnDialogueSessionUpdated.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueViewUpdated);
		DialogueSubsystem->OnDialogueSessionEnded.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleControllerDialogueSessionEnded);
		DialogueSubsystem->OnConversationStarted.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueConversationStarted);
		DialogueSubsystem->OnConversationEnded.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueConversationEnded);
		DialogueSubsystem->OnLineDelivered.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueLineDelivered);
		DialogueSubsystem->OnImportantChoiceMade.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueImportantChoiceMade);
		DialogueSubsystem->OnSpeakerRelationshipLevelChanged.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueSpeakerRelationshipLevelChanged);
		DialogueSubsystem->OnParleyConversationCompleted.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueConversationCompleted);
		DialogueSubsystem->OnSpeakerRelationshipChanged.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueSpeakerRelationshipChanged);
		DialogueSubsystem->OnProgressionTagMutated.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueProgressionTagMutated);
		DialogueSubsystem->OnProgressionStateMarkedDirty.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueProgressionStateMarkedDirty);
		DialogueSubsystem->OnChoiceLookaheadEmotion.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueChoiceLookaheadEmotion);
		DialogueSubsystem->OnChoiceLookaheadCleared.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueChoiceLookaheadCleared);
		DialogueSubsystem->OnDialogueSignalFired.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueSignalFired);
		DialogueSubsystem->OnDialogueAudioRequested.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleDialogueAudioRequested);
	}

	if (UParleySpeakerSubsystem* SpeakerSubsystem = BoundSpeakerSubsystem.Get())
	{
		SpeakerSubsystem->OnSpeakerTalkableChanged.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleSpeakerTalkableChanged);
	}

	if (UParleyFactionSubsystem* FactionSubsystem = BoundFactionSubsystem.Get())
	{
		FactionSubsystem->OnFactionPopularityChanged.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleFactionPopularityChanged);
		FactionSubsystem->OnFactionSpeakerReputationChanged.RemoveDynamic(this, &UParleyDialogueWidgetBase::HandleFactionSpeakerReputationChanged);
	}

	BoundDialogueSubsystem = nullptr;
	BoundSpeakerSubsystem = nullptr;
	BoundFactionSubsystem = nullptr;
}

void UParleyDialogueWidgetBase::ClearCachedDialogueView(const bool bCollapseVisibility)
{
	CurrentDialogueView = FDialogueClientView();
	bHasActiveDialogueView = false;
	if (bCollapseVisibility && bAutoToggleVisibilityFromSessionState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UParleyDialogueWidgetBase::PushInitialViewFromController()
{
	if (!IsValid(BoundController))
	{
		ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
		return;
	}

	RefreshBoundCharacterTag();

	const IParleyPlayerControllerInterface* ControllerInterface = ResolveParleyControllerInterface(BoundController);
	if (!ControllerInterface)
	{
		ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
		return;
	}

	FDialogueClientView CurrentView;
	if (ControllerInterface->QueryLocalDialogueView(CurrentView) || ControllerInterface->GetCachedDialogueView(CurrentView))
	{
		HandleControllerDialogueViewUpdated(CurrentView);
		return;
	}

	ClearCachedDialogueView(/*bCollapseVisibility=*/ true);
}
