#include "TagKeySubsystem.h"

#include "Algo/Sort.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "TagKeyLog.h"
#include "TagKeyProvider.h"
#include "TagKeySettings.h"
#include "UObject/UnrealType.h"

DECLARE_STATS_GROUP(TEXT("TagKey"), STATGROUP_TagKey, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Resolver RebuildRouteCache"), STAT_TagKey_RebuildRouteCache, STATGROUP_TagKey);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadDataTablesForRoots"), STAT_TagKey_PreloadDataTablesForRoots, STATGROUP_TagKey);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadConfiguredCriticalRoutes"), STAT_TagKey_PreloadConfiguredCriticalRoutes, STATGROUP_TagKey);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadAllConfiguredRoutes"), STAT_TagKey_PreloadAllConfiguredRoutes, STATGROUP_TagKey);
DECLARE_CYCLE_STAT(TEXT("Resolver PreloadConfiguredRoutesForPolicy"), STAT_TagKey_PreloadConfiguredRoutesForPolicy, STATGROUP_TagKey);

namespace
{
	struct FConfiguredRouteResolutionCache
	{
		FCriticalSection Mutex;
		uint64 ProviderGeneration = UINT64_MAX;
		uint32 RoutesFingerprint = 0;
		bool bValidRouteMap = false;
		FString LastRouteError;
		TMap<FGameplayTag, FTagKeyRoute> RouteByRootTag;
		TMap<FGameplayTag, TObjectPtr<UDataTable>> LoadedTablesByRootTag;
		TMap<TObjectPtr<UScriptStruct>, FGameplayTag> CachedRowStructToRootTag;
		TSet<TObjectPtr<UScriptStruct>> AmbiguousRowStructs;
	};

	FConfiguredRouteResolutionCache GConfiguredRouteCache;

