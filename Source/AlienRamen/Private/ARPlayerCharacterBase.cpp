#include "ARPlayerCharacterBase.h"

#include "EmoComponent.h"

AARPlayerCharacterBase::AARPlayerCharacterBase()
{
	bReplicates = true;
	EmotionComponent = CreateDefaultSubobject<UEmoComponent>(TEXT("EmotionComponent"));
}

UAbilitySystemComponent* AARPlayerCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}
