#include "AREnemyAIController.h"
#include "ARLog.h"

#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"

AAREnemyAIController::AAREnemyAIController()
{
	bStartAILogicOnPossess = false;

	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	if (StateTreeComponent)
	{
		StateTreeComponent->SetStartLogicAutomatically(false);
	}
}

void AAREnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	StartStateTreeForPawn(InPawn);
}

void AAREnemyAIController::OnUnPossess()
{
	StopStateTree(TEXT("Enemy unpossessed"));
	ClearFocus(EAIFocusPriority::Gameplay);

	Super::OnUnPossess();
}

void AAREnemyAIController::StartStateTreeForPawn(APawn* InPawn)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI] Missing StateTreeComponent on '%s'."), *GetNameSafe(this));
		return;
	}

	if (DefaultStateTree)
	{
		StateTreeComponent->SetStateTree(DefaultStateTree);
	}

	StateTreeComponent->StartLogic();
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Started StateTree for controller '%s' on pawn '%s'."),
		*GetNameSafe(this), *GetNameSafe(InPawn));
}

void AAREnemyAIController::StopStateTree(const FString& Reason)
{
	if (!StateTreeComponent)
	{
		return;
	}

	StateTreeComponent->StopLogic(Reason);
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Stopped StateTree for '%s'. Reason: %s"), *GetNameSafe(this), *Reason);
}

