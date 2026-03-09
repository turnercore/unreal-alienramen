#include "TagContentResolverSubsystem.h"

#include "Algo/Sort.h"
#include "Engine/DataTable.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "TagContentResolverLog.h"
#include "TagContentResolverProvider.h"
#include "TagContentResolverSettings.h"

DECLARE_STATS_GROUP(TEXT("TagContentResolver"), STATGROUP_TagContentResolver, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Resolver RebuildRouteCache"), STAT_TagContentResolver_RebuildRouteCache, STATGROUP_TagContentResolver);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadDataTablesForRoots"), STAT_TagContentResolver_PreloadDataTablesForRoots, STATGROUP_TagContentResolver);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadConfiguredCriticalRoutes"), STAT_TagContentResolver_PreloadConfiguredCriticalRoutes, STATGROUP_TagContentResolver);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadAllConfiguredRoutes"), STAT_TagContentResolver_PreloadAllConfiguredRoutes, STATGROUP_TagContentResolver);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadConfiguredRoutesForPolicy"), STAT_TagContentResolver_PreloadConfiguredRoutesForPolicy, STATGROUP_TagContentResolver);

namespace
{
	FString GetLeafSegment(const FString& InTag)
	{
		int32 LastDot = INDEX_NONE;
		if (InTag.FindLastChar(TEXT('.'), LastDot))
		{
			return InTag.Mid(LastDot + 1);
		}
		return InTag;
	}

	TArray<ITagContentResolverRouteProvider*> GetSortedProviders()
	{
		TArray<ITagContentResolverRouteProvider*> Providers;
		FTagContentResolverRouteProviderRegistry::GetProviders(Providers);
		Providers.RemoveAll([](const ITagContentResolverRouteProvider* Provider)
		{
			return Provider == nullptr;
		});
		Algo::Sort(Providers, [](const ITagContentResolverRouteProvider* A, const ITagContentResolverRouteProvider* B)
		{
			if (A->GetProviderPriority() != B->GetProviderPriority())
			{
				return A->GetProviderPriority() > B->GetProviderPriority();
			}
			return A->GetProviderName().LexicalLess(B->GetProviderName());
		});
		return Providers;
	}
}

void UTagContentResolverSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildRouteCache(true);
}

void UTagContentResolverSubsystem::Deinitialize()
{
	bIsRouteConfigurationValid = false;
	CompiledRoutes.Reset();
	RouteByRootTag.Reset();
	ClearResolvedTableCache();
	CachedRowStructToRootTag.Reset();
	AmbiguousRowStructs.Reset();
	CachedMatchedRootByTag.Reset();
	CachedUnresolvedTags.Reset();
	CachedLeafRowNamesByTag.Reset();
	LoggedFailureHashes.Reset();

	Super::Deinitialize();
}

