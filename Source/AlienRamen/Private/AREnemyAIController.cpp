#include "AREnemyAIController.h"
#include "ARLog.h"
#include "AREnemyBase.h"
#include "ARStateTreeAIComponent.h"

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

	StateTreeComponent = CreateDefaultSubobject<UARStateTreeAIComponent>(TEXT("StateTreeComponent"));
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
	UnbindStateTreeTagBridge(/*bPopAppliedTags=*/false);
	BindStateTreeTagBridge();
	DeferredStartAttemptCounter = 0;

	// Enforce context-driven startup even if BP defaults accidentally re-enable auto-start.
	if (StateTreeComponent)
	{
		StateTreeComponent->SetStartLogicAutomatically(false);
		if (StateTreeComponent->IsRunning())
		{
			StateTreeComponent->StopLogic(TEXT("Waiting for wave runtime context"));
		}
	}
	else
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI] OnPossess '%s': missing StateTreeComponent."), *GetNameSafe(this));
	}

	if (!DefaultStateTree)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] OnPossess '%s': DefaultStateTree is not assigned."), *GetNameSafe(this));
	}
}

void AAREnemyAIController::OnUnPossess()
{
	UnbindStateTreeTagBridge(/*bPopAppliedTags=*/true);
	StopStateTree(TEXT("Enemy unpossessed"));
	ClearFocus(EAIFocusPriority::Gameplay);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredStartTimerHandle);
	}
	bPendingStateTreeStart = false;
	DeferredStartAttemptCounter = 0;

	Super::OnUnPossess();
}

void AAREnemyAIController::TryStartStateTreeForCurrentPawn()
{
	if (!GetPawn())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] TryStartStateTreeForCurrentPawn '%s': no possessed pawn."), *GetNameSafe(this));
	}

	StartStateTreeForPawn(GetPawn());
}

void AAREnemyAIController::StartStateTreeForPawn(APawn* InPawn)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] StartStateTreeForPawn '%s': skipped on non-authority."), *GetNameSafe(this));
		return;
	}

	if (!InPawn)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] StartStateTreeForPawn '%s': null pawn input."), *GetNameSafe(this));
		return;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI] Missing StateTreeComponent on '%s'."), *GetNameSafe(this));
		return;
	}

	// Don't start until director has applied wave runtime context.
	if (const AAREnemyBase* Enemy = Cast<AAREnemyBase>(InPawn))
	{
		if (Enemy->GetWaveInstanceId() == INDEX_NONE)
		{
			if (DeferredStartAttemptCounter == 0 || (DeferredStartAttemptCounter % 30) == 0)
			{
				UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Start deferred '%s' Pawn='%s': WaveInstanceId not set yet (attempt=%d)."),
					*GetNameSafe(this), *GetNameSafe(InPawn), DeferredStartAttemptCounter + 1);
			}
			StartStateTreeForPawn_Deferred(InPawn);
			return;
		}
	}

	// Possess/startup ordering can invoke this more than once; keep startup idempotent and controller-owned.
	if (GetPawn() != InPawn)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Start deferred '%s': input pawn '%s' is not current pawn '%s'."),
			*GetNameSafe(this), *GetNameSafe(InPawn), *GetNameSafe(GetPawn()));
		StartStateTreeForPawn_Deferred(InPawn);
		return;
	}

	if (InPawn->GetController() != this)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Start deferred '%s': pawn '%s' controller is '%s', expected this."),
			*GetNameSafe(this), *GetNameSafe(InPawn), *GetNameSafe(InPawn->GetController()));
		StartStateTreeForPawn_Deferred(InPawn);
		return;
	}

	if (IsStateTreeRunning())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Start skipped '%s': StateTree already running for pawn '%s'."),
			*GetNameSafe(this), *GetNameSafe(InPawn));
		StartStateTreeForPawn_Deferred(InPawn);
		return;
	}

	StartStateTreeForPawn_Deferred(InPawn);
}

