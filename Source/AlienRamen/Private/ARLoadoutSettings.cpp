#include "ARLoadoutSettings.h"

#include "ARLog.h"

int32 UARLoadoutSettings::MergeMissingLoadoutTagsIntoUnlocks(FGameplayTagContainer& InOutUnlocks, const FGameplayTagContainer& InLoadoutTags)
{
	int32 AddedCount = 0;
	for (const FGameplayTag& LoadoutTag : InLoadoutTags)
	{
		if (!LoadoutTag.IsValid() || InOutUnlocks.HasTagExact(LoadoutTag))
		{
			continue;
		}

		InOutUnlocks.AddTag(LoadoutTag);
		++AddedCount;
	}

	return AddedCount;
}

FGameplayTagContainer UARLoadoutSettings::GetEffectiveDefaultStartingUnlocks() const
{
	FGameplayTagContainer EffectiveUnlocks = DefaultStartingUnlocks;
	MergeMissingLoadoutTagsIntoUnlocks(EffectiveUnlocks, DefaultPlayerLoadoutTags);
	return EffectiveUnlocks;
}

bool UARLoadoutSettings::AreDefaultTagsConsistent() const
{
	for (const FGameplayTag& LoadoutTag : DefaultPlayerLoadoutTags)
	{
		if (!LoadoutTag.IsValid())
		{
			continue;
		}

		if (!DefaultStartingUnlocks.HasTagExact(LoadoutTag))
		{
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
void UARLoadoutSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const int32 AddedCount = MergeMissingLoadoutTagsIntoUnlocks(DefaultStartingUnlocks, DefaultPlayerLoadoutTags);
	if (AddedCount > 0)
	{
		UE_LOG(ARLog, Warning, TEXT("[LoadoutSettings] Added %d default loadout tag(s) into DefaultStartingUnlocks to keep defaults consistent."), AddedCount);
		SaveConfig();
	}
}
#endif
