#include "ARShopAIController.h"

#include "ARCustomerComponent.h"
#include "ParleyDialogueSubsystem.h"
#include "ARNPCCharacterBase.h"
#include "ARShopStateTreeAIComponent.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"

AARShopAIController::AARShopAIController()
{
	StateTreeComponent = CreateDefaultSubobject<UARShopStateTreeAIComponent>(TEXT("ShopStateTreeComponent"));
	if (StateTreeComponent)
	{
		StateTreeComponent->SetStartLogicAutomatically(false);
	}
}

void AARShopAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (StateTreeComponent)
	{
		StateTreeComponent->OnActiveStateTagsChanged.RemoveAll(this);
		StateTreeComponent->OnActiveStateTagsChanged.AddUObject(this, &AARShopAIController::HandleShopActiveStateTagsChanged);
	}

	if (AARNPCCharacterBase* SpeakerPawn = Cast<AARNPCCharacterBase>(InPawn))
	{
		SpeakerPawn->OnSpeakerTalkableStateChanged.RemoveDynamic(this, &AARShopAIController::HandleSpeakerTalkableStateChanged);
		SpeakerPawn->OnSpeakerTalkableStateChanged.AddDynamic(this, &AARShopAIController::HandleSpeakerTalkableStateChanged);
	}

	BindDialogueSubsystemDelegates();
	TryStartStateTreeForCurrentPawn();
	RefreshSpeakerDialogueGateFromStateTags();
	RefreshSpeakerDialogueSessionState(/*bEmitEvents=*/ false);
}

void AARShopAIController::OnUnPossess()
{
	AARNPCCharacterBase* SpeakerPawn = Cast<AARNPCCharacterBase>(GetPawn());
	if (HasAuthority() && SpeakerPawn)
	{
		// Ensure dialogue gate is restored when this AI controller detaches from the speaker pawn.
		SpeakerPawn->SetSpeakerLocalStateAllowsDialogue(true);
		SpeakerPawn->OnSpeakerTalkableStateChanged.RemoveDynamic(this, &AARShopAIController::HandleSpeakerTalkableStateChanged);
	}

	UnbindDialogueSubsystemDelegates();
	bWasSpeakerInDialogueSession = false;

	if (StateTreeComponent)
	{
		StateTreeComponent->OnActiveStateTagsChanged.RemoveAll(this);
	}

	if (StateTreeComponent && StateTreeComponent->IsRunning())
	{
		StateTreeComponent->StopLogic(TEXT("Shop speaker pawn unpossessed"));
	}

	Super::OnUnPossess();
}

void AARShopAIController::TryStartStateTreeForCurrentPawn()
{
	if (!HasAuthority() || !StateTreeComponent || !GetPawn())
	{
		return;
	}

	if (StateTreeComponent->IsRunning())
	{
		return;
	}

	if (DefaultStateTree)
	{
		StateTreeComponent->SetStateTree(DefaultStateTree);
	}

	StateTreeComponent->StartLogic();
	RefreshSpeakerDialogueGateFromStateTags();
}

bool AARShopAIController::SendShopStateTreeEventByTag(const FGameplayTag EventTag, const FName Origin)
{
	if (!HasAuthority() || !EventTag.IsValid() || !StateTreeComponent || !StateTreeComponent->IsRunning())
	{
		return false;
	}

	const FStateTreeEvent Event(EventTag, FConstStructView(), Origin);
	StateTreeComponent->SendStateTreeEvent(Event);
	return true;
}

void AARShopAIController::HandleSpeakerTalkableStateChanged(const bool bNewTalkable)
{
	if (!HasAuthority() || !bNewTalkable)
	{
		return;
	}

	const FGameplayTag ConversationOfferedEvent = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Event.ShopNPC.ConversationOffered")), false);
	if (ConversationOfferedEvent.IsValid())
	{
		SendShopStateTreeEventByTag(ConversationOfferedEvent, TEXT("SpeakerTalkable"));
	}
}

void AARShopAIController::HandleDialogueSessionUpdated(const FDialogueClientView& View)
{
	(void)View;
	RefreshSpeakerDialogueSessionState(/*bEmitEvents=*/ true);
}

void AARShopAIController::HandleDialogueSessionEnded(const FString& SessionId)
{
	(void)SessionId;
	RefreshSpeakerDialogueSessionState(/*bEmitEvents=*/ true);
}

void AARShopAIController::HandleConversationCompleted(const FGameplayTag ConversationTag)
{
	if (!HasAuthority() || !BoundDialogueSubsystem || !ConversationTag.IsValid())
	{
		return;
	}

	AARNPCCharacterBase* SpeakerPawn = Cast<AARNPCCharacterBase>(GetPawn());
	if (!SpeakerPawn)
	{
		return;
	}

	FGameplayTag PawnSpeakerTag = SpeakerPawn->GetSpeakerTag();
	if (!PawnSpeakerTag.IsValid())
	{
		if (const UARCustomerComponent* CustomerComponent = SpeakerPawn->GetCustomerComponent())
		{
			PawnSpeakerTag = CustomerComponent->GetSpeakerTag();
		}
	}
	if (!PawnSpeakerTag.IsValid())
	{
		return;
	}

	FGameplayTag ConversationPrimarySpeakerTag;
	if (!BoundDialogueSubsystem->GetPrimarySpeakerForConversation(ConversationTag, ConversationPrimarySpeakerTag))
	{
		return;
	}
	if (!ConversationPrimarySpeakerTag.MatchesTagExact(PawnSpeakerTag))
	{
		return;
	}

	const FGameplayTag ConversationCompletedEvent = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Event.ShopNPC.ConversationCompleted")), false);
	if (ConversationCompletedEvent.IsValid())
	{
		SendShopStateTreeEventByTag(ConversationCompletedEvent, TEXT("DialogueConversationCompleted"));
	}
}