void UTagContentResolverSubsystem::RebuildRouteCache(bool bPreloadConfiguredTables)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TagContentResolver_RebuildRouteCache);
	SCOPE_CYCLE_COUNTER(STAT_TagContentResolver_RebuildRouteCache);

	bIsRouteConfigurationValid = false;
	CompiledRoutes.Reset();
	RouteByRootTag.Reset();
	ClearResolvedTableCache();
	CachedRowStructToRootTag.Reset();
	AmbiguousRowStructs.Reset();
	CachedMatchedRootByTag.Reset();
	CachedUnresolvedTags.Reset();
	CachedLeafRowNamesByTag.Reset();

	FString GatherError;
	TArray<FGameplayTag> CriticalPreloadRoots;
	GatherConfiguredRoutes(CompiledRoutes, GatherError, &CriticalPreloadRoots);
	if (!GatherError.IsEmpty())
	{
		LogFailure(FString::Printf(TEXT("[TagContentResolver] Failed gathering routes: %s"), *GatherError));
	}

	FString ValidateError;
	if (!TryBuildConfiguredRouteMap(CompiledRoutes, RouteByRootTag, ValidateError))
	{
		LogFailure(FString::Printf(TEXT("[TagContentResolver] Invalid route configuration: %s"), *ValidateError));
		return;
	}

	bIsRouteConfigurationValid = true;

	if (!bPreloadConfiguredTables)
	{
		return;
	}

	const UTagContentResolverSettings* Settings = GetDefault<UTagContentResolverSettings>();
	if (!Settings)
	{
		return;
	}

	TArray<FGameplayTag> PreloadRoots;
	switch (Settings->PreloadPolicy)
	{
	case ETagContentResolverPreloadPolicy::AllRoutes:
		for (const FTagContentResolverRoute& Route : CompiledRoutes)
		{
			if (Route.RootTag.IsValid())
			{
				PreloadRoots.Add(Route.RootTag);
			}
		}
		break;
	case ETagContentResolverPreloadPolicy::CriticalRoots:
		PreloadRoots = CriticalPreloadRoots;
		break;
	case ETagContentResolverPreloadPolicy::None:
	default:
		break;
	}

	if (!PreloadRoots.IsEmpty())
	{
		FString PreloadError;
		if (!PreloadDataTablesForRoots(PreloadRoots, PreloadError))
		{
			LogFailure(FString::Printf(TEXT("[TagContentResolver] Preload warning: %s"), *PreloadError));
		}
	}
}

void UTagContentResolverSubsystem::ClearResolvedTableCache()
{
	LoadedTablesByRootTag.Reset();
}

bool UTagContentResolverSubsystem::TryCheckRowExistsForTag(FGameplayTag Tag, FString& OutError)
{
	OutError.Reset();

	UDataTable* DataTable = nullptr;
	FName RowName = NAME_None;
	if (!TryResolveTableAndRowNameForTag(Tag, DataTable, RowName, OutError))
	{
		return false;
	}

	if (!DataTable || !DataTable->GetRowMap().Contains(RowName))
	{
		OutError = FString::Printf(TEXT("Row '%s' not found in DataTable '%s' for tag '%s'."),
			*RowName.ToString(),
			*GetNameSafe(DataTable),
			*Tag.ToString());
		return false;
	}

	return true;
}

bool UTagContentResolverSubsystem::TryValidateRouteConfiguration(FString& OutError)
{
	OutError.Reset();

	TArray<FTagContentResolverRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	return TryValidateRoutes(Routes, OutError);
}

bool UTagContentResolverSubsystem::TryResolveRowForTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError)
{
	OutRow.Reset();
	OutError.Reset();

	FConstStructView RowView;
	if (!TryResolveRowViewForTag(Tag, RowView, OutError))
	{
		return false;
	}

	UScriptStruct* StructType = const_cast<UScriptStruct*>(RowView.GetScriptStruct());
	if (!StructType)
	{
		OutError = FString::Printf(TEXT("Resolved row has no struct type for tag '%s'."), *Tag.ToString());
		return false;
	}

	OutRow.InitializeAs(StructType);
	void* Dest = OutRow.GetMutableMemory();
	check(Dest);
	StructType->CopyScriptStruct(Dest, RowView.GetMemory());
	return true;
}

bool UTagContentResolverSubsystem::TryResolveRowViewForTag(FGameplayTag Tag, FConstStructView& OutRowView, FString& OutError)
{
	OutRowView = FConstStructView();
	OutError.Reset();

	UDataTable* DataTable = nullptr;
	FName RowName = NAME_None;
	if (!TryResolveTableAndRowNameForTag(Tag, DataTable, RowName, OutError))
	{
		return false;
	}

	if (!DataTable)
	{
		OutError = FString::Printf(TEXT("Resolved DataTable is null for tag '%s'."), *Tag.ToString());
		return false;
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		OutError = FString::Printf(TEXT("DataTable '%s' has no row struct for tag '%s'."), *GetNameSafe(DataTable), *Tag.ToString());
		return false;
	}

	const uint8* RowData = DataTable->FindRowUnchecked(RowName);
	if (!RowData)
	{
		const uint8* const* RowPtr = DataTable->GetRowMap().Find(RowName);
		RowData = RowPtr ? *RowPtr : nullptr;
	}

	if (!RowData)
	{
		OutError = FString::Printf(TEXT("Row '%s' could not be resolved in DataTable '%s' for tag '%s'."),
			*RowName.ToString(),
			*GetNameSafe(DataTable),
			*Tag.ToString());
		return false;
	}

	OutRowView = FConstStructView(RowStruct, RowData);
	return true;
}