void AAREnemyAIController::StartStateTreeForPawn_Deferred(APawn* InPawn)
{
	if (bPendingStateTreeStart)
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Deferred start already pending for '%s'."), *GetNameSafe(this));
		return;
	}

	bPendingStateTreeStart = true;
	++DeferredStartAttemptCounter;

	if (UWorld* World = GetWorld())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Queue deferred start for '%s' (attempt=%d pawn='%s')."),
			*GetNameSafe(this), DeferredStartAttemptCounter, *GetNameSafe(InPawn));

		TWeakObjectPtr<AAREnemyAIController> WeakThis(this);
		TWeakObjectPtr<APawn> WeakPawn(InPawn);
		DeferredStartTimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakPawn]()
		{
			AAREnemyAIController* StrongThis = WeakThis.Get();
			APawn* StrongPawn = WeakPawn.Get();
			if (!StrongThis)
			{
				return;
			}

			StrongThis->bPendingStateTreeStart = false;

			if (!StrongPawn || StrongThis->GetPawn() != StrongPawn || StrongPawn->GetController() != StrongThis)
			{
				UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Deferred start aborted '%s' (attempt=%d): pawn/controller relationship changed. CurrentPawn='%s' DeferredPawn='%s' PawnController='%s'."),
					*GetNameSafe(StrongThis),
					StrongThis->DeferredStartAttemptCounter,
					*GetNameSafe(StrongThis->GetPawn()),
					*GetNameSafe(StrongPawn),
					*GetNameSafe(StrongPawn ? StrongPawn->GetController() : nullptr));
				return;
			}

			if (!StrongThis->StateTreeComponent)
			{
				UE_LOG(ARLog, Error, TEXT("[EnemyAI] Missing StateTreeComponent on '%s' during deferred start."), *GetNameSafe(StrongThis));
				return;
			}

			if (const AAREnemyBase* Enemy = Cast<AAREnemyBase>(StrongPawn))
			{
				if (Enemy->GetWaveInstanceId() == INDEX_NONE)
				{
					if ((StrongThis->DeferredStartAttemptCounter % 30) == 0)
					{
						UE_LOG(ARLog, Error, TEXT("[EnemyAI] Deferred start still waiting on wave context '%s' after %d attempts (pawn='%s')."),
							*GetNameSafe(StrongThis),
							StrongThis->DeferredStartAttemptCounter,
							*GetNameSafe(StrongPawn));
					}
					StrongThis->StartStateTreeForPawn(StrongPawn);
					return;
				}

				UE_LOG(
					ARLog,
					Log,
					TEXT("[EnemyAI|StartCtx] Controller='%s' Pawn='%s' WaveId=%d LockEnter=%d LockActive=%d"),
					*GetNameSafe(StrongThis),
					*GetNameSafe(StrongPawn),
					Enemy->GetWaveInstanceId(),
					Enemy->GetFormationLockEnter() ? 1 : 0,
					Enemy->GetFormationLockActive() ? 1 : 0);
			}

			if (!StrongThis->DefaultStateTree && !StrongThis->IsStateTreeRunning())
			{
				UE_LOG(ARLog, Error, TEXT("[EnemyAI] Deferred start failed '%s': DefaultStateTree is null (attempt=%d pawn='%s')."),
					*GetNameSafe(StrongThis),
					StrongThis->DeferredStartAttemptCounter,
					*GetNameSafe(StrongPawn));
				return;
			}

			if (StrongThis->DefaultStateTree && !StrongThis->IsStateTreeRunning())
			{
				StrongThis->StateTreeComponent->SetStateTree(StrongThis->DefaultStateTree);
			}

			StrongThis->StateTreeComponent->StartLogic();
			if (!StrongThis->IsStateTreeRunning())
			{
				UE_LOG(ARLog, Error, TEXT("[EnemyAI] Deferred start failed '%s': StartLogic did not transition to running state (attempt=%d pawn='%s' stateTree='%s')."),
					*GetNameSafe(StrongThis),
					StrongThis->DeferredStartAttemptCounter,
					*GetNameSafe(StrongPawn),
					*GetNameSafe(StrongThis->DefaultStateTree));
				return;
			}

			UE_LOG(ARLog, Log, TEXT("[EnemyAI] Started StateTree for controller '%s' on pawn '%s' (attempt=%d)."),
				*GetNameSafe(StrongThis), *GetNameSafe(StrongPawn), StrongThis->DeferredStartAttemptCounter);
			StrongThis->DeferredStartAttemptCounter = 0;

			if (AAREnemyBase* Enemy = Cast<AAREnemyBase>(StrongPawn))
			{
				StrongThis->NotifyWavePhaseChanged(Enemy->GetWaveInstanceId(), Enemy->GetWavePhase());
				if (Enemy->HasEnteredGameplayScreen())
				{
					StrongThis->NotifyEnemyEnteredScreen(Enemy->GetWaveInstanceId());
				}
				if (Enemy->HasReachedFormationSlot())
				{
					StrongThis->NotifyEnemyInFormation(Enemy->GetWaveInstanceId());
				}
			}
		}));
	}
	else
	{
		bPendingStateTreeStart = false;
		UE_LOG(ARLog, Error, TEXT("[EnemyAI] Deferred start failed '%s': no valid world for timer scheduling."), *GetNameSafe(this));
	}
}

