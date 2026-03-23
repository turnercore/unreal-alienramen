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
	static inline FGameplayTag GetPlayerSlotRootTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Player.Slot"), false);
	}

	static inline FGameplayTag GetPlayerSlotP1Tag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Player.Slot.P1"), false);
	}

	static inline FGameplayTag GetPlayerSlotP2Tag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Player.Slot.P2"), false);
	}

	static inline FGameplayTag GetPlayerSlotTag(const EARPlayerSlot PlayerSlot)
	{
		switch (PlayerSlot)
		{
		case EARPlayerSlot::P1:
			return GetPlayerSlotP1Tag();
		case EARPlayerSlot::P2:
			return GetPlayerSlotP2Tag();
		default:
			return FGameplayTag();
		}
	}

	static inline EARPlayerSlot GetPlayerSlotForTag(const FGameplayTag& PlayerSlotTag)
	{
		if (!PlayerSlotTag.IsValid())
		{
			return EARPlayerSlot::Unknown;
		}

		const FGameplayTag P1Tag = GetPlayerSlotP1Tag();
		if (P1Tag.IsValid() && PlayerSlotTag.MatchesTagExact(P1Tag))
		{
			return EARPlayerSlot::P1;
		}

		const FGameplayTag P2Tag = GetPlayerSlotP2Tag();
		if (P2Tag.IsValid() && PlayerSlotTag.MatchesTagExact(P2Tag))
		{
			return EARPlayerSlot::P2;
		}

		return EARPlayerSlot::Unknown;
	}

	static inline FGameplayTag NormalizePlayerSlotTag(
		const FGameplayTag& PlayerSlotTag,
		const EARPlayerSlot FallbackSlot = EARPlayerSlot::Unknown)
	{
		if (GetPlayerSlotForTag(PlayerSlotTag) != EARPlayerSlot::Unknown)
		{
			return PlayerSlotTag;
		}

		if (FallbackSlot != EARPlayerSlot::Unknown)
		{
			return GetPlayerSlotTag(FallbackSlot);
		}

		return FGameplayTag();
	}

	static inline FGameplayTag GetBrotherCharacterTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Shop.Character.Brother"), false);
	}

	static inline FGameplayTag GetSisterCharacterTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Shop.Character.Sister"), false);
	}

	static inline FGameplayTag GetBrotherParleySpeakerTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Parley.Speaker.Brother"), false);
	}

	static inline FGameplayTag GetSisterParleySpeakerTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Parley.Speaker.Sister"), false);
	}

	static inline FGameplayTag GetBrotherShopCharacterTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Shop.Character.Brother"), false);
	}

	static inline FGameplayTag GetSisterShopCharacterTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Shop.Character.Sister"), false);
	}

	static inline FGameplayTag GetCharacterTagForChoice(const EARCharacterChoice Choice)
	{
		switch (Choice)
		{
		case EARCharacterChoice::Brother:
			return GetBrotherShopCharacterTag();
		case EARCharacterChoice::Sister:
			return GetSisterShopCharacterTag();
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

		const FGameplayTag BrotherCanonicalTag = GetBrotherShopCharacterTag();
		if (BrotherCanonicalTag.IsValid() && CharacterTag.MatchesTagExact(BrotherCanonicalTag))
		{
			return EARCharacterChoice::Brother;
		}

		const FGameplayTag SisterCanonicalTag = GetSisterShopCharacterTag();
		if (SisterCanonicalTag.IsValid() && CharacterTag.MatchesTagExact(SisterCanonicalTag))
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
			return GetCharacterTagForChoice(EARCharacterChoice::Sister);
		case EARPlayerSlot::P1:
		default:
			return GetCharacterTagForChoice(EARCharacterChoice::Brother);
		}
	}

	static inline FGameplayTag GetCharacterTagForPlayerSlot(const EARPlayerSlot PlayerSlot)
	{
		return GetDefaultCharacterTagForSlot(PlayerSlot);
	}

	static inline EARPlayerSlot GetPlayerSlotForCharacterTag(const FGameplayTag& CharacterTag)
	{
		switch (GetCharacterChoiceForTag(CharacterTag))
		{
		case EARCharacterChoice::Brother:
			return EARPlayerSlot::P1;
		case EARCharacterChoice::Sister:
			return EARPlayerSlot::P2;
		default:
			return EARPlayerSlot::Unknown;
		}
	}

	static inline FGameplayTag NormalizeCharacterTag(const FGameplayTag& CharacterTag, const EARPlayerSlot FallbackSlot = EARPlayerSlot::Unknown)
	{
		const EARCharacterChoice ResolvedChoice = GetCharacterChoiceForTag(CharacterTag);
		if (ResolvedChoice != EARCharacterChoice::None)
		{
			return GetCharacterTagForChoice(ResolvedChoice);
		}

		if (FallbackSlot != EARPlayerSlot::Unknown)
		{
			return GetDefaultCharacterTagForSlot(FallbackSlot);
		}

		return FGameplayTag();
	}
}