bool UTagContentResolverSubsystem::TryGetRowNamesForRootTag(FGameplayTag RootTag, TArray<FName>& OutRowNames, FString& OutError)
{
	OutRowNames.Reset();
	OutError.Reset();

	UDataTable* DataTable = nullptr;
	if (!TryResolveDataTableForRootTag(RootTag, DataTable, OutError))
	{
		return false;
	}

	if (!DataTable)
	{
		OutError = FString::Printf(TEXT("Resolved null DataTable for root '%s'."), *RootTag.ToString());
		return false;
	}

	OutRowNames = DataTable->GetRowNames();
	OutRowNames.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	return true;
}

bool UTagContentResolverSubsystem::TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError)
{
	OutDataTable = nullptr;
	OutError.Reset();

	if (!RootTag.IsValid())
	{
		OutError = TEXT("RootTag is invalid.");
		return false;
	}

	if (!bIsRouteConfigurationValid)
	{
		OutError = TEXT("Route configuration is not valid. Call RebuildRouteCache or check startup logs.");
		return false;
	}

	const FTagContentResolverRoute* FoundRoute = RouteByRootTag.Find(RootTag);
	if (!FoundRoute)
	{
		OutError = FString::Printf(TEXT("No route found for root '%s'."), *RootTag.ToString());
		return false;
	}

	if (!TryLoadAndCacheDataTable(RootTag, FoundRoute->DataTable, OutDataTable, OutError))
	{
		return false;
	}

	return true;
}

bool UTagContentResolverSubsystem::TryResolveDataTableForRowStruct(
	UScriptStruct* DesiredRowStruct,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRootTag,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutMatchedRootTag = FGameplayTag();
	OutError.Reset();

	if (!DesiredRowStruct)
	{
		OutError = TEXT("DesiredRowStruct is null.");
		return false;
	}

	if (!bIsRouteConfigurationValid)
	{
		OutError = TEXT("Route configuration is not valid. Call RebuildRouteCache or check startup logs.");
		return false;
	}

	if (AmbiguousRowStructs.Contains(DesiredRowStruct))
	{
		OutError = FString::Printf(TEXT("Ambiguous routes for row struct '%s'."), *GetNameSafe(DesiredRowStruct));
		return false;
	}

	if (const FGameplayTag* CachedRoot = CachedRowStructToRootTag.Find(DesiredRowStruct))
	{
		return TryResolveDataTableForRootTag(*CachedRoot, OutDataTable, OutError)
			? (OutMatchedRootTag = *CachedRoot, true)
			: false;
	}

	int32 MatchCount = 0;
	UDataTable* LastMatchTable = nullptr;
	FGameplayTag LastMatchRoot;

	for (const FTagContentResolverRoute& Route : CompiledRoutes)
	{
		if (!Route.RootTag.IsValid() || Route.DataTable.IsNull())
		{
			continue;
		}

		UDataTable* CandidateTable = nullptr;
		FString LoadError;
		if (!TryLoadAndCacheDataTable(Route.RootTag, Route.DataTable, CandidateTable, LoadError) || !CandidateTable)
		{
			continue;
		}

		if (CandidateTable->GetRowStruct() == DesiredRowStruct)
		{
			++MatchCount;
			LastMatchTable = CandidateTable;
			LastMatchRoot = Route.RootTag;
		}
	}

	if (MatchCount == 0 || !LastMatchTable)
	{
		OutError = FString::Printf(TEXT("No route found for row struct '%s'."), *GetNameSafe(DesiredRowStruct));
		return false;
	}

	if (MatchCount > 1)
	{
		AmbiguousRowStructs.Add(DesiredRowStruct);
		OutError = FString::Printf(TEXT("Ambiguous routes for row struct '%s' (matches=%d)."), *GetNameSafe(DesiredRowStruct), MatchCount);
		return false;
	}

	CachedRowStructToRootTag.Add(DesiredRowStruct, LastMatchRoot);
	OutDataTable = LastMatchTable;
	OutMatchedRootTag = LastMatchRoot;
	return true;
}

