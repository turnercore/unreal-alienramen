#include "AREnemyAIController.h"
#include "ARLog.h"

#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"

AAREnemyAIController::AAREnemyAIController()
{
	bStartAILogicOnPossess = false;

	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	if (StateTreeComponent)
	{
		StateTreeComponent->SetStartLogicAutomatically(false);
	}

	EnteringPhaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Wave.Phase.Entering")), false);
	ActivePhaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Wave.Phase.Active")), false);
	BerserkPhaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Wave.Phase.Berserk")), false);
	ExpiredPhaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Wave.Phase.Expired")), false);
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

void AAREnemyAIController::NotifyWavePhaseChanged(int32 WaveInstanceId, EARWavePhase NewPhase)
{
	if (!HasAuthority() || !StateTreeComponent)
	{
		return;
	}

	FGameplayTag EventTag;
	switch (NewPhase)
	{
	case EARWavePhase::Entering: EventTag = EnteringPhaseEventTag; break;
	case EARWavePhase::Active: EventTag = ActivePhaseEventTag; break;
	case EARWavePhase::Berserk: EventTag = BerserkPhaseEventTag; break;
	case EARWavePhase::Expired: EventTag = ExpiredPhaseEventTag; break;
	default: break;
	}

	if (!EventTag.IsValid())
	{
		return;
	}

	const FStateTreeEvent Event(EventTag, FConstStructView(), FName(*FString::Printf(TEXT("Wave%d"), WaveInstanceId)));
	StateTreeComponent->SendStateTreeEvent(Event);
	UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Sent StateTree wave phase event %s for WaveId=%d on '%s'."),
		*EventTag.ToString(), WaveInstanceId, *GetNameSafe(this));
}
