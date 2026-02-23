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

bool AARPlayerStateBase::ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	if (!SavedState.IsValid())
	{
		return false;
	}

	if (!HasAuthority())
	{
		ServerApplyStateFromStruct(SavedState);
		return true;
	}

	return IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARPlayerStateBase::ServerApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}