bool UTagContentResolverSubsystem::TryResolveRootTagForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRootTag, FString& OutError)
{
	OutMatchedRootTag = FGameplayTag();
	OutError.Reset();

	UDataTable* IgnoredTable = nullptr;
	return TryResolveDataTableAndRootForTag(Tag, IgnoredTable, OutMatchedRootTag, OutError);
}

bool UTagContentResolverSubsystem::PreloadDataTablesForRoots(const TArray<FGameplayTag>& RootTags, FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TagContentResolver_PreloadDataTablesForRoots);
	SCOPE_CYCLE_COUNTER(STAT_TagContentResolver_PreloadDataTablesForRoots);

	OutError.Reset();

	for (const FGameplayTag RootTag : RootTags)
	{
		UDataTable* Table = nullptr;
		FString LocalError;
		if (!TryResolveDataTableForRootTag(RootTag, Table, LocalError))
		{
			if (OutError.IsEmpty())
			{
				OutError = LocalError;
			}
		}
	}

	return OutError.IsEmpty();
}

bool UTagContentResolverSubsystem::PreloadConfiguredCriticalRoutes(FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TagContentResolver_PreloadConfiguredCriticalRoutes);
	SCOPE_CYCLE_COUNTER(STAT_TagContentResolver_PreloadConfiguredCriticalRoutes);

	OutError.Reset();

	TArray<FTagContentResolverRoute> Routes;
	TArray<FGameplayTag> CriticalRoots;
	GatherConfiguredRoutes(Routes, OutError, &CriticalRoots);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	return PreloadDataTablesForRoots(CriticalRoots, OutError);
}

bool UTagContentResolverSubsystem::PreloadAllConfiguredRoutes(FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TagContentResolver_PreloadAllConfiguredRoutes);
	SCOPE_CYCLE_COUNTER(STAT_TagContentResolver_PreloadAllConfiguredRoutes);

	OutError.Reset();

	TArray<FTagContentResolverRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	TArray<FGameplayTag> AllRoots;
	AllRoots.Reserve(Routes.Num());
	for (const FTagContentResolverRoute& Route : Routes)
	{
		if (Route.RootTag.IsValid())
		{
			AllRoots.Add(Route.RootTag);
		}
	}

	return PreloadDataTablesForRoots(AllRoots, OutError);
}

bool UTagContentResolverSubsystem::PreloadConfiguredRoutesForPolicy(FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TagContentResolver_PreloadConfiguredRoutesForPolicy);
	SCOPE_CYCLE_COUNTER(STAT_TagContentResolver_PreloadConfiguredRoutesForPolicy);

	OutError.Reset();

	const UTagContentResolverSettings* Settings = GetDefault<UTagContentResolverSettings>();
	if (!Settings)
	{
		OutError = TEXT("TagContentResolver settings could not be loaded.");
		return false;
	}

	switch (Settings->PreloadPolicy)
	{
	case ETagContentResolverPreloadPolicy::AllRoutes:
		return PreloadAllConfiguredRoutes(OutError);
	case ETagContentResolverPreloadPolicy::CriticalRoots:
		return PreloadConfiguredCriticalRoutes(OutError);
	case ETagContentResolverPreloadPolicy::None:
	default:
		return true;
	}
}