	bool EnsureTagKeyGameThread(const TCHAR* FunctionName, FString* OutError = nullptr)
	{
		if (IsInGameThread())
		{
			return true;
		}

		const FString Message = FString::Printf(
			TEXT("[TagKey] %s must be called on the game thread."),
			FunctionName ? FunctionName : TEXT("Resolver API"));
		if (GIsAutomationTesting)
		{
			UE_LOG(LogTagKey, Warning, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG(LogTagKey, Error, TEXT("%s"), *Message);
			ensureAlwaysMsgf(false, TEXT("%s"), *Message);
		}

		if (OutError)
		{
			*OutError = Message;
		}

		return false;
	}

	bool IsDataTableSoftPath(const FSoftObjectPath& Path)
	{
		if (Path.IsNull())
		{
			return false;
		}

		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(Path);
		return AssetData.IsValid() &&
			AssetData.AssetClassPath == UDataTable::StaticClass()->GetClassPathName();
	}

	FString GetLeafSegment(const FString& InTag)
	{
		int32 LastDot = INDEX_NONE;
		if (InTag.FindLastChar(TEXT('.'), LastDot))
		{
			return InTag.Mid(LastDot + 1);
		}
		return InTag;
	}

	TArray<ITagKeyRouteProvider*> GetSortedProviders()
	{
		TArray<ITagKeyRouteProvider*> Providers;
		FTagKeyRouteProviderRegistry::GetProviders(Providers);
		Providers.RemoveAll([](const ITagKeyRouteProvider* Provider)
		{
			return Provider == nullptr;
		});
		Algo::Sort(Providers, [](const ITagKeyRouteProvider* A, const ITagKeyRouteProvider* B)
		{
			if (A->GetProviderPriority() != B->GetProviderPriority())
			{
				return A->GetProviderPriority() > B->GetProviderPriority();
			}
			return A->GetProviderName().LexicalLess(B->GetProviderName());
		});
		return Providers;
	}

	uint32 ComputeRoutesFingerprint(const TArray<FTagKeyRoute>& Routes)
	{
		uint32 Hash = 1469598103u;
		for (const FTagKeyRoute& Route : Routes)
		{
			Hash = HashCombine(Hash, GetTypeHash(Route.RootTag));
			Hash = HashCombine(Hash, GetTypeHash(Route.DataTable.ToSoftObjectPath()));
		}
		return Hash;
	}

	bool TryLoadAndCacheConfiguredDataTable(
		const FGameplayTag& RootTag,
		const TSoftObjectPtr<UDataTable>& TableRef,
		FConfiguredRouteResolutionCache& Cache,
		UDataTable*& OutDataTable,
		FString& OutError)
	{
		OutDataTable = nullptr;
		OutError.Reset();

		if (const TObjectPtr<UDataTable>* CachedTable = Cache.LoadedTablesByRootTag.Find(RootTag))
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

		Cache.LoadedTablesByRootTag.Add(RootTag, LoadedTable);
		OutDataTable = LoadedTable;
		return true;
	}

	void GatherSoftObjectPathsFromPropertyValue(const FProperty* Property, const void* ValuePtr, TSet<FSoftObjectPath>& OutPaths);

	void GatherSoftObjectPathsFromStruct(const UStruct* StructType, const void* StructMemory, TSet<FSoftObjectPath>& OutPaths)
	{
		if (!StructType || !StructMemory)
		{
			return;
		}

		for (TFieldIterator<const FProperty> PropertyIt(StructType); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (!Property)
			{
				continue;
			}

			for (int32 ValueIndex = 0; ValueIndex < Property->ArrayDim; ++ValueIndex)
			{
				const void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(StructMemory, ValueIndex);
				GatherSoftObjectPathsFromPropertyValue(Property, PropertyValuePtr, OutPaths);
			}
		}
	}

	void GatherSoftObjectPathsFromPropertyValue(const FProperty* Property, const void* ValuePtr, TSet<FSoftObjectPath>& OutPaths)
	{
		if (!Property || !ValuePtr)
		{
			return;
		}

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPtr SoftObjectPtr = SoftObjectProperty->GetPropertyValue(ValuePtr);
			const FSoftObjectPath SoftObjectPath = SoftObjectPtr.ToSoftObjectPath();
			if (!SoftObjectPtr.IsNull() && !SoftObjectPtr.IsValid() && !SoftObjectPath.IsNull())
			{
				OutPaths.Add(SoftObjectPath);
			}
			return;
		}

		if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property))
		{
			const FSoftObjectPtr SoftClassPtr = SoftClassProperty->GetPropertyValue(ValuePtr);
			const FSoftObjectPath SoftClassPath = SoftClassPtr.ToSoftObjectPath();
			if (!SoftClassPtr.IsNull() && !SoftClassPtr.IsValid() && !SoftClassPath.IsNull())
			{
				OutPaths.Add(SoftClassPath);
			}
			return;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			GatherSoftObjectPathsFromStruct(StructProperty->Struct, ValuePtr, OutPaths);
			return;
		}

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			const int32 ElementCount = ArrayHelper.Num();
			for (int32 Index = 0; Index < ElementCount; ++Index)
			{
				const void* ElementValuePtr = ArrayHelper.GetRawPtr(Index);
				GatherSoftObjectPathsFromPropertyValue(ArrayProperty->Inner, ElementValuePtr, OutPaths);
			}
			return;
		}

		if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper SetHelper(SetProperty, ValuePtr);
			const int32 MaxIndex = SetHelper.GetMaxIndex();
			for (int32 Index = 0; Index < MaxIndex; ++Index)
			{
				if (!SetHelper.IsValidIndex(Index))
				{
					continue;
				}

				const void* ElementValuePtr = SetHelper.GetElementPtr(Index);
				GatherSoftObjectPathsFromPropertyValue(SetProperty->ElementProp, ElementValuePtr, OutPaths);
			}
			return;
		}

		if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper MapHelper(MapProperty, ValuePtr);
			const int32 MaxIndex = MapHelper.GetMaxIndex();
			for (int32 Index = 0; Index < MaxIndex; ++Index)
			{
				if (!MapHelper.IsValidIndex(Index))
				{
					continue;
				}

				const void* KeyValuePtr = MapHelper.GetKeyPtr(Index);
				const void* MappedValuePtr = MapHelper.GetValuePtr(Index);
				GatherSoftObjectPathsFromPropertyValue(MapProperty->KeyProp, KeyValuePtr, OutPaths);
				GatherSoftObjectPathsFromPropertyValue(MapProperty->ValueProp, MappedValuePtr, OutPaths);
			}
		}
	}

	void RequestAsyncPreloadForPaths(const TSet<FSoftObjectPath>& PathsToPreload)
	{
		if (PathsToPreload.IsEmpty())
		{
			return;
		}

		TArray<FSoftObjectPath> Paths = PathsToPreload.Array();
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Paths,
			FStreamableDelegate(),
			FStreamableManager::DefaultAsyncLoadPriority,
			true);
	}
}

void UTagKeySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildRouteCache(true);
}

void UTagKeySubsystem::ResetRuntimeCaches(bool bResetLoggedFailures)
{
	bIsRouteConfigurationValid = false;
	RouteProviderGenerationAtBuild = FTagKeyRouteProviderRegistry::GetProviderGeneration();
	CompiledRoutes.Reset();
	RouteByRootTag.Reset();
	ClearResolvedTableCache();
	CachedRowStructToRootTag.Reset();
	AmbiguousRowStructs.Reset();
	CachedMatchedRootByTag.Reset();
	CachedUnresolvedTags.Reset();
	CachedLeafRowNamesByTag.Reset();
	if (bResetLoggedFailures)
	{
		LoggedFailureMessages.Reset();
	}
}

bool UTagKeySubsystem::EnsureGameThread(const TCHAR* FunctionName, FString* OutError) const
{
	return EnsureTagKeyGameThread(FunctionName, OutError);
}

