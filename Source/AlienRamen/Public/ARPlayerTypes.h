#pragma once
/**
 * @file ARPlayerTypes.h
 * @brief Shared player-facing enum types for Alien Ramen gameplay/save/runtime APIs.
 */

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPlayerTypes.generated.h"

UENUM(BlueprintType)
enum class EARPlayerSlot : uint8
{
	Unknown = 0,
	P1,
	P2
};

UENUM(BlueprintType)
enum class EARCharacterChoice : uint8
{
	None = 0,
	Brother,
	Sister
};

UENUM(BlueprintType)
enum class EARCoreAttributeType : uint8
{
	Health,
	MaxHealth,
	Spice,
	MaxSpice,
	MoveSpeed,
	Strength
};

namespace ARPlayer
{
	static inline FGameplayTag GetBrotherCharacterTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Dialogue.Speaker.Brother"), false);
	}

	static inline FGameplayTag GetSisterCharacterTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Dialogue.Speaker.Sister"), false);
	}

	static inline FGameplayTag GetCharacterTagForChoice(const EARCharacterChoice Choice)
	{
		switch (Choice)
		{
		case EARCharacterChoice::Brother:
			return GetBrotherCharacterTag();
		case EARCharacterChoice::Sister:
			return GetSisterCharacterTag();
		default:
			return FGameplayTag();
		}
	}

	static inline EARCharacterChoice GetCharacterChoiceForTag(const FGameplayTag& CharacterTag)
	{
		if (!CharacterTag.IsValid())
		{
			return EARCharacterChoice::None;
		}

		const FGameplayTag BrotherTag = GetBrotherCharacterTag();
		if (BrotherTag.IsValid() && CharacterTag.MatchesTag(BrotherTag))
		{
			return EARCharacterChoice::Brother;
		}

		const FGameplayTag SisterTag = GetSisterCharacterTag();
		if (SisterTag.IsValid() && CharacterTag.MatchesTag(SisterTag))
		{
			return EARCharacterChoice::Sister;
		}

		return EARCharacterChoice::None;
	}

	static inline FGameplayTag GetDefaultCharacterTagForSlot(const EARPlayerSlot PlayerSlot)
	{
		switch (PlayerSlot)
		{
		case EARPlayerSlot::P2:
			return GetSisterCharacterTag();
		case EARPlayerSlot::P1:
		default:
			return GetBrotherCharacterTag();
		}
	}

	static inline FGameplayTag NormalizeCharacterTag(const FGameplayTag& CharacterTag, const EARPlayerSlot FallbackSlot = EARPlayerSlot::Unknown)
	{
		if (GetCharacterChoiceForTag(CharacterTag) != EARCharacterChoice::None)
		{
			return CharacterTag;
		}

		if (FallbackSlot != EARPlayerSlot::Unknown)
		{
			return GetDefaultCharacterTagForSlot(FallbackSlot);
		}

		return FGameplayTag();
	}
}