bool AAREnemyAIController::IsStateTreeRunning() const
{
	return StateTreeComponent && StateTreeComponent->IsRunning();
}

void AAREnemyAIController::BindStateTreeTagBridge()
{
	if (!StateTreeComponent)
	{
		return;
	}

	StateTreeComponent->OnActiveStateTagsChanged.RemoveAll(this);
	StateTreeComponent->OnActiveStateTagsChanged.AddUObject(this, &AAREnemyAIController::HandleStateTreeActiveTagsChanged);

	const FGameplayTagContainer ExistingTags = StateTreeComponent->GetCurrentActiveStateTags();
	if (!ExistingTags.IsEmpty())
	{
		HandleStateTreeActiveTagsChanged(ExistingTags, FGameplayTagContainer());
	}
}

void AAREnemyAIController::UnbindStateTreeTagBridge(bool bPopAppliedTags)
{
	if (StateTreeComponent)
	{
		StateTreeComponent->OnActiveStateTagsChanged.RemoveAll(this);
	}

	if (bPopAppliedTags && !AppliedStateTreeTags.IsEmpty())
	{
		PopPawnASCStateTags(AppliedStateTreeTags);
	}

	AppliedStateTreeTags.Reset();
}

void AAREnemyAIController::HandleStateTreeActiveTagsChanged(const FGameplayTagContainer& AddedTags, const FGameplayTagContainer& RemovedTags)
{
	if (!HasAuthority())
	{
		return;
	}

	FGameplayTagContainer EffectiveRemovedTags;
	for (const FGameplayTag Tag : RemovedTags)
	{
		if (!Tag.IsValid())
		{
			continue;
		}

		EffectiveRemovedTags.AddTag(Tag);
		AppliedStateTreeTags.RemoveTag(Tag);
	}

	if (!EffectiveRemovedTags.IsEmpty())
	{
		PopPawnASCStateTags(EffectiveRemovedTags);
	}

	FGameplayTagContainer EffectiveAddedTags;
	for (const FGameplayTag Tag : AddedTags)
	{
		if (!Tag.IsValid() || AppliedStateTreeTags.HasTagExact(Tag))
		{
			continue;
		}

		EffectiveAddedTags.AddTag(Tag);
		AppliedStateTreeTags.AddTag(Tag);
	}

	if (!EffectiveAddedTags.IsEmpty())
	{
		PushPawnASCStateTags(EffectiveAddedTags);
	}
}

void AAREnemyAIController::PushPawnASCStateTag(FGameplayTag StateTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAREnemyBase* EnemyPawn = Cast<AAREnemyBase>(GetPawn()))
	{
		EnemyPawn->PushASCStateTag(StateTag);
	}
}

void AAREnemyAIController::PopPawnASCStateTag(FGameplayTag StateTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAREnemyBase* EnemyPawn = Cast<AAREnemyBase>(GetPawn()))
	{
		EnemyPawn->PopASCStateTag(StateTag);
	}
}

void AAREnemyAIController::PushPawnASCStateTags(const FGameplayTagContainer& StateTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAREnemyBase* EnemyPawn = Cast<AAREnemyBase>(GetPawn()))
	{
		EnemyPawn->PushASCStateTags(StateTags);
	}
}

void AAREnemyAIController::PopPawnASCStateTags(const FGameplayTagContainer& StateTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAREnemyBase* EnemyPawn = Cast<AAREnemyBase>(GetPawn()))
	{
		EnemyPawn->PopASCStateTags(StateTags);
	}
}

