#include "AREnemyAIController.h"
#include "ARLog.h"

#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace AREnemyAIControllerInternal
{
	static const TCHAR* ToPhaseName(EARWavePhase Phase)
	{
		switch (Phase)
		{
		case EARWavePhase::Active: return TEXT("Active");
		case EARWavePhase::Berserk: return TEXT("Berserk");
		default: return TEXT("Unknown");
		}
	}
}

AAREnemyAIController::AAREnemyAIController()
{
	bStartAILogicOnPossess = false;

	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	if (StateTreeComponent)
	{
		StateTreeComponent->SetStartLogicAutomatically(false);
	}

	ActivePhaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Wave.Phase.Active")), false);
	BerserkPhaseEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Wave.Phase.Berserk")), false);
	EnteredScreenEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Enemy.EnteredScreen")), false);
	InFormationEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Enemy.InFormation")), false);
}

void AAREnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<AAREnemyAIController> WeakThis(this);
		TWeakObjectPtr<APawn> WeakPawn(InPawn);
		World->GetTimerManager().SetTimerForNextTick([WeakThis, WeakPawn]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			WeakThis->StartStateTreeForPawn(WeakPawn.Get());
		});
	}
	else
	{
		StartStateTreeForPawn(InPawn);
	}
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

	if (!InPawn)
	{
		return;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI] Missing StateTreeComponent on '%s'."), *GetNameSafe(this));
		return;
	}

	// Possess/startup ordering can invoke this more than once; keep startup idempotent.
	if (GetPawn() != InPawn || IsStateTreeRunning())
	{
		return;
	}

	if (DefaultStateTree && !IsStateTreeRunning())
	{
		StateTreeComponent->SetStateTree(DefaultStateTree);
	}

	StateTreeComponent->StartLogic();
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Started StateTree for controller '%s' on pawn '%s'."),
		*GetNameSafe(this), *GetNameSafe(InPawn));
}

bool AAREnemyAIController::IsStateTreeRunning() const
{
	return StateTreeComponent && StateTreeComponent->IsRunning();
}

void AAREnemyAIController::StopStateTree(const FString& Reason)
{
	if (!StateTreeComponent)
	{
		return;
	}

	StateTreeComponent->StopLogic(Reason);
	if (!Reason.Equals(TEXT("Enemy unpossessed"), ESearchCase::IgnoreCase))
	{
		UE_LOG(ARLog, Log, TEXT("[EnemyAI] Stopped StateTree for '%s'. Reason: %s"), *GetNameSafe(this), *Reason);
	}
}

void AAREnemyAIController::NotifyWavePhaseChanged(int32 WaveInstanceId, EARWavePhase NewPhase)
{
	if (!HasAuthority() || !StateTreeComponent || !IsStateTreeRunning())
	{
		return;
	}

	FGameplayTag EventTag;
	switch (NewPhase)
	{
	case EARWavePhase::Active: EventTag = ActivePhaseEventTag; break;
	case EARWavePhase::Berserk: EventTag = BerserkPhaseEventTag; break;
	default: break;
	}

	if (!EventTag.IsValid())
	{
		return;
	}

	const FStateTreeEvent Event(EventTag, FConstStructView(), FName(*FString::Printf(TEXT("Wave%d"), WaveInstanceId)));
	StateTreeComponent->SendStateTreeEvent(Event);
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Entered wave phase '%s' for WaveId=%d on '%s' (Event=%s)."),
		AREnemyAIControllerInternal::ToPhaseName(NewPhase), WaveInstanceId, *GetNameSafe(this), *EventTag.ToString());
}

void AAREnemyAIController::NotifyEnemyEnteredScreen(int32 WaveInstanceId)
{
	if (!HasAuthority() || !StateTreeComponent || !IsStateTreeRunning() || !EnteredScreenEventTag.IsValid())
	{
		return;
	}

	const FStateTreeEvent Event(EnteredScreenEventTag, FConstStructView(), FName(*FString::Printf(TEXT("Wave%d"), WaveInstanceId)));
	StateTreeComponent->SendStateTreeEvent(Event);
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Sent entered-screen event for WaveId=%d on '%s' (Event=%s)."),
		WaveInstanceId, *GetNameSafe(this), *EnteredScreenEventTag.ToString());
}

void AAREnemyAIController::NotifyEnemyInFormation(int32 WaveInstanceId)
{
	if (!HasAuthority() || !StateTreeComponent || !IsStateTreeRunning() || !InFormationEventTag.IsValid())
	{
		return;
	}

	const FStateTreeEvent Event(InFormationEventTag, FConstStructView(), FName(*FString::Printf(TEXT("Wave%d"), WaveInstanceId)));
	StateTreeComponent->SendStateTreeEvent(Event);
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Sent in-formation event for WaveId=%d on '%s' (Event=%s)."),
		WaveInstanceId, *GetNameSafe(this), *InFormationEventTag.ToString());
}