bool UTagKeySubsystem::EnsureRouteCacheFresh(FString& OutError)
{
	const uint64 CurrentProviderGeneration = FTagKeyRouteProviderRegistry::GetProviderGeneration();
	if (bIsRouteConfigurationValid && CurrentProviderGeneration == RouteProviderGenerationAtBuild)
	{
		return true;
	}

	RebuildRouteCache(false);
	if (!bIsRouteConfigurationValid)
	{
		OutError = TEXT("Route configuration is not valid. Call RebuildRouteCache or check startup logs.");
		return false;
	}

	return true;
}

void UTagKeySubsystem::Deinitialize()
{
	ResetRuntimeCaches(true);
	Super::Deinitialize();
}

void UTagKeySubsystem::RebuildRouteCache(bool bPreloadConfiguredTables)
{
	if (!EnsureGameThread(TEXT("RebuildRouteCache"), nullptr))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(TagKey_RebuildRouteCache);
	SCOPE_CYCLE_COUNTER(STAT_TagKey_RebuildRouteCache);

	ResetRuntimeCaches(false);

	FString GatherError;
	TArray<FGameplayTag> CriticalPreloadRoots;
	GatherConfiguredRoutes(CompiledRoutes, GatherError, &CriticalPreloadRoots);
	if (!GatherError.IsEmpty())
	{
		LogFailure(FString::Printf(TEXT("[TagKey] Failed gathering routes: %s"), *GatherError));
	}

	FString ValidateError;
	if (!TryBuildConfiguredRouteMap(CompiledRoutes, RouteByRootTag, ValidateError))
	{
		LogFailure(FString::Printf(TEXT("[TagKey] Invalid route configuration: %s"), *ValidateError));
		return;
	}

	bIsRouteConfigurationValid = true;
	RouteProviderGenerationAtBuild = FTagKeyRouteProviderRegistry::GetProviderGeneration();

	if (!bPreloadConfiguredTables)
	{
		return;
	}

	const UTagKeySettings* Settings = GetDefault<UTagKeySettings>();
	if (!Settings)
	{
		return;
	}

	TArray<FGameplayTag> PreloadRoots;
	switch (Settings->PreloadPolicy)
	{
	case ETagKeyPreloadPolicy::AllRoutes:
		for (const FTagKeyRoute& Route : CompiledRoutes)
		{
			if (Route.RootTag.IsValid())
			{
				PreloadRoots.Add(Route.RootTag);
			}
		}
		break;
	case ETagKeyPreloadPolicy::CriticalRoots:
		PreloadRoots = CriticalPreloadRoots;
		break;
	case ETagKeyPreloadPolicy::None:
	default:
		break;
	}

	if (!PreloadRoots.IsEmpty())
	{
		FString PreloadError;
		if (!PreloadDataTablesForRoots(PreloadRoots, PreloadError))
		{
			LogFailure(FString::Printf(TEXT("[TagKey] Preload warning: %s"), *PreloadError));
		}
	}
}

void UTagKeySubsystem::ClearResolvedTableCache()
{
	if (!EnsureGameThread(TEXT("ClearResolvedTableCache"), nullptr))
	{
		return;
	}

	LoadedTablesByRootTag.Reset();
}

bool UTagKeySubsystem::TryCheckRowExistsForTag(FGameplayTag Tag, FString& OutError)
{
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryCheckRowExistsForTag"), &OutError))
	{
		return false;
	}

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

bool UTagKeySubsystem::TryValidateRouteConfiguration(FString& OutError)
{
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryValidateRouteConfiguration"), &OutError))
	{
		return false;
	}

	TArray<FTagKeyRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	return TryValidateRoutes(Routes, OutError);
}

bool UTagKeySubsystem::TryResolveRowStructForTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError)
{
	OutRow.Reset();
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryResolveRowStructForTag"), &OutError))
	{
		return false;
	}

	FConstStructView RowView;
	if (!TryResolveRowRefForTag(Tag, RowView, OutError))
	{
		return false;
	}

	const UScriptStruct* StructType = RowView.GetScriptStruct();
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

bool UTagKeySubsystem::TryResolveRowRefForTag(FGameplayTag Tag, FConstStructView& OutRowView, FString& OutError)
{
	OutRowView = FConstStructView();
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryResolveRowRefForTag"), &OutError))
	{
		return false;
	}

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
		OutError = FString::Printf(TEXT("Row '%s' could not be resolved in DataTable '%s' for tag '%s'."),
			*RowName.ToString(),
			*GetNameSafe(DataTable),
			*Tag.ToString());
		return false;
	}

	TryPreloadSoftReferencesForRow(RowStruct, RowData);
	OutRowView = FConstStructView(RowStruct, RowData);
	return true;
}

void UTagKeySubsystem::TryPreloadSoftReferencesForRow(const UScriptStruct* RowStruct, const uint8* RowData) const
{
	if (!RowStruct || !RowData)
	{
		return;
	}

	const UTagKeySettings* Settings = GetDefault<UTagKeySettings>();
	if (!Settings || !Settings->bAutoPreloadRowSoftReferences)
	{
		return;
	}

	TSet<FSoftObjectPath> PathsToPreload;
	GatherSoftObjectPathsFromStruct(RowStruct, RowData, PathsToPreload);
	RequestAsyncPreloadForPaths(PathsToPreload);
}

