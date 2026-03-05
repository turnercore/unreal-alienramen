#include "ARPlayerCharacterBase.h"

AARPlayerCharacterBase::AARPlayerCharacterBase()
{
	bReplicates = true;
}

UAbilitySystemComponent* AARPlayerCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}
