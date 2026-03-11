#include "AREmotionResolverSubsystem.h"

#include "AREmotionSettings.h"
#include "AREmotionTypes.h"
#include "ARLog.h"
#include "TagContentResolverSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
	enum class EAREmotionTableSource : uint8
	{
		TagContentResolverRuntime,
		TagContentResolverConfiguredRoutes
	};

	static const TCHAR* ToEmotionTableSourceText(const EAREmotionTableSource Source)
	{
		switch (Source)
		{
		case EAREmotionTableSource::TagContentResolverRuntime:
			return TEXT("TagContentResolver(Runtime)");
		case EAREmotionTableSource::TagContentResolverConfiguredRoutes:
			return TEXT("TagContentResolver(ConfiguredRoutes)");
		default:
			return TEXT("TagContentResolver");
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
		const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
		return Settings && Settings->bEnableVerboseResolverLogs;
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
		const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
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

		return UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.Emotion")), false);
	}

	static FGameplayTag ResolveGenericEmotionRootTag()
	{
		const UAREmotionSettings* Settings = GetDefault<UAREmotionSettings>();
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

	static bool TryResolveEmotionDataTable(
		UGameInstance* GameInstance,
		UDataTable*& OutDataTable,
		EAREmotionTableSource& OutSource,
		FGameplayTag& OutRouteRootTag,
		FString& OutResolveError)
	{
		OutDataTable = nullptr;
		OutSource = EAREmotionTableSource::TagContentResolverConfiguredRoutes;
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
				if (UTagContentResolverSubsystem* ResolverSubsystem = GameInstance->GetSubsystem<UTagContentResolverSubsystem>())
				{
					FString ResolverError;
					if (ResolverSubsystem->TryResolveDataTableForRootTag(ResolverRootTag, OutDataTable, ResolverError))
					{
						OutSource = EAREmotionTableSource::TagContentResolverRuntime;
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
			if (UTagContentResolverSubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
				ResolverRootTag,
				OutDataTable,
				ConfiguredRouteError))
			{
				OutSource = EAREmotionTableSource::TagContentResolverConfiguredRoutes;
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

		AppendError(TEXT("No TagContentResolver route could resolve the configured emotion root."));
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
				UE_LOG(ARLog, Warning, TEXT("[Emotion] BuildIconMapFromDataTable failed: DataTable is null."));
			}
			return false;
		}

		const UScriptStruct* RowStruct = EmotionDataTable->GetRowStruct();
		if (!RowStruct || !RowStruct->IsChildOf(FAREmotionIconRow::StaticStruct()))
		{
			if (bLogRowWarnings)
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[Emotion] EmotionDataTable '%s' has incompatible row struct '%s' (expected FAREmotionIconRow)."),
					*GetNameSafe(EmotionDataTable),
					*GetNameSafe(RowStruct));
			}
			return false;
		}

		const bool bVerboseRowLogging = bLogRowWarnings || ShouldLogResolverVerbose();
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
				if (bVerboseRowLogging)
				{
					UE_LOG(ARLog, Warning, TEXT("[Emotion] Row '%s' skipped: missing valid EmotionTag."), *RowName.ToString());
				}
				continue;
			}

			if (Row->IconTexture.IsNull())
			{
				if (bVerboseRowLogging)
				{
					UE_LOG(ARLog, Warning, TEXT("[Emotion] Row '%s' (%s) skipped: IconTexture is empty."), *RowName.ToString(), *EmotionTag.ToString());
				}
				continue;
			}

			if (OutIconByEmotionTag.Contains(EmotionTag))
			{
				if (bVerboseRowLogging)
				{
					UE_LOG(ARLog, Warning, TEXT("[Emotion] Duplicate EmotionTag '%s' in EmotionDataTable. First mapping kept."), *EmotionTag.ToString());
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
		const bool bLogRowWarnings)
	{
		OutIconByEmotionTag.Reset();
		OutResolvedDataTable = nullptr;
		OutResolvedDataTablePath.Reset();
		OutResolvedDataSource.Reset();

		EAREmotionTableSource Source = EAREmotionTableSource::TagContentResolverConfiguredRoutes;
		FGameplayTag RouteRootTag;
		FString ResolveError;
		if (!TryResolveEmotionDataTable(GameInstance, OutResolvedDataTable, Source, RouteRootTag, ResolveError))
		{
			if (bLogRowWarnings || ShouldLogResolverVerbose())
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[Emotion] Failed resolving icon DataTable from TagContentResolver routes: %s"),
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
			if (bLogRowWarnings || ShouldLogResolverVerbose())
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[Emotion] Resolved icon DataTable '%s' from %s, but no valid icon rows were found."),
					*GetNameSafe(OutResolvedDataTable),
					*OutResolvedDataSource);
			}
			return false;
		}

		if (ShouldLogResolverVerbose())
		{
			UE_LOG(
				ARLog,
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

		if (ShouldLogResolverVerbose())
		{
			UE_LOG(
				ARLog,
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
				ARLog,
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
		UE_LOG(ARLog, Verbose, TEXT("[Emotion] Resolve miss: Requested=%s"), *RequestedEmotionTag.ToString());
	}

	return false;
}

bool UAREmotionResolverSubsystem::TryResolveEmotionIconFromConfiguredData(
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
		false))
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
		true);

	CachedEmotionDataTablePath = ResolvedDataTablePath;
	CachedResolverRootTag = CurrentResolverRootTag;
	CachedGenericRootTag = CurrentGenericRootTag;
	BindToConfiguredDataTable(ResolvedDataTable);
	++CacheBuildCount;

	if (bCacheBuilt && ShouldLogResolverVerbose())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Emotion] Cache rebuilt from %s (DataTable=%s, Icons=%d)."),
			*ResolvedDataSource,
			*CachedEmotionDataTablePath.ToString(),
			IconByEmotionTag.Num());
	}

	return bCacheBuilt;
}

void UAREmotionResolverSubsystem::RegisterDebugConsoleCommands()
{
	UnregisterDebugConsoleCommands();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	CmdLogCacheStats = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.emotion.log_cache_stats"),
		TEXT("Logs emotion resolver cache hit/miss/build stats."),
		FConsoleCommandDelegate::CreateUObject(this, &UAREmotionResolverSubsystem::LogCacheStats),
		ECVF_Default);

	CmdRebuildCache = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.emotion.rebuild_cache"),
		TEXT("Forces an emotion resolver cache rebuild."),
		FConsoleCommandDelegate::CreateUObject(this, &UAREmotionResolverSubsystem::RebuildCache),
		ECVF_Default);
}

void UAREmotionResolverSubsystem::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// Teardown can invalidate console-object pointers before subsystem deinit.
	// Unregister by name to avoid dereferencing stale pointers.
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.emotion.log_cache_stats"), false);
	ConsoleManager.UnregisterConsoleObject(TEXT("ar.emotion.rebuild_cache"), false);
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

void UAREmotionResolverSubsystem::BindToConfiguredDataTable(UDataTable* DataTable)
{
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
	const FGameplayTag CurrentResolverRootTag = ResolveEmotionResolverRootTag();
	const FGameplayTag CurrentGenericRootTag = Settings ? Settings->GenericEmotionRootTag : FGameplayTag();

	return !AreResolverTagsEqual(CurrentResolverRootTag, CachedResolverRootTag)
		|| !AreResolverTagsEqual(CurrentGenericRootTag, CachedGenericRootTag);
}
