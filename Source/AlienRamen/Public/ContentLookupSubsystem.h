#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "ContentLookupSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FContentLookupRoute
{
	GENERATED_BODY()

	// Example: Unlocks.Ships
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Content Lookup")
	FGameplayTag RootTag;

	// Example: DT_Unlocks_Ships
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Content Lookup")
	TSoftObjectPtr<UDataTable> DataTable;
};

UCLASS(BlueprintType)
class ALIENRAMEN_API UContentLookupRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	// List rather than map for editor friendliness.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Content Lookup")
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
	// Runtime override (optional). If set, this takes priority over project settings RegistryAsset.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Content Lookup")
	TObjectPtr<UContentLookupRegistry> Registry;

	// ---- Cache ----
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UDataTable>> LoadedTables;

	// ---- Blueprint Helpers ----

	// Provide a registry at runtime (e.g., from GameInstance or during testing).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Content Lookup")
	void SetRegistry(UContentLookupRegistry* InRegistry);

	// Clears cached loaded tables (useful during PIE iteration).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Content Lookup")
	void ClearCache();

	// Convenience: checks existence without returning the row struct.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Content Lookup")
	bool DoesRowExistForTag(FGameplayTag Tag, FString& OutError);

	// Validates registry content (duplicate roots, missing tables, etc.)
	// Returns true if it looks usable.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Content Lookup")
	bool ValidateRegistry(FString& OutError);

	// Generic lookup: resolves the route, finds the DataTable row, and returns the row as an InstancedStruct.
	// OutRow will contain a copy of the row struct (type depends on the DataTable's RowStruct).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Content Lookup")
	bool LookupWithGameplayTag(
		FGameplayTag Tag,
		FInstancedStruct& OutRow,
		FString& OutError
	);


private:
	// Returns the leaf segment used as RowName (Unlocks.Ships.Sammy -> Sammy).
	static FName GetLeafRowNameFromTag(FGameplayTag Tag);

	// Core internal helper: resolve matching DataTable and RowName (leaf).
	bool GetTableAndRowNameFromTag(
		FGameplayTag Tag,
		UDataTable*& OutDataTable,
		FName& OutRowName,
		FString& OutError
	);

	// Internal: choose active registry (runtime override first, else loaded asset).
	UContentLookupRegistry* GetActiveRegistry();

	// Internal: resolve table by prefix match.
	UDataTable* ResolveTableForTag(FGameplayTag Tag, FGameplayTag& OutMatchedRoot, FString& OutError);

	// Internal: load a DT (sync) and cache it.
	UDataTable* LoadAndCacheTable(const FGameplayTag& RootTag, const TSoftObjectPtr<UDataTable>& TableRef, FString& OutError);
};
