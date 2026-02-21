#include "ARPlayerStateBase.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "ARAttributeSetCore.h"

void AARPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARPlayerStateBase, LoadoutTags);
}

void AARPlayerStateBase::OnRep_Loadout()
{
	// Optional: notify UI, fire delegate, etc.
}
AARPlayerStateBase::AARPlayerStateBase()
{
	// PlayerState replicates by default; still safe to be explicit
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Good default for players; you can revisit later
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
}

UAbilitySystemComponent* AARPlayerStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}