#include "ARTaggedPlayerStart.h"

bool AARTaggedPlayerStart::MatchesSpawnIdentityTag(const FGameplayTag& QueryTag, const bool bRequireExact) const
{
	if (!SpawnIdentityTag.IsValid() || !QueryTag.IsValid())
	{
		return false;
	}

	if (bRequireExact || bExactTagMatchOnly)
	{
		return SpawnIdentityTag.MatchesTagExact(QueryTag);
	}

	return SpawnIdentityTag.MatchesTag(QueryTag) || QueryTag.MatchesTag(SpawnIdentityTag);
}