void AARShopAIController::HandleShopActiveStateTagsChanged(const FGameplayTagContainer& AddedTags, const FGameplayTagContainer& RemovedTags)
{
	(void)AddedTags;
	(void)RemovedTags;
	RefreshSpeakerDialogueGateFromStateTags();
}

void AARShopAIController::RefreshSpeakerDialogueGateFromStateTags()
{
	if (!HasAuthority() || !StateTreeComponent)
	{
		return;
	}

	AARNPCCharacterBase* SpeakerPawn = Cast<AARNPCCharacterBase>(GetPawn());
	if (!SpeakerPawn)
	{
		return;
	}

	const FGameplayTagContainer ActiveTags = StateTreeComponent->GetCurrentActiveStateTags();
	const FGameplayTag ShopStateRootTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("State.ShopNPC")), false);
	bool bAllowsDialogue = true;
	if (ShopStateRootTag.IsValid() && ActiveTags.HasTag(ShopStateRootTag))
	{
		const FGameplayTag DialogueWindowTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("State.ShopNPC.DialogueWindow")), false);
		bAllowsDialogue = DialogueWindowTag.IsValid() && ActiveTags.HasTagExact(DialogueWindowTag);
	}

	SpeakerPawn->SetSpeakerLocalStateAllowsDialogue(bAllowsDialogue);
}

void AARShopAIController::RefreshSpeakerDialogueSessionState(const bool bEmitEvents)
{
	if (!HasAuthority() || !BoundDialogueSubsystem)
	{
		return;
	}

	AARNPCCharacterBase* SpeakerPawn = Cast<AARNPCCharacterBase>(GetPawn());
	if (!SpeakerPawn)
	{
		return;
	}

	FGameplayTag PawnSpeakerTag = SpeakerPawn->GetSpeakerTag();
	if (!PawnSpeakerTag.IsValid())
	{
		if (const UARCustomerComponent* CustomerComponent = SpeakerPawn->GetCustomerComponent())
		{
			PawnSpeakerTag = CustomerComponent->GetSpeakerTag();
		}
	}
	if (!PawnSpeakerTag.IsValid())
	{
		return;
	}

	const bool bIsInDialogueSession = BoundDialogueSubsystem->IsPrimarySpeakerInActiveSession(PawnSpeakerTag);
	if (bIsInDialogueSession == bWasSpeakerInDialogueSession)
	{
		return;
	}

	bWasSpeakerInDialogueSession = bIsInDialogueSession;
	if (!bEmitEvents)
	{
		return;
	}

	const FGameplayTag SessionEvent = UGameplayTagsManager::Get().RequestGameplayTag(
		bIsInDialogueSession ? FName(TEXT("Event.ShopNPC.DialogueStarted")) : FName(TEXT("Event.ShopNPC.DialogueEnded")),
		false);
	if (SessionEvent.IsValid())
	{
		SendShopStateTreeEventByTag(SessionEvent, bIsInDialogueSession ? TEXT("DialogueSessionStarted") : TEXT("DialogueSessionEnded"));
	}
}

void AARShopAIController::BindDialogueSubsystemDelegates()
{
	UnbindDialogueSubsystemDelegates();

	if (!HasAuthority())
	{
		return;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	BoundDialogueSubsystem = GI ? GI->GetSubsystem<UParleyDialogueSubsystem>() : nullptr;
	if (!BoundDialogueSubsystem)
	{
		return;
	}

	BoundDialogueSubsystem->OnDialogueSessionUpdated.RemoveDynamic(this, &AARShopAIController::HandleDialogueSessionUpdated);
	BoundDialogueSubsystem->OnDialogueSessionUpdated.AddDynamic(this, &AARShopAIController::HandleDialogueSessionUpdated);
	BoundDialogueSubsystem->OnDialogueSessionEnded.RemoveDynamic(this, &AARShopAIController::HandleDialogueSessionEnded);
	BoundDialogueSubsystem->OnDialogueSessionEnded.AddDynamic(this, &AARShopAIController::HandleDialogueSessionEnded);
	BoundDialogueSubsystem->OnConversationCompleted.RemoveDynamic(this, &AARShopAIController::HandleConversationCompleted);
	BoundDialogueSubsystem->OnConversationCompleted.AddDynamic(this, &AARShopAIController::HandleConversationCompleted);
}

void AARShopAIController::UnbindDialogueSubsystemDelegates()
{
	if (!BoundDialogueSubsystem)
	{
		return;
	}

	BoundDialogueSubsystem->OnDialogueSessionUpdated.RemoveDynamic(this, &AARShopAIController::HandleDialogueSessionUpdated);
	BoundDialogueSubsystem->OnDialogueSessionEnded.RemoveDynamic(this, &AARShopAIController::HandleDialogueSessionEnded);
	BoundDialogueSubsystem->OnConversationCompleted.RemoveDynamic(this, &AARShopAIController::HandleConversationCompleted);
	BoundDialogueSubsystem = nullptr;
}
