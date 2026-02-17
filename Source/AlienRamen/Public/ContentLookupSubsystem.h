// ContentLookupSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "ContentLookupSubsystem.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogContentLookup, Log, All);

USTRUCT(BlueprintType)
struct FContentLookupRoute
{
	GENERATED_BODY()

	// Example: Unlocks.Ships
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContentLookup")
	FGameplayTag RootTag;

	// Example: DT_Unlocks_Ships
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContentLookup")
	TSoftObjectPtr<UDataTable> DataTable;
};

UCLASS(BlueprintType)
class ALIENRAMEN_API UContentLookupRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	// List rather than map for editor friendliness.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContentLookup")
	TArray<FContentLookupRoute> Routes;
};

/**
 * ContentLookupSubsystem
 *
 * - Routes a GameplayTag to a DataTable based on RootTag prefix matching.
 * - Uses the tag leaf (last segment after '.') as the DataTable RowName by default.
 *
 * Example:
 *   Unlocks.Ships.Sammy
 *     RootTag: Unlocks.Ships -> DT_Unlocks_Ships
 *     Leaf: Sammy -> RowName 'Sammy'
 */
UCLASS()
class ALIENRAMEN_API UContentLookupSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---- UGameInstanceSubsystem ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Registry Source ----
	// Assign this in defaults (CDO) or set it at runtime via SetRegistry().
	// Keeping it soft helps cooking + load order; we load it in Initialize().
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContentLookup")
	TSoftObjectPtr<UContentLookupRegistry> RegistryAsset;

	// Runtime override (optional). If set, this takes priority over RegistryAsset.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ContentLookup")
	TObjectPtr<UContentLookupRegistry> Registry;

	// ---- Cache ----
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UDataTable>> LoadedTables;

	// ---- Blueprint Helpers ----

	// Provide a registry at runtime (e.g., from GameInstance or during testing).
	UFUNCTION(BlueprintCallable, Category = "ContentLookup")
	void SetRegistry(UContentLookupRegistry* InRegistry);

	// Clears cached loaded tables (useful during PIE iteration).
	UFUNCTION(BlueprintCallable, Category = "ContentLookup")
	void ClearCache();

	// Returns the leaf segment used as RowName (Unlocks.Ships.Sammy -> Sammy).
	UFUNCTION(BlueprintPure, Category = "ContentLookup")
	static FName GetLeafRowNameFromTag(FGameplayTag Tag);

	// Core: Given a tag, returns the matching DataTable and RowName (leaf),
	// plus a readable error if anything fails.
	UFUNCTION(BlueprintCallable, Category = "ContentLookup")
	bool GetTableAndRowNameFromTag(
		FGameplayTag Tag,
		UPARAM(DisplayName = "DataTable") UDataTable*& OutDataTable,
		UPARAM(DisplayName = "RowName") FName& OutRowName,
		FString& OutError
	);

	// Convenience: checks existence without returning the row struct.
	UFUNCTION(BlueprintCallable, Category = "ContentLookup")
	bool DoesRowExistForTag(FGameplayTag Tag, FString& OutError);

	// Validates registry content (duplicate roots, missing tables, etc.)
	// Returns true if it looks usable.
	UFUNCTION(BlueprintCallable, Category = "ContentLookup")
	bool ValidateRegistry(FString& OutError) const;

	// Generic lookup: resolves the route, finds the DataTable row, and returns the row as an InstancedStruct.
	// OutRow will contain a copy of the row struct (type depends on the DataTable's RowStruct).
	UFUNCTION(BlueprintCallable, Category = "ContentLookup")
	bool LookupWithGameplayTag(
		FGameplayTag Tag,
		FInstancedStruct& OutRow,
		FString& OutError
	);


private:
	// Internal: choose active registry (runtime override first, else loaded asset).
	UContentLookupRegistry* GetActiveRegistry() const;

	// Internal: resolve table by prefix match.
	UDataTable* ResolveTableForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRoot, FString& OutError);

	// Internal: load a DT (sync) and cache it.
	UDataTable* LoadAndCacheTable(const FGameplayTag& RootTag, const TSoftObjectPtr<UDataTable>& TableRef, FString& OutError);
};
