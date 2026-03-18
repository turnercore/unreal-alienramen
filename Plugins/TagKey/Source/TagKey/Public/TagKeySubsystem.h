#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "TagKeyTypes.h"
#include "TagKeySubsystem.generated.h"

class UDataTable;
class UScriptStruct;

UCLASS()
class TAGKEY_API UTagKeySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Rebuilds the resolver's internal route map and caches from current Project Settings plus any registered route providers. Use this after changing routes at runtime, enabling/disabling providers, or when you need a clean resolver state. If bPreloadConfiguredTables is true, preloading runs after the rebuild using the active preload policy."))
	void RebuildRouteCache(bool bPreloadConfiguredTables = true);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Clears only loaded DataTable cache entries. Route configuration remains intact. Use this to release loaded table references or force the next lookup to reload tables from soft references without rebuilding route definitions."))
	void ClearResolvedTableCache();

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Checks whether a row can be resolved for a full gameplay tag path. The resolver walks up the tag's parent chain to find the first configured root, then uses the leaf segment as row name. Overlapping roots are rejected by validation. Returns true only when both route/table and row exist. Returns false with OutError describing the failure reason."))
	bool TryCheckRowExistsForTag(FGameplayTag Tag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Validates configured routes without mutating runtime state. Detects invalid root tags, null table references, duplicate roots, and overlapping root hierarchies. Empty route sets are valid but inert. Use this in setup/validation flows to surface configuration issues early."))
	bool TryValidateRouteConfiguration(FString& OutError);

	// Copy-owning resolve: returns an InstancedStruct copy (safe to keep after table unload).
	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Resolves a gameplay tag to a DataTable row and copies that row into an InstancedStruct for Blueprint use. Use this when you need row data payload from a content tag and do not know row struct type at compile time. Returns false with OutError if route/table/row cannot be resolved."))
	bool TryResolveRowStructForTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError);

	// View-only resolve: returns a const struct view into the loaded table (no copy, only valid while table stays loaded).
	// Native-only because FConstStructView is not a Blueprint/UHT-reflected parameter type.
	bool TryResolveRowRefForTag(FGameplayTag Tag, FConstStructView& OutRowView, FString& OutError);

	// Deprecated wrapper: use TryResolveRowStructForTag instead.
	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(DeprecatedFunction, DeprecationMessage="Use TryResolveRowStructForTag", ToolTip="Deprecated wrapper kept for Blueprint migration. Use TryResolveRowStructForTag instead."))
	UE_DEPRECATED(5.3, "Use TryResolveRowStructForTag")
	bool TryResolveRowForTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError) { return TryResolveRowStructForTag(Tag, OutRow, OutError); }

	// Deprecated wrapper: use TryResolveRowRefForTag instead.
	UE_DEPRECATED(5.3, "Use TryResolveRowRefForTag")
	bool TryResolveRowViewForTag(FGameplayTag Tag, FConstStructView& OutRowView, FString& OutError) { return TryResolveRowRefForTag(Tag, OutRowView, OutError); }

	// Deprecated wrapper: use TryResolveRowStructForTag instead.
	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(DeprecatedFunction, DeprecationMessage="Use TryResolveRowStructForTag", ToolTip="Deprecated wrapper kept for Blueprint migration. Use TryResolveRowStructForTag instead."))
	UE_DEPRECATED(5.3, "Use TryResolveRowStructForTag")
	bool TryResolveRowForContentTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError) { return TryResolveRowStructForTag(Tag, OutRow, OutError); }

	// Deprecated wrapper: use TryResolveRowRefForTag instead.
	UE_DEPRECATED(5.3, "Use TryResolveRowRefForTag")
	bool TryResolveRowViewForContentTag(FGameplayTag Tag, FConstStructView& OutRowView, FString& OutError) { return TryResolveRowRefForTag(Tag, OutRowView, OutError); }

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Returns all row names from the DataTable mapped to an exact root tag. Row names are sorted alphabetically. Use this when you want to iterate a family of definitions under one root (for example validation, menus, random selection, or authoring tools)."))
	bool TryGetRowNamesForRootTag(FGameplayTag RootTag, TArray<FName>& OutRowNames, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Resolves and loads the DataTable mapped to an exact root tag. Use this when the caller already knows the root category and wants direct table access. Returns false with OutError if root is not configured or table cannot be loaded."))
	bool TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Resolves a DataTable by row struct type. Useful when you know the row struct class but not the configured root tag. Returns the matched root in OutMatchedRootTag. Fails when no match exists or when multiple routes share the same row struct (ambiguous)."))
	bool TryResolveDataTableForRowStruct(
		UScriptStruct* DesiredRowStruct,
		UDataTable*& OutDataTable,
		FGameplayTag& OutMatchedRootTag,
		FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Resolves which configured root tag would handle a full gameplay tag. This does not return row data; it only tells you the matched route root selected by ancestry walk. Use this to inspect routing decisions, debug tag hierarchies, or build diagnostics."))
	bool TryResolveRootTagForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRootTag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Preloads DataTables for the provided root tag list and stores them in cache. Use this during loading screens or phase transitions to avoid first-lookup sync load hitches. Returns false when one or more roots fail to resolve or load; OutError aggregates all encountered failures."))
	bool PreloadDataTablesForRoots(const TArray<FGameplayTag>& RootTags, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Preloads only project routes marked with bPreload=true. This is the curated startup set for important content that should be warm before gameplay starts. Best used from loading screens or session bootstrap flows."))
	bool PreloadConfiguredCriticalRoutes(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Preloads every configured route table (project routes plus provider routes that resolve through root lookups). Use when you want maximum runtime lookup readiness and can afford higher up-front load cost."))
	bool PreloadAllConfiguredRoutes(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Preloads tables according to Project Settings PreloadPolicy. Never: no preload. Only Routes Marked as Preload: loads routes with bPreload=true. All Routes: loads every route. Use this as the default one-call preload entry point in loading screens."))
	bool PreloadConfiguredRoutesForPolicy(FString& OutError);

	/** Clears the static configured-route cache used by the static routing helpers. Call this after editor-time asset edits when the route path stays the same but the table content/schema changed. */
	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Clears the static configured-route cache used by the static routing helpers. Call this after editor-time asset edits when the route path stays the same but the table content or schema changed."))
	static void ResetConfiguredRouteCache();

	UFUNCTION(BlueprintCallable, Category = "TagKey", meta=(ToolTip="Returns resolver runtime diagnostics, including route count, loaded table cache size, matched/unresolved tag cache sizes, and deduplicated failure log count. Use this for debug UI, telemetry, and performance verification."))
	void GetResolverDiagnostics(FTagKeyDiagnostics& OutDiagnostics) const;

	/** Returns true when a table for the given root is currently loaded in cache (no loading side effects). */
	UFUNCTION(BlueprintPure, Category = "TagKey")
	bool IsRootTableLoaded(FGameplayTag RootTag) const;

	/**
	 * Keeps only the provided roots loaded: unloads any other cached tables, clears dependent caches,
	 * and loads any missing tables for the provided roots.
	 * Returns false if loading any keep-root fails (see OutError).
	 */
	UFUNCTION(BlueprintCallable, Category = "TagKey")
	bool ResetLoadedTablesToExactRoots(const TArray<FGameplayTag>& RootsToKeep, FString& OutError);

	/**
	 * Preloads a root's DataTable and all soft object/class references found in its rows.
	 * Optionally recurses into DataTables referenced by those soft paths (depth-limited).
	 * Recursion only synchronously loads paths identified as DataTables; all gathered soft refs are
	 * then requested through async streaming.
	 * @param RootTag						Resolver root to load.
	 * @param MaxRecursiveTableDepth		How many levels of referenced DataTables to walk (0 = only the root table).
	 * @param MaxAssetsToLoad				Guard rail to prevent runaway recursion; early-outs if exceeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "TagKey")
	bool PreloadRootTableAndSoftReferences(
		FGameplayTag RootTag,
		int32 MaxRecursiveTableDepth,
		int32 MaxAssetsToLoad,
		FString& OutError);

	/** Static configured-route resolver for exact root tags. Must be called on the game thread. */
	static bool TryResolveDataTableForRootTagFromConfiguredRoutes(
		FGameplayTag RootTag,
		UDataTable*& OutDataTable,
		FString& OutError);

	/** Static configured-route resolver for row struct matching. Must be called on the game thread. */
	static bool TryResolveDataTableForRowStructFromConfiguredRoutes(
		UScriptStruct* DesiredRowStruct,
		UDataTable*& OutDataTable,
		FGameplayTag& OutMatchedRootTag,
		FString& OutError);

private:
	static void GatherConfiguredRoutes(
		TArray<FTagKeyRoute>& OutRoutes,
		FString& OutError,
		TArray<FGameplayTag>* OutCriticalPreloadRoots = nullptr);
	static bool TryValidateRoutes(const TArray<FTagKeyRoute>& Routes, FString& OutError);
	static bool TryBuildConfiguredRouteMap(const TArray<FTagKeyRoute>& Routes, TMap<FGameplayTag, FTagKeyRoute>& OutRouteMap, FString& OutError);
	void ResetRuntimeCaches(bool bResetLoggedFailures);
	bool EnsureGameThread(const TCHAR* FunctionName, FString* OutError = nullptr) const;
	bool EnsureRouteCacheFresh(FString& OutError);

	FName ExtractLeafRowNameFromTag(FGameplayTag Tag);

	bool TryResolveTableAndRowNameForTag(FGameplayTag Tag, UDataTable*& OutDataTable, FName& OutRowName, FString& OutError);
	bool TryResolveDataTableAndRootForTag(FGameplayTag Tag, UDataTable*& OutDataTable, FGameplayTag& OutMatchedRoot, FString& OutError);
	bool TryLoadAndCacheDataTable(const FGameplayTag& RootTag, const TSoftObjectPtr<UDataTable>& TableRef, UDataTable*& OutDataTable, FString& OutError);
	void TryPreloadSoftReferencesForRow(const UScriptStruct* RowStruct, const uint8* RowData) const;
	void LogFailure(const FString& Message, ELogVerbosity::Type Verbosity = ELogVerbosity::Warning);

	bool bIsRouteConfigurationValid = false;
	uint64 RouteProviderGenerationAtBuild = 0;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UDataTable>> LoadedTablesByRootTag;

	UPROPERTY(Transient)
	TArray<FTagKeyRoute> CompiledRoutes;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FTagKeyRoute> RouteByRootTag;

	TMap<TObjectPtr<UScriptStruct>, FGameplayTag> CachedRowStructToRootTag;
	TSet<TObjectPtr<UScriptStruct>> AmbiguousRowStructs;
	TMap<FGameplayTag, FGameplayTag> CachedMatchedRootByTag;
	TSet<FGameplayTag> CachedUnresolvedTags;
	TMap<FGameplayTag, FName> CachedLeafRowNamesByTag;
	TSet<FString> LoggedFailureMessages;
};
