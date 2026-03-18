#include "AREmotionViewerTags.h"

#include "ARPlayerStateBase.h"
#include "ARPlayerTypes.h"
#include "ParleySpeakerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

namespace
{
	static void AddViewerTagIfValid(FGameplayTagContainer& InOutTags, const FGameplayTag Tag)
	{
		if (Tag.IsValid())
		{
			InOutTags.AddTag(Tag);
		}
	}

	static void AddDerivedCharacterTags(FGameplayTagContainer& InOutTags, const FGameplayTag CharacterTag)
	{
		const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
		AddViewerTagIfValid(InOutTags, NormalizedCharacterTag);

		switch (ARPlayer::GetCharacterChoiceForTag(NormalizedCharacterTag))
		{
		case EARCharacterChoice::Brother:
			AddViewerTagIfValid(InOutTags, ARPlayer::GetBrotherParleySpeakerTag());
			break;
		case EARCharacterChoice::Sister:
			AddViewerTagIfValid(InOutTags, ARPlayer::GetSisterParleySpeakerTag());
			break;
		default:
			break;
		}
	}
}

FGameplayTagContainer AREmotion::BuildEmotionViewerTags(const APlayerState* PlayerState, const APawn* PossessedPawn)
{
	FGameplayTagContainer ViewerTags;

	if (const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
	{
		AddDerivedCharacterTags(ViewerTags, ARPlayerState->GetCurrentCharacterTag());
	}

	if (const UParleySpeakerComponent* SpeakerComponent = PossessedPawn ? PossessedPawn->FindComponentByClass<UParleySpeakerComponent>() : nullptr)
	{
		AddViewerTagIfValid(ViewerTags, SpeakerComponent->GetSpeakerTag());
	}

	return ViewerTags;
}