bool AAREnemyAIController::SendStateTreeEvent(const FStateTreeEvent& Event)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Dropped StateTree event on non-authority controller '%s' (Event=%s)."),
			*GetNameSafe(this), *Event.Tag.ToString());
		return false;
	}

	if (!Event.Tag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Dropped StateTree event on '%s': invalid event tag."), *GetNameSafe(this));
		return false;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyAI] Dropped StateTree event on '%s': missing StateTreeComponent (Event=%s)."),
			*GetNameSafe(this), *Event.Tag.ToString());
		return false;
	}

	if (!IsStateTreeRunning())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Dropped StateTree event on '%s': StateTree not running (Event=%s)."),
			*GetNameSafe(this), *Event.Tag.ToString());
		return false;
	}

	StateTreeComponent->SendStateTreeEvent(Event);
	return true;
}

bool AAREnemyAIController::SendStateTreeEventByTag(FGameplayTag EventTag, FName Origin)
{
	if (!EventTag.IsValid())
	{
		return false;
	}

	const FStateTreeEvent Event(EventTag, FConstStructView(), Origin);
	return SendStateTreeEvent(Event);
}

bool AAREnemyAIController::ReceivePawnSignal(
	FGameplayTag SignalTag,
	AActor* RelatedActor,
	FVector WorldLocation,
	float ScalarValue,
	bool bForwardToStateTree)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Ignored pawn signal on non-authority controller '%s' (Tag=%s)."),
			*GetNameSafe(this), *SignalTag.ToString());
		return false;
	}

	if (!SignalTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Ignored pawn signal on '%s': invalid signal tag."), *GetNameSafe(this));
		return false;
	}

	// Controller remains the decision owner; pawn only reports facts.
	BP_OnPawnSignal(SignalTag, RelatedActor, WorldLocation, ScalarValue);

	if (bForwardToStateTree)
	{
		const FName Origin = RelatedActor ? FName(*GetNameSafe(RelatedActor)) : NAME_None;
		return SendStateTreeEventByTag(SignalTag, Origin);
	}

	return true;
}

void AAREnemyAIController::StopStateTree(const FString& Reason)
{
	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] StopStateTree ignored on '%s': missing StateTreeComponent."), *GetNameSafe(this));
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
	if (!HasAuthority())
	{
		return;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Wave phase event skipped on '%s': missing StateTreeComponent."), *GetNameSafe(this));
		return;
	}

	if (!IsStateTreeRunning())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Wave phase event skipped on '%s': StateTree not running (WaveId=%d Phase=%s)."),
			*GetNameSafe(this), WaveInstanceId, AREnemyAIControllerInternal::ToPhaseName(NewPhase));
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
	if (!HasAuthority())
	{
		return;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Entered-screen event skipped on '%s': missing StateTreeComponent."), *GetNameSafe(this));
		return;
	}

	if (!IsStateTreeRunning())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] Entered-screen event skipped on '%s': StateTree not running (WaveId=%d)."),
			*GetNameSafe(this), WaveInstanceId);
		return;
	}

	if (!EnteredScreenEventTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] Entered-screen event skipped on '%s': EnteredScreenEventTag is invalid."), *GetNameSafe(this));
		return;
	}

	const FStateTreeEvent Event(EnteredScreenEventTag, FConstStructView(), FName(*FString::Printf(TEXT("Wave%d"), WaveInstanceId)));
	StateTreeComponent->SendStateTreeEvent(Event);
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Sent entered-screen event for WaveId=%d on '%s' (Event=%s)."),
		WaveInstanceId, *GetNameSafe(this), *EnteredScreenEventTag.ToString());
}

void AAREnemyAIController::NotifyEnemyInFormation(int32 WaveInstanceId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!StateTreeComponent)
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] In-formation event skipped on '%s': missing StateTreeComponent."), *GetNameSafe(this));
		return;
	}

	if (!IsStateTreeRunning())
	{
		UE_LOG(ARLog, Verbose, TEXT("[EnemyAI] In-formation event skipped on '%s': StateTree not running (WaveId=%d)."),
			*GetNameSafe(this), WaveInstanceId);
		return;
	}

	if (!InFormationEventTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyAI] In-formation event skipped on '%s': InFormationEventTag is invalid."), *GetNameSafe(this));
		return;
	}

	const FStateTreeEvent Event(InFormationEventTag, FConstStructView(), FName(*FString::Printf(TEXT("Wave%d"), WaveInstanceId)));
	StateTreeComponent->SendStateTreeEvent(Event);
	UE_LOG(ARLog, Log, TEXT("[EnemyAI] Sent in-formation event for WaveId=%d on '%s' (Event=%s)."),
		WaveInstanceId, *GetNameSafe(this), *InFormationEventTag.ToString());
}
