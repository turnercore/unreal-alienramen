// Fill out your copyright notice in the Description page of Project Settings.


#include "AREnemyBase.h"

#include "AbilitySystemComponent.h"
#include "ARAttributeSetCore.h"

AAREnemyBase::AAREnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
}

UAbilitySystemComponent* AAREnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAREnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAREnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();
}

void AAREnemyBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitAbilityActorInfo();
}

void AAREnemyBase::UnPossessed()
{
	Super::UnPossessed();
}

void AAREnemyBase::InitAbilityActorInfo()
{
	if (!AbilitySystemComponent) return;

	// Enemy-owned ASC: owner and avatar are this pawn.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void AAREnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

