#include "EmoResolverSubsystem.h"

#include "EmoComponent.h"
#include "EmoSettings.h"
#include "EmoTypes.h"
#include "EmoLog.h"
#include "TagKeySubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
	enum class EEmoEmotionTableSource : uint8
	{
		TagKeyRuntime,
		TagKeyConfiguredRoutes
	};

	static const TCHAR* ToEmotionTableSourceText(const EEmoEmotionTableSource Source)
	{
		switch (Source)
		{
		case EEmoEmotionTableSource::TagKeyRuntime:
			return TEXT("TagKey(Runtime)");
		case EEmoEmotionTableSource::TagKeyConfiguredRoutes:
			return TEXT("TagKey(ConfiguredRoutes)");
		default:
			return TEXT("TagKey");
		}
	}

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

	static bool ShouldLogResolverVerbose()
	{
		const UEmoSettings* Settings = GetDefault<UEmoSettings>();
		return Settings && Settings->bEnableVerboseResolverLogs;
	}

	static void AddUniqueCandidate(TArray<FGameplayTag>& Candidates, const FGameplayTag CandidateTag)
	{
		if (!CandidateTag.IsValid())
		{
			return;
		}

		if (!Candidates.ContainsByPredicate([&CandidateTag](const FGameplayTag ExistingTag)
			{
				return AreResolverTagsEqual(ExistingTag, CandidateTag);
			}))
		{
			Candidates.Add(CandidateTag);
		}
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

	static FGameplayTag ResolveEmotionResolverRootTag()
	{
		const UEmoSettings* Settings = GetDefault<UEmoSettings>();
		if (Settings)
		{
			if (Settings->EmotionResolverRootTag.IsValid())
			{
				return Settings->EmotionResolverRootTag;
			}

			if (Settings->GenericEmotionRootTag.IsValid())
			{
				return Settings->GenericEmotionRootTag;
			}
		}

		return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Parley.Emotion")), false);
	}

	static FGameplayTag ResolveGenericEmotionRootTag()
	{
		const UEmoSettings* Settings = GetDefault<UEmoSettings>();
		if (Settings && Settings->GenericEmotionRootTag.IsValid())
		{
			return Settings->GenericEmotionRootTag;
		}

		return ResolveEmotionResolverRootTag();
	}

	static void BuildLookupCandidatesInternal(const FGameplayTag& RequestedEmotionTag, TArray<FGameplayTag>& OutCandidates)
	{
		OutCandidates.Reset();
		if (!RequestedEmotionTag.IsValid())
		{
			return;
		}

		AddUniqueCandidate(OutCandidates, RequestedEmotionTag);

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

		const bool bIsSpeakerTag = SpeakerIndex != INDEX_NONE;
		const bool bHasSpeakerEmotionLeaf = bIsSpeakerTag && Segments.IsValidIndex(SpeakerIndex + 2);

		FString SuffixPath;
		if (bHasSpeakerEmotionLeaf)
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
			// Keep exact-tag lookup only when no generic candidate exists.
		}
		else
		{
			AddUniqueCandidate(OutCandidates, GenericCandidate);
		}
	}

	static bool TryResolveEmotionDataTable(
		UGameInstance* GameInstance,
		UDataTable*& OutDataTable,
		EEmoEmotionTableSource& OutSource,
		FGameplayTag& OutRouteRootTag,
		FString& OutResolveError)
	{
		OutDataTable = nullptr;
		OutSource = EEmoEmotionTableSource::TagKeyConfiguredRoutes;
		OutRouteRootTag = FGameplayTag();
		OutResolveError.Reset();

		auto AppendError = [&OutResolveError](const FString& Message)
		{
			if (Message.IsEmpty())
			{
				return;
			}

			if (!OutResolveError.IsEmpty())
			{
				OutResolveError += TEXT(" | ");
			}
			OutResolveError += Message;
		};

		const FGameplayTag ResolverRootTag = ResolveEmotionResolverRootTag();
		if (ResolverRootTag.IsValid())
		{
			if (GameInstance)
			{
				if (UTagKeySubsystem* ResolverSubsystem = GameInstance->GetSubsystem<UTagKeySubsystem>())
				{
					FString ResolverError;
					if (ResolverSubsystem->TryResolveDataTableForRootTag(ResolverRootTag, OutDataTable, ResolverError))
					{
						OutSource = EEmoEmotionTableSource::TagKeyRuntime;
						OutRouteRootTag = ResolverRootTag;
						return true;
					}

					AppendError(FString::Printf(
						TEXT("Runtime resolver root '%s' failed: %s"),
						*ResolverRootTag.ToString(),
						ResolverError.IsEmpty() ? TEXT("<no error>") : *ResolverError));
				}
				else
				{
					AppendError(TEXT("Runtime resolver subsystem unavailable."));
				}
			}
			else
			{
				AppendError(TEXT("Runtime resolver unavailable: no GameInstance."));
			}

			FString ConfiguredRouteError;
			if (UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
				ResolverRootTag,
				OutDataTable,
				ConfiguredRouteError))
			{
				OutSource = EEmoEmotionTableSource::TagKeyConfiguredRoutes;
				OutRouteRootTag = ResolverRootTag;
				return true;
			}

			AppendError(FString::Printf(
				TEXT("Configured-routes resolver root '%s' failed: %s"),
				*ResolverRootTag.ToString(),
				ConfiguredRouteError.IsEmpty() ? TEXT("<no error>") : *ConfiguredRouteError));
		}
		else
		{
			AppendError(TEXT("Emotion resolver root tag is invalid."));
		}

		AppendError(TEXT("No TagKey route could resolve the configured emotion root."));
		return false;
	}

	static bool BuildIconMapFromDataTable(
		UDataTable* EmotionDataTable,
		TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>>& OutIconByEmotionTag,
		const bool bLogRowWarnings)
	{
		OutIconByEmotionTag.Reset();

		if (!EmotionDataTable)
		{
			if (bLogRowWarnings)
			{
				UE_LOG(EmoLog, Warning, TEXT("[Emotion] BuildIconMapFromDataTable failed: DataTable is null."));
			}
			return false;
		}

		const UScriptStruct* RowStruct = EmotionDataTable->GetRowStruct();
		if (!RowStruct || !RowStruct->IsChildOf(FEmoIconRow::StaticStruct()))
		{
			if (bLogRowWarnings)
			{
				UE_LOG(
					EmoLog,
					Warning,
					TEXT("[Emotion] EmotionDataTable '%s' has incompatible row struct '%s' (expected FEmoIconRow)."),
					*GetNameSafe(EmotionDataTable),
					*GetNameSafe(RowStruct));
			}
			return false;
		}

		const bool bVerboseRowLogging = bLogRowWarnings || ShouldLogResolverVerbose();
		const FGameplayTag GenericRootTag = ResolveGenericEmotionRootTag();
		for (const FName RowName : EmotionDataTable->GetRowNames())
		{
			const FEmoIconRow* Row = EmotionDataTable->FindRow<FEmoIconRow>(RowName, TEXT("EmotionResolver"), false);
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
				if (bVerboseRowLogging)
				{
					UE_LOG(EmoLog, Warning, TEXT("[Emotion] Row '%s' skipped: missing valid EmotionTag."), *RowName.ToString());
				}
				continue;
			}

			if (Row->IconTexture.IsNull())
			{
				if (bVerboseRowLogging)
				{
					UE_LOG(EmoLog, Warning, TEXT("[Emotion] Row '%s' (%s) skipped: IconTexture is empty."), *RowName.ToString(), *EmotionTag.ToString());
				}
				continue;
			}

			if (OutIconByEmotionTag.Contains(EmotionTag))
			{
				if (bVerboseRowLogging)
				{
					UE_LOG(EmoLog, Warning, TEXT("[Emotion] Duplicate EmotionTag '%s' in EmotionDataTable. First mapping kept."), *EmotionTag.ToString());
				}
				continue;
			}

			OutIconByEmotionTag.Add(EmotionTag, Row->IconTexture);
		}

		return !OutIconByEmotionTag.IsEmpty();
	}

	static bool BuildIconMapFromConfiguredSource(
		UGameInstance* GameInstance,
		TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>>& OutIconByEmotionTag,
		UDataTable*& OutResolvedDataTable,
		FSoftObjectPath& OutResolvedDataTablePath,
		FString& OutResolvedDataSource,
		const bool bLogRowWarnings,
		const bool bLogResolveSummary)
	{
		OutIconByEmotionTag.Reset();
		OutResolvedDataTable = nullptr;
		OutResolvedDataTablePath.Reset();
		OutResolvedDataSource.Reset();

		EEmoEmotionTableSource Source = EEmoEmotionTableSource::TagKeyConfiguredRoutes;
		FGameplayTag RouteRootTag;
		FString ResolveError;
		if (!TryResolveEmotionDataTable(GameInstance, OutResolvedDataTable, Source, RouteRootTag, ResolveError))
		{
			if (bLogRowWarnings || (bLogResolveSummary && ShouldLogResolverVerbose()))
			{
				UE_LOG(
					EmoLog,
					Warning,
					TEXT("[Emotion] Failed resolving icon DataTable from TagKey routes: %s"),
					ResolveError.IsEmpty() ? TEXT("<no error>") : *ResolveError);
			}
			return false;
		}

		OutResolvedDataTablePath = FSoftObjectPath(OutResolvedDataTable);
		OutResolvedDataSource = RouteRootTag.IsValid()
			? FString::Printf(TEXT("%s Root=%s"), ToEmotionTableSourceText(Source), *RouteRootTag.ToString())
			: FString(ToEmotionTableSourceText(Source));

		const bool bBuilt = BuildIconMapFromDataTable(OutResolvedDataTable, OutIconByEmotionTag, bLogRowWarnings);
		if (!bBuilt)
		{
			if (bLogRowWarnings || (bLogResolveSummary && ShouldLogResolverVerbose()))
			{
				UE_LOG(
					EmoLog,
					Warning,
					TEXT("[Emotion] Resolved icon DataTable '%s' from %s, but no valid icon rows were found."),
					*GetNameSafe(OutResolvedDataTable),
					*OutResolvedDataSource);
			}
			return false;
		}

		if (bLogResolveSummary && ShouldLogResolverVerbose())
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion] Resolved icon DataTable '%s' via %s (Rows=%d, Icons=%d)."),
				*GetNameSafe(OutResolvedDataTable),
				*OutResolvedDataSource,
				OutResolvedDataTable ? OutResolvedDataTable->GetRowNames().Num() : 0,
				OutIconByEmotionTag.Num());
		}

		return true;
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

void UEmoResolverSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterDebugConsoleCommands();
	RebuildCache();
}

void UEmoResolverSubsystem::Deinitialize()
{
	UnregisterDebugConsoleCommands();
	UnbindDataTableChangedDelegate();

	IconByEmotionTag.Reset();
	RequestToResolvedTagCache.Reset();
	RequestToResolvedIconCache.Reset();
	RequestMissCache.Reset();
	BoundDataTable.Reset();
	CachedEmotionDataTablePath.Reset();
	CachedResolverRootTag = FGameplayTag();
	CachedGenericRootTag = FGameplayTag();
	CacheBuildCount = 0;
	LookupCount = 0;
	CacheHitCount = 0;
	CacheMissCount = 0;
	CacheInvalidationCount = 0;
	bCacheBuilt = false;
	Super::Deinitialize();
}

void UEmoResolverSubsystem::NotifyComponentEmotionChanged(UEmoComponent* Component, const FGameplayTag NewEmotionTag)
{
	if (!IsValid(Component))
	{
		return;
	}

	OnAnyEmotionChanged.Broadcast(Component, NewEmotionTag);
}

void UEmoResolverSubsystem::RebuildCache()
{
	BuildCache();
}

void UEmoResolverSubsystem::LogCacheStats() const
{
	UE_LOG(
		EmoLog,
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

bool UEmoResolverSubsystem::TryResolveEmotionIcon(
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

		if (ShouldLogResolverVerbose())
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion] Resolve fallback (%s): Requested=%s Resolved=%s Icon=%s"),
				bResolvedFromFallback ? TEXT("hit") : TEXT("miss"),
				*RequestedEmotionTag.ToString(),
				*OutResolvedEmotionTag.ToString(),
				OutIconTexture.IsNull() ? TEXT("<none>") : *OutIconTexture.ToSoftObjectPath().ToString());
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

		if (ShouldLogResolverVerbose())
		{
			UE_LOG(
				EmoLog,
				Verbose,
				TEXT("[Emotion] Resolve hit: Requested=%s Resolved=%s Icon=%s"),
				*RequestedEmotionTag.ToString(),
				*OutResolvedEmotionTag.ToString(),
				OutIconTexture.IsNull() ? TEXT("<none>") : *OutIconTexture.ToSoftObjectPath().ToString());
		}
		return true;
	}

	RequestMissCache.Add(RequestedEmotionTag);
	++CacheMissCount;

	if (ShouldLogResolverVerbose())
	{
		UE_LOG(EmoLog, Verbose, TEXT("[Emotion] Resolve miss: Requested=%s"), *RequestedEmotionTag.ToString());
	}

	return false;
}

