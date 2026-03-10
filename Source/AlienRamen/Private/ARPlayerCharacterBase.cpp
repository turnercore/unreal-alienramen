#include "ARPlayerCharacterBase.h"

#include "AREmotionComponent.h"

AARPlayerCharacterBase::AARPlayerCharacterBase()
{
	bReplicates = true;
	EmotionComponent = CreateDefaultSubobject<UAREmotionComponent>(TEXT("EmotionComponent"));
}

UAbilitySystemComponent* AARPlayerCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}