void UTagContentResolverSubsystem::GetResolverDiagnostics(FTagContentResolverDiagnostics& OutDiagnostics) const
{
	OutDiagnostics.bIsConfigurationValid = bIsRouteConfigurationValid;
	OutDiagnostics.RouteCount = CompiledRoutes.Num();
	OutDiagnostics.LoadedTableCount = LoadedTablesByRootTag.Num();
	OutDiagnostics.ResolvedTagCacheCount = CachedMatchedRootByTag.Num();
	OutDiagnostics.UnresolvedTagCacheCount = CachedUnresolvedTags.Num();
	OutDiagnostics.LeafRowCacheCount = CachedLeafRowNamesByTag.Num();
	OutDiagnostics.LoggedFailureCount = LoggedFailureHashes.Num();
}

bool UTagContentResolverSubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
	FGameplayTag RootTag,
	UDataTable*& OutDataTable,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutError.Reset();

	if (!RootTag.IsValid())
	{
		OutError = TEXT("RootTag is invalid.");
		return false;
	}

	TArray<FTagContentResolverRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	TMap<FGameplayTag, FTagContentResolverRoute> RouteMap;
	if (!TryBuildConfiguredRouteMap(Routes, RouteMap, OutError))
	{
		return false;
	}

	const FTagContentResolverRoute* Route = RouteMap.Find(RootTag);
	if (!Route)
	{
		OutError = FString::Printf(TEXT("No route found for root '%s'."), *RootTag.ToString());
		return false;
	}

	if (Route->DataTable.IsNull())
	{
		OutError = FString::Printf(TEXT("Route '%s' has null DataTable reference."), *RootTag.ToString());
		return false;
	}

	OutDataTable = Route->DataTable.LoadSynchronous();
	if (!OutDataTable)
	{
		OutError = FString::Printf(TEXT("Failed to load DataTable for root '%s'."), *RootTag.ToString());
		return false;
	}

	return true;
}

bool UTagContentResolverSubsystem::TryResolveDataTableForRowStructFromConfiguredRoutes(
	UScriptStruct* DesiredRowStruct,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRootTag,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutMatchedRootTag = FGameplayTag();
	OutError.Reset();

	if (!DesiredRowStruct)
	{
		OutError = TEXT("DesiredRowStruct is null.");
		return false;
	}

	TArray<FTagContentResolverRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	FString ValidateError;
	if (!TryValidateRoutes(Routes, ValidateError))
	{
		OutError = ValidateError;
		return false;
	}

	int32 MatchCount = 0;
	for (const FTagContentResolverRoute& Route : Routes)
	{
		if (!Route.RootTag.IsValid() || Route.DataTable.IsNull())
		{
			continue;
		}

		UDataTable* CandidateTable = Route.DataTable.LoadSynchronous();
		if (!CandidateTable || CandidateTable->GetRowStruct() != DesiredRowStruct)
		{
			continue;
		}

		OutDataTable = CandidateTable;
		OutMatchedRootTag = Route.RootTag;
		++MatchCount;
	}

	if (MatchCount == 0 || !OutDataTable)
	{
		OutError = FString::Printf(TEXT("No route found for row struct '%s'."), *GetNameSafe(DesiredRowStruct));
		return false;
	}

	if (MatchCount > 1)
	{
		OutError = FString::Printf(TEXT("Ambiguous routes for row struct '%s' (matches=%d)."), *GetNameSafe(DesiredRowStruct), MatchCount);
		return false;
	}

	return true;
}