bool UEmoResolverSubsystem::TryResolveEmotionIconFromConfiguredData(
	const FGameplayTag RequestedEmotionTag,
	TSoftObjectPtr<UTexture2D>& OutIconTexture,
	FGameplayTag& OutResolvedEmotionTag)
{
	UDataTable* ResolvedDataTable = nullptr;
	FSoftObjectPath ResolvedDataTablePath;
	FString ResolvedDataSource;
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> LocalIconMap;
	if (!BuildIconMapFromConfiguredSource(
		nullptr,
		LocalIconMap,
		ResolvedDataTable,
		ResolvedDataTablePath,
		ResolvedDataSource,
		false,
		false))
	{
		OutIconTexture.Reset();
		OutResolvedEmotionTag = FGameplayTag();
		return false;
	}

	return ResolveIconFromMap(LocalIconMap, RequestedEmotionTag, OutIconTexture, OutResolvedEmotionTag);
}

bool UEmoResolverSubsystem::EnsureCacheBuilt()
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

bool UEmoResolverSubsystem::BuildCache()
{
	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	const FGameplayTag CurrentResolverRootTag = ResolveEmotionResolverRootTag();
	const FGameplayTag CurrentGenericRootTag = Settings ? Settings->GenericEmotionRootTag : FGameplayTag();

	IconByEmotionTag.Reset();
	RequestToResolvedTagCache.Reset();
	RequestToResolvedIconCache.Reset();
	RequestMissCache.Reset();

	UnbindDataTableChangedDelegate();

	UDataTable* ResolvedDataTable = nullptr;
	FSoftObjectPath ResolvedDataTablePath;
	FString ResolvedDataSource;
	bCacheBuilt = BuildIconMapFromConfiguredSource(
		GetGameInstance(),
		IconByEmotionTag,
		ResolvedDataTable,
		ResolvedDataTablePath,
		ResolvedDataSource,
		true,
		true);

	CachedEmotionDataTablePath = ResolvedDataTablePath;
	CachedResolverRootTag = CurrentResolverRootTag;
	CachedGenericRootTag = CurrentGenericRootTag;
	BindToConfiguredDataTable(ResolvedDataTable);
	++CacheBuildCount;

	if (bCacheBuilt && ShouldLogResolverVerbose())
	{
		UE_LOG(
			EmoLog,
			Verbose,
			TEXT("[Emotion] Cache rebuilt from %s (DataTable=%s, Icons=%d)."),
			*ResolvedDataSource,
			*CachedEmotionDataTablePath.ToString(),
			IconByEmotionTag.Num());
	}

	return bCacheBuilt;
}

