#include "ARTaggedPlayerStart.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

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

#if WITH_EDITOR
bool AARTaggedPlayerStart::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// This actor class routes runtime matching through SpawnIdentityTag only.
	if (InProperty && InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(APlayerStart, PlayerStartTag))
	{
		return false;
	}

	return true;
}
#endif
