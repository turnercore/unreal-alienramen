#include "AREmotionResolverSubsystem.h"

#include "AREmotionSettings.h"
#include "AREmotionTypes.h"
#include "ARLog.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameplayTagsManager.h"

namespace
{
	static bool AreTagsEqual(const FGameplayTag& Left, const FGameplayTag& Right)
	{
		if (!Left.IsValid() && !Right.IsValid())
		{
			return true;
		}
		if (!Left.IsValid() || !Right.IsValid())
		{
			return false;
		}
		return Left.MatchesTagExact(Right);
	}

	static FString JoinSegments(const TArray<FString>& Segments, const int32 StartIndex)
	{
		if (!Segments.IsValidIndex(StartIndex))
		{
			return FString();
		}

		FString Joined = Segments[StartIndex];
		for (int32 Index = StartIndex + 1; Index < Segments.Num(); ++Index)
		{
			Joined += TEXT(".");
			Joined += Segments[Index];
		}
		return Joined;
	}

	static FGameplayTag ResolveGenericEmotionRootTag()
	{
		const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
		if (Settings && Settings->GenericEmotionRootTag.IsValid())
		{
			return Settings->GenericEmotionRootTag;
		}

		return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.Emotion")), false);
	}

	static void BuildLookupCandidatesInternal(const FGameplayTag& RequestedEmotionTag, TArray<FGameplayTag>& OutCandidates)
	{
		OutCandidates.Reset();
		if (!RequestedEmotionTag.IsValid())
		{
			return;
		}

		OutCandidates.Add(RequestedEmotionTag);

		const FGameplayTag GenericRootTag = ResolveGenericEmotionRootTag();
		if (!GenericRootTag.IsValid())
		{
			return;
		}

		const FString RequestedPath = RequestedEmotionTag.ToString();
		TArray<FString> Segments;
		RequestedPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.IsEmpty())
		{
			return;
		}

		int32 SpeakerIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			if (Segments[Index].Equals(TEXT("speaker"), ESearchCase::IgnoreCase))
			{
				SpeakerIndex = Index;
				break;
			}
		}

		FString SuffixPath;
		if (SpeakerIndex != INDEX_NONE && Segments.IsValidIndex(SpeakerIndex + 2))
		{
			SuffixPath = JoinSegments(Segments, SpeakerIndex + 2);
		}
		else if (Segments.Num() > 1)
		{
			SuffixPath = Segments.Last();
		}

		if (SuffixPath.IsEmpty())
		{
			return;
		}

		const FString CandidatePath = FString::Printf(TEXT("%s.%s"), *GenericRootTag.ToString(), *SuffixPath);
		const FGameplayTag GenericCandidate = UGameplayTagsManager::Get().RequestGameplayTag(FName(*CandidatePath), false);
		if (!GenericCandidate.IsValid())
		{
			return;
		}

		if (!OutCandidates.ContainsByPredicate([&GenericCandidate](const FGameplayTag ExistingTag)
			{
				return AreTagsEqual(ExistingTag, GenericCandidate);
			}))
		{
			OutCandidates.Add(GenericCandidate);
		}
	}

	static bool BuildIconMapFromSettings(TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>>& OutIconByEmotionTag)
	{
		OutIconByEmotionTag.Reset();

		const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
		if (!Settings)
		{
			return false;
		}

		if (Settings->EmotionDataTable.IsNull())
		{
			UE_LOG(ARLog, Warning, TEXT("[Emotion] No EmotionDataTable configured in UAREmotionSettings."));
			return false;
		}

		UDataTable* EmotionDataTable = Settings->EmotionDataTable.LoadSynchronous();
		if (!EmotionDataTable)
		{
			UE_LOG(ARLog, Warning, TEXT("[Emotion] Failed to load EmotionDataTable from settings."));
			return false;
		}

		const UScriptStruct* RowStruct = EmotionDataTable->GetRowStruct();
		if (!RowStruct || !RowStruct->IsChildOf(FAREmotionIconRow::StaticStruct()))
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Emotion] EmotionDataTable '%s' has incompatible row struct '%s' (expected FAREmotionIconRow)."),
				*GetNameSafe(EmotionDataTable),
				*GetNameSafe(RowStruct));
			return false;
		}

		const FGameplayTag GenericRootTag = ResolveGenericEmotionRootTag();
		for (const FName RowName : EmotionDataTable->GetRowNames())
		{
			const FAREmotionIconRow* Row = EmotionDataTable->FindRow<FAREmotionIconRow>(RowName, TEXT("EmotionResolver"), false);
			if (!Row)
			{
				continue;
			}

			FGameplayTag EmotionTag = Row->EmotionTag;
			if (!EmotionTag.IsValid() && GenericRootTag.IsValid() && !RowName.IsNone())
			{
				const FString DerivedPath = FString::Printf(TEXT("%s.%s"), *GenericRootTag.ToString(), *RowName.ToString());
				EmotionTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*DerivedPath), false);
			}

			if (!EmotionTag.IsValid())
			{
				UE_LOG(ARLog, Warning, TEXT("[Emotion] Row '%s' skipped: missing valid EmotionTag."), *RowName.ToString());
				continue;
			}

			if (Row->IconTexture.IsNull())
			{
				UE_LOG(ARLog, Warning, TEXT("[Emotion] Row '%s' (%s) skipped: IconTexture is empty."), *RowName.ToString(), *EmotionTag.ToString());
				continue;
			}

			if (OutIconByEmotionTag.Contains(EmotionTag))
			{
				UE_LOG(ARLog, Warning, TEXT("[Emotion] Duplicate EmotionTag '%s' in EmotionDataTable. First mapping kept."), *EmotionTag.ToString());
				continue;
			}

			OutIconByEmotionTag.Add(EmotionTag, Row->IconTexture);
		}

		return !OutIconByEmotionTag.IsEmpty();
	}

	static bool ResolveIconFromMap(
		const TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>>& IconByEmotionTag,
		const FGameplayTag RequestedEmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag)
	{
		OutIconTexture.Reset();
		OutResolvedEmotionTag = FGameplayTag();
		if (!RequestedEmotionTag.IsValid())
		{
			return false;
		}

		TArray<FGameplayTag> Candidates;
		BuildLookupCandidatesInternal(RequestedEmotionTag, Candidates);
		for (const FGameplayTag Candidate : Candidates)
		{
			const TSoftObjectPtr<UTexture2D>* FoundIcon = IconByEmotionTag.Find(Candidate);
			if (!FoundIcon || FoundIcon->IsNull())
			{
				continue;
			}

			OutIconTexture = *FoundIcon;
			OutResolvedEmotionTag = Candidate;
			return true;
		}

		return false;
	}
}

void UAREmotionResolverSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildCache();
}

void UAREmotionResolverSubsystem::Deinitialize()
{
	IconByEmotionTag.Reset();
	RequestToResolvedTagCache.Reset();
	RequestToResolvedIconCache.Reset();
	RequestMissCache.Reset();
	bCacheBuilt = false;
	Super::Deinitialize();
}

void UAREmotionResolverSubsystem::RebuildCache()
{
	BuildCache();
}

bool UAREmotionResolverSubsystem::TryResolveEmotionIcon(
	const FGameplayTag RequestedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag)
{
	OutIconTexture.Reset();
	OutResolvedEmotionTag = FGameplayTag();

	if (!RequestedEmotionTag.IsValid())
	{
		return false;
	}

	if (!EnsureCacheBuilt())
	{
		return TryResolveEmotionIconFromConfiguredData(RequestedEmotionTag, OutIconTexture, OutResolvedEmotionTag);
	}

	if (const FGameplayTag* CachedResolvedTag = RequestToResolvedTagCache.Find(RequestedEmotionTag))
	{
		if (const TSoftObjectPtr<UTexture2D>* CachedIcon = RequestToResolvedIconCache.Find(RequestedEmotionTag))
		{
			OutResolvedEmotionTag = *CachedResolvedTag;
			OutIconTexture = *CachedIcon;
			return OutIconTexture.IsValid() || !OutIconTexture.IsNull();
		}
	}

	if (RequestMissCache.Contains(RequestedEmotionTag))
	{
		return false;
	}

	const bool bResolved = ResolveIconFromMap(IconByEmotionTag, RequestedEmotionTag, OutIconTexture, OutResolvedEmotionTag);
	if (bResolved)
	{
		RequestToResolvedTagCache.Add(RequestedEmotionTag, OutResolvedEmotionTag);
		RequestToResolvedIconCache.Add(RequestedEmotionTag, OutIconTexture);
		return true;
	}

	RequestMissCache.Add(RequestedEmotionTag);
	return false;
}

bool UAREmotionResolverSubsystem::TryResolveEmotionIconFromConfiguredData(
	const FGameplayTag RequestedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag)
{
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> LocalIconMap;
	if (!BuildIconMapFromSettings(LocalIconMap))
	{
		OutIconTexture.Reset();
		OutResolvedEmotionTag = FGameplayTag();
		return false;
	}

	return ResolveIconFromMap(LocalIconMap, RequestedEmotionTag, OutIconTexture, OutResolvedEmotionTag);
}

bool UAREmotionResolverSubsystem::EnsureCacheBuilt()
{
	if (bCacheBuilt)
	{
		return true;
	}

	return BuildCache();
}

bool UAREmotionResolverSubsystem::BuildCache()
{
	IconByEmotionTag.Reset();
	RequestToResolvedTagCache.Reset();
	RequestToResolvedIconCache.Reset();
	RequestMissCache.Reset();
	bCacheBuilt = BuildIconMapFromSettings(IconByEmotionTag);
	return bCacheBuilt;
}
