// Fill out your copyright notice in the Description page of Project Settings.


#include "AREnemyBase.h"
#include "AREnemyAIController.h"
#include "ARLog.h"

#include "AbilitySystemComponent.h"
#include "ARAbilitySet.h"
#include "ARAttributeSetCore.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace AREnemyBaseInternal
{
	static void GrantAbilitySet(
		UAbilitySystemComponent* ASC,
		const UARAbilitySet* Set,
		TArray<FGameplayAbilitySpecHandle>& OutGranted,
		TArray<FActiveGameplayEffectHandle>& OutApplied
	)
	{
		if (!ASC || !Set)
		{
			return;
		}

		for (const FARAbilitySet_AbilityEntry& Entry : Set->Abilities)
		{
			if (!Entry.Ability)
			{
				continue;
			}

			FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level);
			if (Entry.ActivationTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(Entry.ActivationTag);
			}

			OutGranted.Add(ASC->GiveAbility(Spec));
		}

		for (const FARAbilitySet_EffectEntry& Entry : Set->StartupEffects)
		{
			if (!Entry.Effect)
			{
				continue;
			}

			const FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Entry.Effect, Entry.Level, Ctx);
			if (Spec.IsValid())
			{
				OutApplied.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
			}
		}
	}
}

AAREnemyBase::AAREnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAREnemyAIController::StaticClass();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.f, 640.f, 0.f);
		MoveComp->GravityScale = 0.f;
	}

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
}

UAbilitySystemComponent* AAREnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAREnemyBase::BeginPlay()
{
	Super::BeginPlay();
	BindHealthChangeDelegate();
}

void AAREnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHealthChangeDelegate();
	Super::EndPlay(EndPlayReason);
}

void AAREnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAREnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();

	if (HasAuthority())
	{
		ApplyStartupAbilitySet();
	}

	TryDispatchWavePhaseEvent();
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
	BP_OnEnemyInitialized();
	UE_LOG(ARLog, Log, TEXT("[EnemyBase] Possessed '%s' by '%s'."),
		*GetNameSafe(this), *GetNameSafe(NewController));
}

void AAREnemyBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitAbilityActorInfo();
}

void AAREnemyBase::UnPossessed()
{
	if (HasAuthority())
	{
		ClearStartupAbilitySet();
	}

	Super::UnPossessed();
}

void AAREnemyBase::InitAbilityActorInfo()
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(ARLog, Error, TEXT("[EnemyBase] InitAbilityActorInfo failed: no AbilitySystemComponent on '%s'."), *GetNameSafe(this));
		return;
	}

	// Enemy-owned ASC: owner and avatar are this pawn.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	BindHealthChangeDelegate();
	UE_LOG(ARLog, Log, TEXT("[EnemyBase] ASC initialized for '%s'."), *GetNameSafe(this));
}

void AAREnemyBase::ApplyStartupAbilitySet()
{
	if (!HasAuthority() || !AbilitySystemComponent || !StartupAbilitySet || bStartupSetApplied)
	{
		return;
	}

	AREnemyBaseInternal::GrantAbilitySet(AbilitySystemComponent, StartupAbilitySet, StartupGrantedAbilityHandles, StartupAppliedEffectHandles);
	bStartupSetApplied = true;

	UE_LOG(ARLog, Log, TEXT("[EnemyBase] Applied startup ability set '%s' to '%s' (Abilities=%d Effects=%d)."),
		*GetNameSafe(StartupAbilitySet), *GetNameSafe(this), StartupGrantedAbilityHandles.Num(), StartupAppliedEffectHandles.Num());
}

void AAREnemyBase::ClearStartupAbilitySet()
{
	if (!AbilitySystemComponent || !bStartupSetApplied)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : StartupGrantedAbilityHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : StartupAppliedEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}

	StartupGrantedAbilityHandles.Reset();
	StartupAppliedEffectHandles.Reset();
	bStartupSetApplied = false;
}

void AAREnemyBase::BindHealthChangeDelegate()
{
	if (!AbilitySystemComponent || HealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	HealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute())
		.AddUObject(this, &AAREnemyBase::OnHealthChanged);
}

void AAREnemyBase::UnbindHealthChangeDelegate()
{
	if (!AbilitySystemComponent || !HealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute())
		.Remove(HealthChangedDelegateHandle);
	HealthChangedDelegateHandle.Reset();
}

void AAREnemyBase::OnHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	if (ChangeData.NewValue <= 0.f)
	{
		AActor* DamageInstigator = nullptr;
		if (ChangeData.GEModData)
		{
			DamageInstigator = ChangeData.GEModData->EffectSpec.GetContext().GetOriginalInstigator();
		}

		HandleDeath(DamageInstigator);
	}
}

void AAREnemyBase::HandleDeath_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	ForceNetUpdate();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	if (AController* PawnController = GetController())
	{
		PawnController->StopMovement();
		if (AAIController* AIController = Cast<AAIController>(PawnController))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Enemy died"));
			}
		}
	}

	UE_LOG(ARLog, Log, TEXT("[EnemyBase] Death handled for '%s'. Instigator='%s'."),
		*GetNameSafe(this), *GetNameSafe(InstigatorActor));

	BP_OnEnemyDied(InstigatorActor);
}

void AAREnemyBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		BP_OnEnemyDied(nullptr);
	}
}

