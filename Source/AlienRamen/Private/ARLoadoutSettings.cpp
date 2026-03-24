#include "ARLoadoutSettings.h"

FGameplayTagContainer UARLoadoutSettings::GetEffectiveDefaultStartingUnlocks() const
{
	return DefaultStartingUnlocks;
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
	SaveConfig();
}
#endif
