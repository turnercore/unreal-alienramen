#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "TagContentResolverTypes.h"
#include "TagContentResolverSubsystem.generated.h"

class UDataTable;
class UScriptStruct;

UCLASS()
class TAGCONTENTRESOLVER_API UTagContentResolverSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	void RebuildRouteCache(bool bPreloadConfiguredTables = true);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	void ClearResolvedTableCache();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryCheckRowExistsForTag(FGameplayTag Tag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryValidateRouteConfiguration(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryResolveRowForTag(FGameplayTag Tag, FInstancedStruct& OutRow, FString& OutError);

	bool TryResolveRowViewForTag(FGameplayTag Tag, FConstStructView& OutRowView, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryGetRowNamesForRootTag(FGameplayTag RootTag, TArray<FName>& OutRowNames, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryResolveDataTableForRowStruct(
		UScriptStruct* DesiredRowStruct,
		UDataTable*& OutDataTable,
		FGameplayTag& OutMatchedRootTag,
		FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool TryResolveRootTagForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRootTag, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool PreloadDataTablesForRoots(const TArray<FGameplayTag>& RootTags, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool PreloadConfiguredCriticalRoutes(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool PreloadAllConfiguredRoutes(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	bool PreloadConfiguredRoutesForPolicy(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Tag Content Resolver")
	void GetResolverDiagnostics(FTagContentResolverDiagnostics& OutDiagnostics) const;

	static bool TryResolveDataTableForRootTagFromConfiguredRoutes(
		FGameplayTag RootTag,
		UDataTable*& OutDataTable,
		FString& OutError);

	static bool TryResolveDataTableForRowStructFromConfiguredRoutes(
		UScriptStruct* DesiredRowStruct,
		UDataTable*& OutDataTable,
		FGameplayTag& OutMatchedRootTag,
		FString& OutError);

private:
	static void GatherConfiguredRoutes(
		TArray<FTagContentResolverRoute>& OutRoutes,
		FString& OutError,
		TArray<FGameplayTag>* OutCriticalPreloadRoots = nullptr);
	static bool TryValidateRoutes(const TArray<FTagContentResolverRoute>& Routes, FString& OutError);
	static bool TryBuildConfiguredRouteMap(const TArray<FTagContentResolverRoute>& Routes, TMap<FGameplayTag, FTagContentResolverRoute>& OutRouteMap, FString& OutError);

	FName ExtractLeafRowNameFromTag(FGameplayTag Tag);

	bool TryResolveTableAndRowNameForTag(FGameplayTag Tag, UDataTable*& OutDataTable, FName& OutRowName, FString& OutError);
	bool TryResolveDataTableAndRootForTag(FGameplayTag Tag, UDataTable*& OutDataTable, FGameplayTag& OutMatchedRoot, FString& OutError);
	bool TryLoadAndCacheDataTable(const FGameplayTag& RootTag, const TSoftObjectPtr<UDataTable>& TableRef, UDataTable*& OutDataTable, FString& OutError);
	void LogFailure(const FString& Message, ELogVerbosity::Type Verbosity = ELogVerbosity::Warning);

	bool bIsRouteConfigurationValid = false;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UDataTable>> LoadedTablesByRootTag;

	UPROPERTY(Transient)
	TArray<FTagContentResolverRoute> CompiledRoutes;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FTagContentResolverRoute> RouteByRootTag;

	TMap<TObjectPtr<UScriptStruct>, FGameplayTag> CachedRowStructToRootTag;
	TSet<TObjectPtr<UScriptStruct>> AmbiguousRowStructs;
	TMap<FGameplayTag, FGameplayTag> CachedMatchedRootByTag;
	TSet<FGameplayTag> CachedUnresolvedTags;
	TMap<FGameplayTag, FName> CachedLeafRowNamesByTag;
	TSet<uint32> LoggedFailureHashes;
};
