#include "ARInvaderSpicyTrackSettings.h"

UARInvaderSpicyTrackSettings::UARInvaderSpicyTrackSettings()
{
	if (LevelOffsetWeights.IsEmpty())
	{
		LevelOffsetWeights = {
			{ 0, 1.00f },
			{ 1, 0.50f },
			{ -1, 0.50f },
			{ 2, 0.20f },
			{ -2, 0.20f },
			{ 3, 0.05f },
			{ -3, 0.05f }
		};
	}

	if (SkipScrapRewardByTier.IsEmpty())
	{
		SkipScrapRewardByTier = { 10, 20, 30, 40, 60 };
	}

	DefaultBaseKillSpiceValue = FMath::Max(1.0f, DefaultBaseKillSpiceValue);
}

int32 UARInvaderSpicyTrackSettings::GetSkipScrapRewardForTier(const int32 Tier) const
{
	if (Tier <= 0)
	{
		return 0;
	}

	const int32 Index = Tier - 1;
	if (!SkipScrapRewardByTier.IsValidIndex(Index))
	{
		return 0;
	}

	return FMath::Max(0, SkipScrapRewardByTier[Index]);
}