void UEmoResolverSubsystem::RegisterDebugConsoleCommands()
{
	UnregisterDebugConsoleCommands();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdLogCacheStats = ConsoleManager.RegisterConsoleCommand(
		TEXT("emo.log_cache_stats"),
		TEXT("Logs emotion resolver cache hit/miss/build stats."),
		FConsoleCommandDelegate::CreateUObject(this, &UEmoResolverSubsystem::LogCacheStats),
		ECVF_Default);

	CmdRebuildCache = ConsoleManager.RegisterConsoleCommand(
		TEXT("emo.rebuild_cache"),
		TEXT("Forces an emotion resolver cache rebuild."),
		FConsoleCommandDelegate::CreateUObject(this, &UEmoResolverSubsystem::RebuildCache),
		ECVF_Default);
}

void UEmoResolverSubsystem::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// Teardown can invalidate console-object pointers before subsystem deinit.
	// Unregister by name to avoid dereferencing stale pointers.
	ConsoleManager.UnregisterConsoleObject(TEXT("emo.log_cache_stats"), false);
	ConsoleManager.UnregisterConsoleObject(TEXT("emo.rebuild_cache"), false);
	ConsoleManager.UnregisterConsoleObject(TEXT("emo.LogCacheStats"), false);
	ConsoleManager.UnregisterConsoleObject(TEXT("emo.RebuildCache"), false);

	CmdLogCacheStats = nullptr;
	CmdRebuildCache = nullptr;
}

