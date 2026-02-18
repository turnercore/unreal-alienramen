#include "GameplayTagUtilities.h"
#include "GameplayTagsManager.h"

static FString JoinParts(const TArray<FString>& Parts, int32 NumParts)
{
	FString Out;
	for (int32 i = 0; i < NumParts; ++i)
	{
		if (i > 0) Out += TEXT(".");
		Out += Parts[i];
	}
	return Out;
}

bool UGameplayTagUtilities::SplitTagToParts(FGameplayTag Tag, TArray<FString>& OutParts)
{
	OutParts.Reset();

	if (!Tag.IsValid())
	{
		return false;
	}

	const FString TagStr = Tag.ToString();
	if (TagStr.IsEmpty())
	{
		return false;
	}

	TagStr.ParseIntoArray(OutParts, TEXT("."), true);
	return OutParts.Num() > 0;
}

bool UGameplayTagUtilities::TryMakeTagFromParts(const TArray<FString>& Parts, int32 NumParts, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();

	if (NumParts <= 0 || NumParts > Parts.Num())
	{
		return false;
	}

	const FString Wanted = JoinParts(Parts, NumParts);

	// "false" here means: don't crash/assert if missing.
	// If tag doesn't exist in your project settings, this returns invalid.
	OutTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Wanted), false);
	return OutTag.IsValid();
}

int32 UGameplayTagUtilities::GetTagDepth(FGameplayTag Tag)
{
	TArray<FString> Parts;
	if (!SplitTagToParts(Tag, Parts)) return 0;
	return Parts.Num();
}

bool UGameplayTagUtilities::TryGetParentTag(FGameplayTag Tag, FGameplayTag& OutParent)
{
	TArray<FString> Parts;
	if (!SplitTagToParts(Tag, Parts)) return false;
	if (Parts.Num() < 2) return false;

	return TryMakeTagFromParts(Parts, Parts.Num() - 1, OutParent);
}

bool UGameplayTagUtilities::TryGetAncestorTag(FGameplayTag Tag, int32 UpLevels, FGameplayTag& OutAncestor)
{
	TArray<FString> Parts;
	if (!SplitTagToParts(Tag, Parts)) return false;
	if (UpLevels <= 0) return false;

	const int32 WantedParts = Parts.Num() - UpLevels;
	if (WantedParts < 1) return false;

	return TryMakeTagFromParts(Parts, WantedParts, OutAncestor);
}

bool UGameplayTagUtilities::TryGetTopLevelTag(FGameplayTag Tag, FGameplayTag& OutTopLevel)
{
	TArray<FString> Parts;
	if (!SplitTagToParts(Tag, Parts)) return false;

	return TryMakeTagFromParts(Parts, 1, OutTopLevel);
}

bool UGameplayTagUtilities::TryGetTagAtDepth(FGameplayTag Tag, int32 Depth, FGameplayTag& OutTagAtDepth)
{
	TArray<FString> Parts;
	if (!SplitTagToParts(Tag, Parts)) return false;

	// Depth is 1..Parts.Num()
	if (Depth < 1 || Depth > Parts.Num()) return false;

	return TryMakeTagFromParts(Parts, Depth, OutTagAtDepth);
}

bool UGameplayTagUtilities::TryGetAllPrefixTags(FGameplayTag Tag, FGameplayTagContainer& OutPrefixes)
{
	OutPrefixes.Reset();

	TArray<FString> Parts;
	if (!SplitTagToParts(Tag, Parts)) return false;

	for (int32 Depth = 1; Depth <= Parts.Num(); ++Depth)
	{
		FGameplayTag Prefix;
		if (TryMakeTagFromParts(Parts, Depth, Prefix))
		{
			OutPrefixes.AddTag(Prefix);
		}
	}

	return OutPrefixes.Num() > 0;
}

bool UGameplayTagUtilities::ReplaceTagInSlot(FGameplayTagContainer& InOutContainer, FGameplayTag NewTag)
{
	if (!NewTag.IsValid()) return false;

	// Slot is the direct parent of NewTag (one-up).
	FGameplayTag SlotTag;
	if (!TryGetParentTag(NewTag, SlotTag))
	{
		// No parent => can't define a slot.
		return false;
	}

	// Remove everything that is a descendant of SlotTag, but keep SlotTag itself.
	// We have to iterate since RemoveTagsMatching isn't exposed everywhere in BP.
	TArray<FGameplayTag> ExistingTags;
	InOutContainer.GetGameplayTagArray(ExistingTags);

	for (const FGameplayTag& T : ExistingTags)
	{
		if (!T.IsValid()) continue;

		const bool bIsInSlotSubtree = T.MatchesTag(SlotTag); // hierarchical
		const bool bIsExactlySlot = (T == SlotTag);

		if (bIsInSlotSubtree && !bIsExactlySlot)
		{
			InOutContainer.RemoveTag(T);
		}
	}

	// Add the new selection tag itself
	InOutContainer.AddTag(NewTag);
	return true;
}