void AAREnemyBase::SetWaveRuntimeContext(
	int32 InWaveInstanceId,
	int32 InFormationSlotIndex,
	EARWavePhase InWavePhase,
	float InPhaseStartServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	const EARWavePhase PreviousPhase = WavePhase;
	const int32 PreviousWaveInstanceId = WaveInstanceId;
	WaveInstanceId = InWaveInstanceId;
	FormationSlotIndex = InFormationSlotIndex;
	WavePhase = InWavePhase;
	WavePhaseStartServerTime = InPhaseStartServerTime;
	bHasEnteredGameplayScreen = false;
	bReachedFormationSlot = false;
	bHasDispatchedEnteredScreenEvent = false;
	bHasDispatchedInFormationEvent = false;
	LastDispatchedWavePhaseWaveId = INDEX_NONE;
	ForceNetUpdate();

	if (PreviousPhase != WavePhase || PreviousWaveInstanceId != WaveInstanceId)
	{
		TryDispatchWavePhaseEvent();
	}
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
}

void AAREnemyBase::SetWavePhase(EARWavePhase InWavePhase, float InPhaseStartServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	WavePhase = InWavePhase;
	WavePhaseStartServerTime = InPhaseStartServerTime;
	ForceNetUpdate();
	TryDispatchWavePhaseEvent();
}

void AAREnemyBase::NotifyEnteredGameplayScreen(float InServerTime)
{
	if (!HasAuthority() || bHasEnteredGameplayScreen)
	{
		return;
	}

	bHasEnteredGameplayScreen = true;
	EnteredGameplayScreenServerTime = InServerTime;
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
}

void AAREnemyBase::SetFormationLockRules(bool bInFormationLockEnter, bool bInFormationLockActive)
{
	if (!HasAuthority())
	{
		return;
	}

	bFormationLockEnter = bInFormationLockEnter;
	bFormationLockActive = bInFormationLockActive;
	TryDispatchEnteredScreenEvent();
	TryDispatchInFormationEvent();
	ForceNetUpdate();
}

void AAREnemyBase::SetReachedFormationSlot(bool bInReachedFormationSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	bReachedFormationSlot = bInReachedFormationSlot;
	TryDispatchInFormationEvent();
}

bool AAREnemyBase::CheckAndMarkLeaked(float LeakBoundaryX)
{
	if (!HasAuthority() || bCountedAsLeak || bIsDead)
	{
		return false;
	}

	// In this coordinate layout, leak means crossing player-side low-X boundary.
	if (GetActorLocation().X >= LeakBoundaryX)
	{
		return false;
	}

	bCountedAsLeak = true;
	SignalEnemyLeaked.Broadcast(this);
	BP_OnEnemyLeaked();
	return true;
}

void AAREnemyBase::TryDispatchWavePhaseEvent()
{
	if (!HasAuthority())
	{
		return;
	}

	if (LastDispatchedWavePhaseWaveId == WaveInstanceId && LastDispatchedWavePhase == WavePhase)
	{
		return;
	}

	LastDispatchedWavePhaseWaveId = WaveInstanceId;
	LastDispatchedWavePhase = WavePhase;
	BP_OnWavePhaseChanged(WavePhase);
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->NotifyWavePhaseChanged(WaveInstanceId, WavePhase);
	}
}

void AAREnemyBase::TryDispatchEnteredScreenEvent()
{
	if (!HasAuthority() || bHasDispatchedEnteredScreenEvent)
	{
		return;
	}

	if (!bHasEnteredGameplayScreen)
	{
		return;
	}

	bHasDispatchedEnteredScreenEvent = true;
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->NotifyEnemyEnteredScreen(WaveInstanceId);
	}
}

void AAREnemyBase::TryDispatchInFormationEvent()
{
	if (!HasAuthority() || bHasDispatchedInFormationEvent)
	{
		return;
	}

	// InFormation means "formation slot reached while on screen".
	if (!bReachedFormationSlot || !bHasEnteredGameplayScreen)
	{
		return;
	}

	bHasDispatchedInFormationEvent = true;
	if (AAREnemyAIController* EnemyAI = Cast<AAREnemyAIController>(GetController()))
	{
		EnemyAI->NotifyEnemyInFormation(WaveInstanceId);
	}
}

bool AAREnemyBase::CanFireByWaveRules() const
{
	if (!bHasEnteredGameplayScreen)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	return (Now - EnteredGameplayScreenServerTime) >= 1.0f;
}

void AAREnemyBase::OnRep_WaveRuntimeContext()
{
	BP_OnWavePhaseChanged(WavePhase);
}

bool AAREnemyBase::ActivateAbilityByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] ActivateAbilityByTag ignored on non-authority for '%s'."), *GetNameSafe(this));
		return false;
	}

	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return false;
	}

	if (!CanFireByWaveRules())
	{
		return false;
	}

	FGameplayTagContainer QueryTags;
	QueryTags.AddTag(AbilityTag);
	return AbilitySystemComponent->TryActivateAbilitiesByTag(QueryTags, bAllowPartialMatch);
}

void AAREnemyBase::CancelAbilitiesByTag(FGameplayTag AbilityTag, bool bAllowPartialMatch)
{
	(void)bAllowPartialMatch;

	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[EnemyBase] CancelAbilitiesByTag ignored on non-authority for '%s'."), *GetNameSafe(this));
		return;
	}

	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer WithTags;
	WithTags.AddTag(AbilityTag);
	AbilitySystemComponent->CancelAbilities(&WithTags, nullptr);
}

void AAREnemyBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAREnemyBase, EnemyColor);
	DOREPLIFETIME(AAREnemyBase, EnemyArchetypeTag);
	DOREPLIFETIME(AAREnemyBase, bIsDead);
	DOREPLIFETIME(AAREnemyBase, WaveInstanceId);
	DOREPLIFETIME(AAREnemyBase, FormationSlotIndex);
	DOREPLIFETIME(AAREnemyBase, WavePhase);
	DOREPLIFETIME(AAREnemyBase, WavePhaseStartServerTime);
	DOREPLIFETIME(AAREnemyBase, bFormationLockEnter);
	DOREPLIFETIME(AAREnemyBase, bFormationLockActive);
}

void AAREnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

