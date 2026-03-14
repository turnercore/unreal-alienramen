#include "ParleyPlayerSlotHelpers.h"

#include "ParleyDialogueSettings.h"
#include "GameplayTagsManager.h"

namespace
{
	static FGameplayTag GetConfiguredSlotTagByIndex(const int32 SlotIndex)
	{
		const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
		FGameplayTag Tag;
		if (Settings)
		{
			Tag = Settings->PlayerSlotTags.IsValidIndex(SlotIndex) ? Settings->PlayerSlotTags[SlotIndex] : FGameplayTag();
		}

		if (!Tag.IsValid())
		{
			switch (SlotIndex)
			{
			case 0:
				Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Player.Slot.P1")), false);
				break;
			case 1:
				Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Player.Slot.P2")), false);
				break;
			default:
				break;
			}
		}

		return Tag;
	}
}

namespace ParleyPlayerSlot
{
	FGameplayTag SlotToTag(const EParleyPlayerSlot Slot)
	{
		switch (Slot)
		{
		case EParleyPlayerSlot::P1:
			return GetP1Tag();
		case EParleyPlayerSlot::P2:
			return GetP2Tag();
		default:
			return FGameplayTag();
		}
	}

	EParleyPlayerSlot TagToSlot(const FGameplayTag& SlotTag)
	{
		switch (GetIndexForTag(SlotTag))
		{
		case 0:
			return EParleyPlayerSlot::P1;
		case 1:
			return EParleyPlayerSlot::P2;
		default:
			return EParleyPlayerSlot::Unknown;
		}
	}

	FGameplayTag GetP1Tag()
	{
		return GetConfiguredSlotTagByIndex(0);
	}

	FGameplayTag GetP2Tag()
	{
		return GetConfiguredSlotTagByIndex(1);
	}

	FGameplayTag GetTagByIndex(const int32 SlotIndex)
	{
		return GetConfiguredSlotTagByIndex(SlotIndex);
	}

	int32 GetIndexForTag(const FGameplayTag& SlotTag)
	{
		if (!SlotTag.IsValid())
		{
			return INDEX_NONE;
		}

		if (SlotTag.MatchesTagExact(GetConfiguredSlotTagByIndex(0)))
		{
			return 0;
		}

		if (SlotTag.MatchesTagExact(GetConfiguredSlotTagByIndex(1)))
		{
			return 1;
		}

		return INDEX_NONE;
	}

	FGameplayTag NormalizeSlotTag(const FGameplayTag& SlotTag, const FGameplayTag& FallbackSlotTag)
	{
		if (IsValidSlotTag(SlotTag))
		{
			return SlotTag;
		}

		if (IsValidSlotTag(FallbackSlotTag))
		{
			return FallbackSlotTag;
		}

		return FGameplayTag();
	}

	bool IsValidSlotTag(const FGameplayTag& SlotTag)
	{
		return GetIndexForTag(SlotTag) != INDEX_NONE;
	}
}
