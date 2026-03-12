#include "ARShopAIController.h"

#include "ARNPCCharacterBase.h"
#include "ARShopStateTreeAIComponent.h"
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

	TryStartStateTreeForCurrentPawn();
	RefreshSpeakerDialogueGateFromStateTags();
}

void AARShopAIController::OnUnPossess()
{
	AARNPCCharacterBase* SpeakerPawn = Cast<AARNPCCharacterBase>(GetPawn());
	if (HasAuthority() && SpeakerPawn)
	{
		// Ensure dialogue gate is restored when this AI controller detaches from the speaker pawn.
		SpeakerPawn->SetSpeakerLocalStateAllowsDialogue(true);
	}

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
