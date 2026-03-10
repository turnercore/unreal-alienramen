#include "AREmotionResolverSubsystem.h"

#include "AREmotionSettings.h"
#include "AREmotionTypes.h"
#include "ARLog.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
	static bool AreResolverTagsEqual(const FGameplayTag& Left, const FGameplayTag& Right)
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
				return AreResolverTagsEqual(ExistingTag, GenericCandidate);
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
	RegisterDebugConsoleCommands();
	RebuildCache();
}

void UAREmotionResolverSubsystem::Deinitialize()
{
	UnregisterDebugConsoleCommands();
	UnbindDataTableChangedDelegate();

	IconByEmotionTag.Reset();
	RequestToResolvedTagCache.Reset();
	RequestToResolvedIconCache.Reset();
	RequestMissCache.Reset();
	BoundDataTable.Reset();
	CachedEmotionDataTablePath.Reset();
	CachedGenericRootTag = FGameplayTag();
	CacheBuildCount = 0;
	LookupCount = 0;
	CacheHitCount = 0;
	CacheMissCount = 0;
	CacheInvalidationCount = 0;
	bCacheBuilt = false;
	Super::Deinitialize();
}

void UAREmotionResolverSubsystem::RebuildCache()
{
	BuildCache();
}

void UAREmotionResolverSubsystem::LogCacheStats() const
{
	UE_LOG(
		ARLog,
		Log,
		TEXT("[Emotion] CacheStats Built=%d BuildCount=%llu Invalidations=%llu Lookups=%llu Hits=%llu Misses=%llu IconMap=%d ResolvedCache=%d MissCache=%d DataTable=%s"),
		bCacheBuilt ? 1 : 0,
		CacheBuildCount,
		CacheInvalidationCount,
		LookupCount,
		CacheHitCount,
		CacheMissCount,
		IconByEmotionTag.Num(),
		RequestToResolvedTagCache.Num(),
		RequestMissCache.Num(),
		*CachedEmotionDataTablePath.ToString());
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

	++LookupCount;

	if (!EnsureCacheBuilt())
	{
		const bool bResolvedFromFallback = TryResolveEmotionIconFromConfiguredData(RequestedEmotionTag, OutIconTexture, OutResolvedEmotionTag);
		if (!bResolvedFromFallback)
		{
			++CacheMissCount;
		}
		return bResolvedFromFallback;
	}

	if (const FGameplayTag* CachedResolvedTag = RequestToResolvedTagCache.Find(RequestedEmotionTag))
	{
		if (const TSoftObjectPtr<UTexture2D>* CachedIcon = RequestToResolvedIconCache.Find(RequestedEmotionTag))
		{
			++CacheHitCount;
			OutResolvedEmotionTag = *CachedResolvedTag;
			OutIconTexture = *CachedIcon;
			return OutIconTexture.IsValid() || !OutIconTexture.IsNull();
		}
	}

	if (RequestMissCache.Contains(RequestedEmotionTag))
	{
		++CacheMissCount;
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
	++CacheMissCount;
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
	const bool bConfigChanged = HasConfigInputsChanged();
	if (bCacheBuilt && !bConfigChanged)
	{
		return true;
	}

	if (bCacheBuilt && bConfigChanged)
	{
		++CacheInvalidationCount;
	}

	return BuildCache();
}

bool UAREmotionResolverSubsystem::BuildCache()
{
	const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
	const FSoftObjectPath CurrentDataTablePath = Settings ? Settings->EmotionDataTable.ToSoftObjectPath() : FSoftObjectPath();
	const FGameplayTag CurrentGenericRootTag = Settings ? Settings->GenericEmotionRootTag : FGameplayTag();

	IconByEmotionTag.Reset();
	RequestToResolvedTagCache.Reset();
	RequestToResolvedIconCache.Reset();
	RequestMissCache.Reset();

	UnbindDataTableChangedDelegate();

	CachedEmotionDataTablePath = CurrentDataTablePath;
	CachedGenericRootTag = CurrentGenericRootTag;
	bCacheBuilt = BuildIconMapFromSettings(IconByEmotionTag);
	BindToConfiguredDataTable();
	++CacheBuildCount;
	return bCacheBuilt;
}

void UAREmotionResolverSubsystem::RegisterDebugConsoleCommands()
{
	UnregisterDebugConsoleCommands();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdLogCacheStats = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.emotion.LogCacheStats"),
		TEXT("Logs emotion resolver cache hit/miss/build stats."),
		FConsoleCommandDelegate::CreateUObject(this, &UAREmotionResolverSubsystem::LogCacheStats),
		ECVF_Default);

	CmdRebuildCache = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.emotion.RebuildCache"),
		TEXT("Forces an emotion resolver cache rebuild."),
		FConsoleCommandDelegate::CreateUObject(this, &UAREmotionResolverSubsystem::RebuildCache),
		ECVF_Default);
}

void UAREmotionResolverSubsystem::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// Teardown can invalidate console-object pointers before subsystem deinit.
	// Unregister by name to avoid dereferencing stale pointers.
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.emotion.LogCacheStats"), false);
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.emotion.RebuildCache"), false);

	CmdLogCacheStats = nullptr;
	CmdRebuildCache = nullptr;
}

void UAREmotionResolverSubsystem::HandleEmotionDataTableChanged()
{
	++CacheInvalidationCount;
	UE_LOG(ARLog, Verbose, TEXT("[Emotion] Source data table changed; rebuilding resolver cache."));
	RebuildCache();
}

void UAREmotionResolverSubsystem::BindToConfiguredDataTable()
{
	const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
	if (!Settings || Settings->EmotionDataTable.IsNull())
	{
		return;
	}

	UDataTable* DataTable = Settings->EmotionDataTable.Get();
	if (!DataTable)
	{
		DataTable = Settings->EmotionDataTable.LoadSynchronous();
	}

	if (!DataTable)
	{
		return;
	}

	BoundDataTable = DataTable;
	DataTableChangedHandle = DataTable->OnDataTableChanged().AddUObject(this, &UAREmotionResolverSubsystem::HandleEmotionDataTableChanged);
}

void UAREmotionResolverSubsystem::UnbindDataTableChangedDelegate()
{
	if (UDataTable* DataTable = BoundDataTable.Get())
	{
		if (DataTableChangedHandle.IsValid())
		{
			DataTable->OnDataTableChanged().Remove(DataTableChangedHandle);
		}
	}

	DataTableChangedHandle.Reset();
	BoundDataTable.Reset();
}

bool UAREmotionResolverSubsystem::HasConfigInputsChanged() const
{
	const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
	const FSoftObjectPath CurrentDataTablePath = Settings ? Settings->EmotionDataTable.ToSoftObjectPath() : FSoftObjectPath();
	const FGameplayTag CurrentGenericRootTag = Settings ? Settings->GenericEmotionRootTag : FGameplayTag();

	return CurrentDataTablePath != CachedEmotionDataTablePath
		|| !AreResolverTagsEqual(CurrentGenericRootTag, CachedGenericRootTag);
}