void UEmoResolverSubsystem::HandleEmotionDataTableChanged()
{
	++CacheInvalidationCount;
	UE_LOG(EmoLog, Verbose, TEXT("[Emotion] Source data table changed; rebuilding resolver cache."));
	RebuildCache();
}

void UEmoResolverSubsystem::BindToConfiguredDataTable(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return;
	}

	BoundDataTable = DataTable;
	DataTableChangedHandle = DataTable->OnDataTableChanged().AddUObject(this, &UEmoResolverSubsystem::HandleEmotionDataTableChanged);
}

void UEmoResolverSubsystem::UnbindDataTableChangedDelegate()
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

bool UEmoResolverSubsystem::HasConfigInputsChanged() const
{
	const UEmoSettings* Settings = GetDefault<UEmoSettings>();
	const FGameplayTag CurrentResolverRootTag = ResolveEmotionResolverRootTag();
	const FGameplayTag CurrentGenericRootTag = Settings ? Settings->GenericEmotionRootTag : FGameplayTag();
	UDataTable* CurrentResolvedDataTable = nullptr;
	FSoftObjectPath CurrentResolvedDataTablePath;
	EEmoEmotionTableSource CurrentResolvedSource = EEmoEmotionTableSource::TagKeyConfiguredRoutes;
	FString CurrentResolveError;
	const bool bResolved = TryResolveEmotionDataTable(
		GetGameInstance(),
		CurrentResolvedDataTable,
		CurrentResolvedSource,
		CurrentResolvedDataTablePath,
		CurrentResolveError);

	if (!bResolved)
	{
		return !CachedEmotionDataTablePath.IsNull();
	}

	return !AreResolverTagsEqual(CurrentResolverRootTag, CachedResolverRootTag)
		|| !AreResolverTagsEqual(CurrentGenericRootTag, CachedGenericRootTag)
		|| CurrentResolvedDataTablePath != CachedEmotionDataTablePath;
}