void UTagContentResolverSubsystem::GatherConfiguredRoutes(
	TArray<FTagContentResolverRoute>& OutRoutes,
	FString& OutError,
	TArray<FGameplayTag>* OutCriticalPreloadRoots)
{
	OutRoutes.Reset();
	OutError.Reset();
	if (OutCriticalPreloadRoots)
	{
		OutCriticalPreloadRoots->Reset();
	}

	const UTagContentResolverSettings* Settings = GetDefault<UTagContentResolverSettings>();
	if (!Settings)
	{
		OutError = TEXT("TagContentResolver settings could not be loaded.");
		return;
	}

	for (const FTagContentResolverProjectRoute& ProjectRoute : Settings->ProjectRoutes)
	{
		OutRoutes.Add(ProjectRoute.Route);
		if (OutCriticalPreloadRoots && ProjectRoute.bPreload && ProjectRoute.Route.RootTag.IsValid())
		{
			OutCriticalPreloadRoots->Add(ProjectRoute.Route.RootTag);
		}
	}

	const TArray<ITagContentResolverRouteProvider*> Providers = GetSortedProviders();
	for (const ITagContentResolverRouteProvider* Provider : Providers)
	{
		if (!Provider)
		{
			continue;
		}

		TArray<FTagContentResolverRoute> ProviderRoutes;
		Provider->GetProvidedRoutes(ProviderRoutes);
		OutRoutes.Append(ProviderRoutes);
	}
}

bool UTagContentResolverSubsystem::TryValidateRoutes(const TArray<FTagContentResolverRoute>& Routes, FString& OutError)
{
	OutError.Reset();

	if (Routes.IsEmpty())
	{
		OutError = TEXT("Route list is empty.");
		return false;
	}

	TSet<FGameplayTag> SeenRoots;
	for (const FTagContentResolverRoute& Route : Routes)
	{
		if (!Route.RootTag.IsValid())
		{
			OutError = TEXT("Route contains invalid RootTag.");
			return false;
		}

		if (SeenRoots.Contains(Route.RootTag))
		{
			OutError = FString::Printf(TEXT("Duplicate RootTag detected: %s"), *Route.RootTag.ToString());
			return false;
		}

		SeenRoots.Add(Route.RootTag);

		if (Route.DataTable.IsNull())
		{
			OutError = FString::Printf(TEXT("Route '%s' has null DataTable reference."), *Route.RootTag.ToString());
			return false;
		}
	}

	return true;
}

bool UTagContentResolverSubsystem::TryBuildConfiguredRouteMap(
	const TArray<FTagContentResolverRoute>& Routes,
	TMap<FGameplayTag, FTagContentResolverRoute>& OutRouteMap,
	FString& OutError)
{
	OutRouteMap.Reset();
	OutError.Reset();

	if (!TryValidateRoutes(Routes, OutError))
	{
		return false;
	}

	for (const FTagContentResolverRoute& Route : Routes)
	{
		OutRouteMap.Add(Route.RootTag, Route);
	}

	return true;
}

FName UTagContentResolverSubsystem::ExtractLeafRowNameFromTag(FGameplayTag Tag)
{
	if (const FName* Cached = CachedLeafRowNamesByTag.Find(Tag))
	{
		return *Cached;
	}

	const FName LeafName(*GetLeafSegment(Tag.GetTagName().ToString()));
	CachedLeafRowNamesByTag.Add(Tag, LeafName);
	return LeafName;
}

bool UTagContentResolverSubsystem::TryResolveTableAndRowNameForTag(
	FGameplayTag Tag,
	UDataTable*& OutDataTable,
	FName& OutRowName,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutRowName = NAME_None;
	OutError.Reset();

	if (!Tag.IsValid())
	{
		OutError = TEXT("Tag is invalid.");
		return false;
	}

	FGameplayTag MatchedRoot;
	if (!TryResolveDataTableAndRootForTag(Tag, OutDataTable, MatchedRoot, OutError))
	{
		return false;
	}

	OutRowName = ExtractLeafRowNameFromTag(Tag);
	if (OutRowName.IsNone())
	{
		OutError = TEXT("Computed row name is none.");
		return false;
	}

	return true;
}

