#include "ARPlayerCharacterBase.h"

#include "EmoComponent.h"
#include "ARPlayerStateBase.h"
#include "ARPlayerTypes.h"
#include "ParleySpeakerComponent.h"

AARPlayerCharacterBase::AARPlayerCharacterBase()
{
	bReplicates = true;
	EmotionComponent = CreateDefaultSubobject<UEmoComponent>(TEXT("EmotionComponent"));
	ParleySpeakerComponent = CreateDefaultSubobject<UParleySpeakerComponent>(TEXT("ParleySpeakerComponent"));
}

UAbilitySystemComponent* AARPlayerCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

void AARPlayerCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshParleySpeakerFromPlayerState();
}

void AARPlayerCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshParleySpeakerFromPlayerState();
}

void AARPlayerCharacterBase::RefreshParleySpeakerFromPlayerState()
{
	if (!ParleySpeakerComponent)
	{
		return;
	}

	// Allow arbitrary possessed pawns to keep authored speaker identities; only auto-seed when unset.
	if (ParleySpeakerComponent->GetSpeakerTag().IsValid())
	{
		return;
	}

	const AARPlayerStateBase* ARPlayerState = GetPlayerState<AARPlayerStateBase>();
	if (!ARPlayerState)
	{
		return;
	}

	const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ARPlayerState->GetCurrentCharacterTag());
	switch (ARPlayer::GetCharacterChoiceForTag(CharacterTag))
	{
	case EARCharacterChoice::Brother:
		ParleySpeakerComponent->SetSpeakerTag(ARPlayer::GetBrotherCharacterTag());
		break;
	case EARCharacterChoice::Sister:
		ParleySpeakerComponent->SetSpeakerTag(ARPlayer::GetSisterCharacterTag());
		break;
	default:
		break;
	}
}