bool UTagKeySubsystem::TryGetRowNamesForRootTag(FGameplayTag RootTag, TArray<FName>& OutRowNames, FString& OutError)
{
	OutRowNames.Reset();
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryGetRowNamesForRootTag"), &OutError))
	{
		return false;
	}

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

bool UTagKeySubsystem::TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError)
{
	OutDataTable = nullptr;
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryResolveDataTableForRootTag"), &OutError))
	{
		return false;
	}

	if (!RootTag.IsValid())
	{
		OutError = TEXT("RootTag is invalid.");
		return false;
	}

	if (!EnsureRouteCacheFresh(OutError))
	{
		return false;
	}

	const FTagKeyRoute* FoundRoute = RouteByRootTag.Find(RootTag);
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

bool UTagKeySubsystem::TryResolveDataTableForRowStruct(
	UScriptStruct* DesiredRowStruct,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRootTag,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutMatchedRootTag = FGameplayTag();
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryResolveDataTableForRowStruct"), &OutError))
	{
		return false;
	}

	if (!DesiredRowStruct)
	{
		OutError = TEXT("DesiredRowStruct is null.");
		return false;
	}

	if (!EnsureRouteCacheFresh(OutError))
	{
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

	for (const FTagKeyRoute& Route : CompiledRoutes)
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

bool UTagKeySubsystem::TryResolveRootTagForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRootTag, FString& OutError)
{
	OutMatchedRootTag = FGameplayTag();
	OutError.Reset();
	if (!EnsureGameThread(TEXT("TryResolveRootTagForTag"), &OutError))
	{
		return false;
	}

	UDataTable* IgnoredTable = nullptr;
	return TryResolveDataTableAndRootForTag(Tag, IgnoredTable, OutMatchedRootTag, OutError);
}

bool UTagKeySubsystem::PreloadDataTablesForRoots(const TArray<FGameplayTag>& RootTags, FString& OutError)
{
	if (!EnsureGameThread(TEXT("PreloadDataTablesForRoots"), &OutError))
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(TagKey_PreloadDataTablesForRoots);
	SCOPE_CYCLE_COUNTER(STAT_TagKey_PreloadDataTablesForRoots);

	OutError.Reset();

	for (const FGameplayTag& RootTag : RootTags)
	{
		UDataTable* Table = nullptr;
		FString LocalError;
		if (!TryResolveDataTableForRootTag(RootTag, Table, LocalError))
		{
			if (!LocalError.IsEmpty())
			{
				if (!OutError.IsEmpty())
				{
					OutError.Append(TEXT("\n"));
				}
				OutError.Append(LocalError);
			}
		}
	}

	return OutError.IsEmpty();
}

bool UTagKeySubsystem::PreloadConfiguredCriticalRoutes(FString& OutError)
{
	if (!EnsureGameThread(TEXT("PreloadConfiguredCriticalRoutes"), &OutError))
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(TagKey_PreloadConfiguredCriticalRoutes);
	SCOPE_CYCLE_COUNTER(STAT_TagKey_PreloadConfiguredCriticalRoutes);

	OutError.Reset();

	TArray<FTagKeyRoute> Routes;
	TArray<FGameplayTag> CriticalRoots;
	GatherConfiguredRoutes(Routes, OutError, &CriticalRoots);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	return PreloadDataTablesForRoots(CriticalRoots, OutError);
}

bool UTagKeySubsystem::PreloadAllConfiguredRoutes(FString& OutError)
{
	if (!EnsureGameThread(TEXT("PreloadAllConfiguredRoutes"), &OutError))
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(TagKey_PreloadAllConfiguredRoutes);
	SCOPE_CYCLE_COUNTER(STAT_TagKey_PreloadAllConfiguredRoutes);

	OutError.Reset();

	TArray<FTagKeyRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	TArray<FGameplayTag> AllRoots;
	AllRoots.Reserve(Routes.Num());
	for (const FTagKeyRoute& Route : Routes)
	{
		if (Route.RootTag.IsValid())
		{
			AllRoots.Add(Route.RootTag);
		}
	}

	return PreloadDataTablesForRoots(AllRoots, OutError);
}

bool UTagKeySubsystem::PreloadConfiguredRoutesForPolicy(FString& OutError)
{
	if (!EnsureGameThread(TEXT("PreloadConfiguredRoutesForPolicy"), &OutError))
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(TagKey_PreloadConfiguredRoutesForPolicy);
	SCOPE_CYCLE_COUNTER(STAT_TagKey_PreloadConfiguredRoutesForPolicy);

	OutError.Reset();

	const UTagKeySettings* Settings = GetDefault<UTagKeySettings>();
	if (!Settings)
	{
		OutError = TEXT("TagKey settings could not be loaded.");
		return false;
	}

	switch (Settings->PreloadPolicy)
	{
	case ETagKeyPreloadPolicy::AllRoutes:
		return PreloadAllConfiguredRoutes(OutError);
	case ETagKeyPreloadPolicy::CriticalRoots:
		return PreloadConfiguredCriticalRoutes(OutError);
	case ETagKeyPreloadPolicy::None:
	default:
		return true;
	}
}

void UTagKeySubsystem::GetResolverDiagnostics(FTagKeyDiagnostics& OutDiagnostics) const
{
	if (!EnsureGameThread(TEXT("GetResolverDiagnostics"), nullptr))
	{
		OutDiagnostics = FTagKeyDiagnostics();
		return;
	}

	OutDiagnostics.bIsConfigurationValid = bIsRouteConfigurationValid;
	OutDiagnostics.RouteCount = CompiledRoutes.Num();
	OutDiagnostics.LoadedTableCount = LoadedTablesByRootTag.Num();
	OutDiagnostics.ResolvedTagCacheCount = CachedMatchedRootByTag.Num();
	OutDiagnostics.UnresolvedTagCacheCount = CachedUnresolvedTags.Num();
	OutDiagnostics.LeafRowCacheCount = CachedLeafRowNamesByTag.Num();
	OutDiagnostics.LoggedFailureCount = LoggedFailureMessages.Num();
}

bool UTagKeySubsystem::IsRootTableLoaded(FGameplayTag RootTag) const
{
	return LoadedTablesByRootTag.Contains(RootTag);
}

bool UTagKeySubsystem::ResetLoadedTablesToExactRoots(const TArray<FGameplayTag>& RootsToKeep, FString& OutError)
{
	OutError.Reset();
	if (!EnsureGameThread(TEXT("ResetLoadedTablesToExactRoots"), &OutError))
	{
		return false;
	}

	// Build keep set of valid roots.
	TSet<FGameplayTag> KeepSet;
	for (const FGameplayTag& Root : RootsToKeep)
	{
		if (Root.IsValid())
		{
			KeepSet.Add(Root);
		}
	}

	// Remove any loaded tables not in the keep set.
	TArray<FGameplayTag> LoadedRoots;
	LoadedTablesByRootTag.GetKeys(LoadedRoots);
	for (const FGameplayTag& LoadedRoot : LoadedRoots)
	{
		if (!KeepSet.Contains(LoadedRoot))
		{
			LoadedTablesByRootTag.Remove(LoadedRoot);
		}
	}

	// Clear caches that may reference unloaded tables; routes remain intact.
	CachedMatchedRootByTag.Reset();
	CachedLeafRowNamesByTag.Reset();

	// Load any keep-roots that are not currently cached.
	TArray<FGameplayTag> MissingRoots;
	for (const FGameplayTag& Root : KeepSet)
	{
		if (!LoadedTablesByRootTag.Contains(Root))
		{
			MissingRoots.Add(Root);
		}
	}

	if (!MissingRoots.IsEmpty())
	{
		if (!PreloadDataTablesForRoots(MissingRoots, OutError))
		{
			return false;
		}
	}

	return true;
}

namespace
{
	void GatherSoftObjectPathsFromStruct(const UStruct* StructType, const void* StructMemory, TSet<FSoftObjectPath>& OutPaths);
	void GatherSoftObjectPathsFromPropertyValue(const FProperty* Property, const void* ValuePtr, TSet<FSoftObjectPath>& OutPaths);

	void GatherSoftObjectPathsFromObject(const UObject* Object, TSet<FSoftObjectPath>& OutPaths)
	{
		if (!Object)
		{
			return;
		}

		for (TFieldIterator<const FProperty> PropertyIt(Object->GetClass()); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (!Property)
			{
				continue;
			}
			const void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
			GatherSoftObjectPathsFromPropertyValue(Property, PropertyValuePtr, OutPaths);
		}
	}
}

bool UTagKeySubsystem::PreloadRootTableAndSoftReferences(
	FGameplayTag RootTag,
	int32 MaxRecursiveTableDepth,
	int32 MaxAssetsToLoad,
	FString& OutError)
{
	OutError.Reset();
	if (!EnsureGameThread(TEXT("PreloadRootTableAndSoftReferences"), &OutError))
	{
		return false;
	}

	if (!EnsureRouteCacheFresh(OutError))
	{
		return false;
	}

	if (MaxAssetsToLoad <= 0)
	{
		OutError = TEXT("MaxAssetsToLoad must be > 0.");
		return false;
	}

	if (MaxRecursiveTableDepth < 0)
	{
		OutError = TEXT("MaxRecursiveTableDepth must be >= 0.");
		return false;
	}

	// Load root table (will cache it).
	UDataTable* RootTable = nullptr;
	if (!TryResolveDataTableForRootTag(RootTag, RootTable, OutError) || !RootTable)
	{
		return false;
	}

	TSet<FSoftObjectPath> PathsToLoad;
	TSet<FSoftObjectPath> VisitedTables;
	TSet<FSoftObjectPath> VisitedAssets;

	TFunction<bool(UDataTable*, int32)> GatherTableSoftRefsRecursive =
		[&](UDataTable* Table, int32 DepthRemaining) -> bool
	{
		if (!Table || DepthRemaining < 0)
		{
			return true;
		}

		for (const auto& Pair : Table->GetRowMap())
		{
			const uint8* RowData = reinterpret_cast<const uint8*>(Pair.Value);
			GatherSoftObjectPathsFromStruct(Table->GetRowStruct(), RowData, PathsToLoad);
		}

		if (DepthRemaining == 0)
		{
			return true;
		}

		// Optionally recurse into DataTables referenced by soft paths.
		TArray<FSoftObjectPath> CurrentPaths = PathsToLoad.Array();
		for (const FSoftObjectPath& Path : CurrentPaths)
		{
			if (Path.IsNull())
			{
				continue;
			}

			if (VisitedAssets.Contains(Path))
			{
				continue;
			}
			VisitedAssets.Add(Path);

			if (VisitedAssets.Num() > MaxAssetsToLoad)
			{
				OutError = TEXT("PreloadRootTableAndSoftReferences aborted: MaxAssetsToLoad exceeded.");
				return false;
			}

			if (!IsDataTableSoftPath(Path))
			{
				continue;
			}

			UDataTable* SubTable = Cast<UDataTable>(Path.ResolveObject());
			if (!SubTable)
			{
				SubTable = Cast<UDataTable>(Path.TryLoad());
			}
			if (SubTable)
			{
				// Only walk each DataTable once.
				const FSoftObjectPath SubTablePath(SubTable);
				if (!VisitedTables.Contains(SubTablePath))
				{
					VisitedTables.Add(SubTablePath);
					if (!GatherTableSoftRefsRecursive(SubTable, DepthRemaining - 1))
					{
						return false;
					}
				}
			}
		}

		return true;
	};

	VisitedTables.Add(FSoftObjectPath(RootTable));
	if (!GatherTableSoftRefsRecursive(RootTable, MaxRecursiveTableDepth))
	{
		return false;
	}

	if (!PathsToLoad.IsEmpty())
	{
		// Preload all gathered assets asynchronously (non-blocking for soft refs).
		TArray<FSoftObjectPath> Paths = PathsToLoad.Array();
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Paths,
			FStreamableDelegate(),
			FStreamableManager::DefaultAsyncLoadPriority,
			true);
	}

	return true;
}
bool UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(
	FGameplayTag RootTag,
	UDataTable*& OutDataTable,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutError.Reset();
	if (!EnsureTagKeyGameThread(TEXT("TryResolveDataTableForRootTagFromConfiguredRoutes"), &OutError))
	{
		return false;
	}

	if (!RootTag.IsValid())
	{
		OutError = TEXT("RootTag is invalid.");
		return false;
	}

	TArray<FTagKeyRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	const uint64 ProviderGeneration = FTagKeyRouteProviderRegistry::GetProviderGeneration();
	const uint32 RoutesFingerprint = ComputeRoutesFingerprint(Routes);

	FScopeLock Lock(&GConfiguredRouteCache.Mutex);
	const bool bNeedsRebuild =
		!GConfiguredRouteCache.bValidRouteMap ||
		GConfiguredRouteCache.ProviderGeneration != ProviderGeneration ||
		GConfiguredRouteCache.RoutesFingerprint != RoutesFingerprint;
	if (bNeedsRebuild)
	{
		GConfiguredRouteCache.RouteByRootTag.Reset();
		GConfiguredRouteCache.LoadedTablesByRootTag.Reset();
		GConfiguredRouteCache.CachedRowStructToRootTag.Reset();
		GConfiguredRouteCache.AmbiguousRowStructs.Reset();
		if (!TryBuildConfiguredRouteMap(Routes, GConfiguredRouteCache.RouteByRootTag, GConfiguredRouteCache.LastRouteError))
		{
			GConfiguredRouteCache.bValidRouteMap = false;
			OutError = GConfiguredRouteCache.LastRouteError;
			return false;
		}

		GConfiguredRouteCache.ProviderGeneration = ProviderGeneration;
		GConfiguredRouteCache.RoutesFingerprint = RoutesFingerprint;
		GConfiguredRouteCache.LastRouteError.Reset();
		GConfiguredRouteCache.bValidRouteMap = true;
	}

	const FTagKeyRoute* Route = GConfiguredRouteCache.RouteByRootTag.Find(RootTag);
	if (!Route)
	{
		OutError = FString::Printf(TEXT("No route found for root '%s'."), *RootTag.ToString());
		return false;
	}

	return TryLoadAndCacheConfiguredDataTable(
		RootTag,
		Route->DataTable,
		GConfiguredRouteCache,
		OutDataTable,
		OutError);
}

bool UTagKeySubsystem::TryResolveDataTableForRowStructFromConfiguredRoutes(
	UScriptStruct* DesiredRowStruct,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRootTag,
	FString& OutError)
{
	OutDataTable = nullptr;
	OutMatchedRootTag = FGameplayTag();
	OutError.Reset();
	if (!EnsureTagKeyGameThread(TEXT("TryResolveDataTableForRowStructFromConfiguredRoutes"), &OutError))
	{
		return false;
	}

	if (!DesiredRowStruct)
	{
		OutError = TEXT("DesiredRowStruct is null.");
		return false;
	}

	TArray<FTagKeyRoute> Routes;
	GatherConfiguredRoutes(Routes, OutError, nullptr);
	if (!OutError.IsEmpty())
	{
		return false;
	}

	const uint64 ProviderGeneration = FTagKeyRouteProviderRegistry::GetProviderGeneration();
	const uint32 RoutesFingerprint = ComputeRoutesFingerprint(Routes);

	FScopeLock Lock(&GConfiguredRouteCache.Mutex);
	const bool bNeedsRebuild =
		!GConfiguredRouteCache.bValidRouteMap ||
		GConfiguredRouteCache.ProviderGeneration != ProviderGeneration ||
		GConfiguredRouteCache.RoutesFingerprint != RoutesFingerprint;
	if (bNeedsRebuild)
	{
		GConfiguredRouteCache.RouteByRootTag.Reset();
		GConfiguredRouteCache.LoadedTablesByRootTag.Reset();
		GConfiguredRouteCache.CachedRowStructToRootTag.Reset();
		GConfiguredRouteCache.AmbiguousRowStructs.Reset();
		if (!TryBuildConfiguredRouteMap(Routes, GConfiguredRouteCache.RouteByRootTag, GConfiguredRouteCache.LastRouteError))
		{
			GConfiguredRouteCache.bValidRouteMap = false;
			OutError = GConfiguredRouteCache.LastRouteError;
			return false;
		}

		GConfiguredRouteCache.ProviderGeneration = ProviderGeneration;
		GConfiguredRouteCache.RoutesFingerprint = RoutesFingerprint;
		GConfiguredRouteCache.LastRouteError.Reset();
		GConfiguredRouteCache.bValidRouteMap = true;
	}

	if (GConfiguredRouteCache.AmbiguousRowStructs.Contains(DesiredRowStruct))
	{
		OutError = FString::Printf(TEXT("Ambiguous routes for row struct '%s'."), *GetNameSafe(DesiredRowStruct));
		return false;
	}

	if (const FGameplayTag* CachedRoot = GConfiguredRouteCache.CachedRowStructToRootTag.Find(DesiredRowStruct))
	{
		const FTagKeyRoute* CachedRoute = GConfiguredRouteCache.RouteByRootTag.Find(*CachedRoot);
		if (!CachedRoute)
		{
			OutError = FString::Printf(TEXT("No route found for row struct '%s'."), *GetNameSafe(DesiredRowStruct));
			return false;
		}

		if (!TryLoadAndCacheConfiguredDataTable(
			*CachedRoot,
			CachedRoute->DataTable,
			GConfiguredRouteCache,
			OutDataTable,
			OutError))
		{
			return false;
		}

		OutMatchedRootTag = *CachedRoot;
		return true;
	}

	int32 MatchCount = 0;
	FGameplayTag LastMatchRoot;
	UDataTable* LastMatchTable = nullptr;
	for (const TPair<FGameplayTag, FTagKeyRoute>& Pair : GConfiguredRouteCache.RouteByRootTag)
	{
		const FTagKeyRoute& Route = Pair.Value;
		UDataTable* CandidateTable = nullptr;
		FString LoadError;
		if (!TryLoadAndCacheConfiguredDataTable(
			Route.RootTag,
			Route.DataTable,
			GConfiguredRouteCache,
			CandidateTable,
			LoadError))
		{
			continue;
		}

		if (CandidateTable && CandidateTable->GetRowStruct() == DesiredRowStruct)
		{
			++MatchCount;
			LastMatchRoot = Route.RootTag;
			LastMatchTable = CandidateTable;
		}
	}

	if (MatchCount == 0 || !LastMatchTable)
	{
		OutError = FString::Printf(TEXT("No route found for row struct '%s'."), *GetNameSafe(DesiredRowStruct));
		return false;
	}

	if (MatchCount > 1)
	{
		GConfiguredRouteCache.AmbiguousRowStructs.Add(DesiredRowStruct);
		OutError = FString::Printf(TEXT("Ambiguous routes for row struct '%s' (matches=%d)."), *GetNameSafe(DesiredRowStruct), MatchCount);
		return false;
	}

	GConfiguredRouteCache.CachedRowStructToRootTag.Add(DesiredRowStruct, LastMatchRoot);
	OutMatchedRootTag = LastMatchRoot;
	OutDataTable = LastMatchTable;
	return true;
}

void UTagKeySubsystem::GatherConfiguredRoutes(
	TArray<FTagKeyRoute>& OutRoutes,
	FString& OutError,
	TArray<FGameplayTag>* OutCriticalPreloadRoots)
{
	OutRoutes.Reset();
	OutError.Reset();
	if (OutCriticalPreloadRoots)
	{
		OutCriticalPreloadRoots->Reset();
	}

	const UTagKeySettings* Settings = GetDefault<UTagKeySettings>();
	if (!Settings)
	{
		OutError = TEXT("TagKey settings could not be loaded.");
		return;
	}

	for (const FTagKeyProjectRoute& ProjectRoute : Settings->ProjectRoutes)
	{
		FTagKeyRoute Route;
		Route.RootTag = ProjectRoute.RootTag;
		Route.DataTable = ProjectRoute.DataTable;
		OutRoutes.Add(Route);

		if (OutCriticalPreloadRoots && ProjectRoute.bPreload && ProjectRoute.RootTag.IsValid())
		{
			OutCriticalPreloadRoots->Add(ProjectRoute.RootTag);
		}
	}

	const TArray<ITagKeyRouteProvider*> Providers = GetSortedProviders();
	for (const ITagKeyRouteProvider* Provider : Providers)
	{
		if (!Provider)
		{
			continue;
		}

		TArray<FTagKeyRoute> ProviderRoutes;
		Provider->GetProvidedRoutes(ProviderRoutes);
		OutRoutes.Append(ProviderRoutes);
	}
}

bool UTagKeySubsystem::TryValidateRoutes(const TArray<FTagKeyRoute>& Routes, FString& OutError)
{
	OutError.Reset();

	if (Routes.IsEmpty())
	{
		OutError = TEXT("Route list is empty.");
		return false;
	}

	TSet<FGameplayTag> SeenRoots;
	for (const FTagKeyRoute& Route : Routes)
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

	for (int32 IndexA = 0; IndexA < Routes.Num(); ++IndexA)
	{
		const FGameplayTag TagA = Routes[IndexA].RootTag;
		for (int32 IndexB = IndexA + 1; IndexB < Routes.Num(); ++IndexB)
		{
			const FGameplayTag TagB = Routes[IndexB].RootTag;
			if (TagA.MatchesTag(TagB) || TagB.MatchesTag(TagA))
			{
				OutError = FString::Printf(
					TEXT("Overlapping RootTag hierarchy detected: '%s' and '%s'."),
					*TagA.ToString(),
					*TagB.ToString());
				return false;
			}
		}
	}

	return true;
}

bool UTagKeySubsystem::TryBuildConfiguredRouteMap(
	const TArray<FTagKeyRoute>& Routes,
	TMap<FGameplayTag, FTagKeyRoute>& OutRouteMap,
	FString& OutError)
{
	OutRouteMap.Reset();
	OutError.Reset();

	if (!TryValidateRoutes(Routes, OutError))
	{
		return false;
	}

	for (const FTagKeyRoute& Route : Routes)
	{
		OutRouteMap.Add(Route.RootTag, Route);
	}

	return true;
}

FName UTagKeySubsystem::ExtractLeafRowNameFromTag(FGameplayTag Tag)
{
	if (const FName* Cached = CachedLeafRowNamesByTag.Find(Tag))
	{
		return *Cached;
	}

	const FName LeafName(*GetLeafSegment(Tag.GetTagName().ToString()));
	CachedLeafRowNamesByTag.Add(Tag, LeafName);
	return LeafName;
}

bool UTagKeySubsystem::TryResolveTableAndRowNameForTag(
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

bool UTagKeySubsystem::TryResolveDataTableAndRootForTag(
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

	if (!EnsureRouteCacheFresh(OutError))
	{
		return false;
	}

	if (const FGameplayTag* CachedRoot = CachedMatchedRootByTag.Find(Tag))
	{
		const FTagKeyRoute* CachedRoute = RouteByRootTag.Find(*CachedRoot);
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
		if (const FTagKeyRoute* Route = RouteByRootTag.Find(Current))
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

bool UTagKeySubsystem::TryLoadAndCacheDataTable(
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

void UTagKeySubsystem::LogFailure(const FString& Message, ELogVerbosity::Type Verbosity)
{
	const UTagKeySettings* Settings = GetDefault<UTagKeySettings>();
	const bool bDedupe = !Settings || Settings->bDeduplicateFailureLogs;

	if (bDedupe)
	{
		if (Settings && LoggedFailureMessages.Num() >= Settings->MaxRememberedFailureLogs)
		{
			LoggedFailureMessages.Reset();
		}

		if (LoggedFailureMessages.Contains(Message))
		{
			return;
		}
		LoggedFailureMessages.Add(Message);
	}

	switch (Verbosity)
	{
	case ELogVerbosity::Error:
		UE_LOG(LogTagKey, Error, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Verbose:
		UE_LOG(LogTagKey, Verbose, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Display:
		UE_LOG(LogTagKey, Display, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Log:
		UE_LOG(LogTagKey, Log, TEXT("%s"), *Message);
		break;
	case ELogVerbosity::Warning:
	default:
		UE_LOG(LogTagKey, Warning, TEXT("%s"), *Message);
		break;
	}
}