bool UTagContentResolverSubsystem::TryResolveDataTableAndRootForTag(
	FGameplayTag Tag,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRoot,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutMatchedRoot = FGameplayTag();
	OutError.Reset();

	if (!Tag.IsValid())
	{
		OutError = TEXT("Tag is invalid.");
		return false;
	}

	if (!bIsRouteConfigurationValid)
	{
		OutError = TEXT("Route configuration is not valid. Call RebuildRouteCache or check startup logs.");
		return false;
	}

	if (const FGameplayTag* CachedRoot = CachedMatchedRootByTag.Find(Tag))
	{
		const FTagContentResolverRoute* CachedRoute = RouteByRootTag.Find(*CachedRoot);
		if (CachedRoute)
		{
			OutMatchedRoot = *CachedRoot;
			return TryLoadAndCacheDataTable(*CachedRoot, CachedRoute->DataTable, OutDataTable, OutError);
		}
	}

	if (CachedUnresolvedTags.Contains(Tag))
	{
		OutError = FString::Printf(TEXT("No route found for tag '%s'."), *Tag.ToString());
		return false;
	}

	FGameplayTag Current = Tag;
	while (Current.IsValid())
	{
		if (const FTagContentResolverRoute* Route = RouteByRootTag.Find(Current))
		{
			OutMatchedRoot = Current;
			CachedMatchedRootByTag.Add(Tag, Current);
			return TryLoadAndCacheDataTable(Current, Route->DataTable, OutDataTable, OutError);
		}

		Current = Current.RequestDirectParent();
	}

	CachedUnresolvedTags.Add(Tag);
	OutError = FString::Printf(TEXT("No route found for tag '%s'."), *Tag.ToString());
	return false;
}

bool UTagContentResolverSubsystem::TryLoadAndCacheDataTable(
	const FGameplayTag& RootTag,
	const TSoftObjectPtr<UDataTable>& TableRef,
	UDataTable*& OutDataTable,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutError.Reset();

	if (const TObjectPtr<UDataTable>* CachedTable = LoadedTablesByRootTag.Find(RootTag))
	{
		OutDataTable = CachedTable->Get();
		if (OutDataTable)
		{
			return true;
		}
	}

	if (TableRef.IsNull())
	{
		OutError = FString::Printf(TEXT("Route '%s' has null DataTable reference."), *RootTag.ToString());
		return false;
	}

	UDataTable* LoadedTable = TableRef.LoadSynchronous();
	if (!LoadedTable)
	{
		OutError = FString::Printf(TEXT("Failed to load DataTable for route '%s'."), *RootTag.ToString());
		return false;
	}

	LoadedTablesByRootTag.Add(RootTag, LoadedTable);
	OutDataTable = LoadedTable;
	return true;
}

void UTagContentResolverSubsystem::LogFailure(const FString& Message, ELogVerbosity::Type Verbosity)
{
	const UTagContentResolverSettings* Settings = GetDefault<UTagContentResolverSettings>();
	const bool bDedupe = !Settings || Settings->bDeduplicateFailureLogs;

	if (bDedupe)
	{
		if (Settings && LoggedFailureHashes.Num() >= Settings->MaxRememberedFailureLogs)
		{
			LoggedFailureHashes.Reset();
		}

		const uint32 Hash = GetTypeHash(Message);
		if (LoggedFailureHashes.Contains(Hash))
		{
			return;
		}
		LoggedFailureHashes.Add(Hash);
	}

	switch (Verbosity)
	{
	case ELogVerbosity::Error:
		UE_LOG(LogTagContentResolver, Error, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Verbose:
		UE_LOG(LogTagContentResolver, Verbose, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Display:
		UE_LOG(LogTagContentResolver, Display, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Log:
		UE_LOG(LogTagContentResolver, Log, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Warning:
	default:
		UE_LOG(LogTagContentResolver, Warning, TEXT("%s"), *Message);
		break;
	}
}
