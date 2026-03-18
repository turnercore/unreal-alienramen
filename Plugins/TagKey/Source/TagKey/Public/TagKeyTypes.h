#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "TagKeyTypes.generated.h"

class UDataTable;

USTRUCT(BlueprintType)
struct TAGKEY_API FTagKeyRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Root gameplay tag prefix this route handles. Example: Dialogue.Speaker"))
	FGameplayTag RootTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="DataTable used when a tag resolves to this RootTag route."))
	TSoftObjectPtr<UDataTable> DataTable;
};

USTRUCT(BlueprintType)
struct TAGKEY_API FTagKeyProjectRoute
{
	GENERATED_BODY()

	/** Root gameplay tag prefix this route handles. Example: Dialogue.Speaker. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Root gameplay tag prefix this route handles. Example: Dialogue.Speaker"))
	FGameplayTag RootTag;

	/** When true, this route is included in explicit critical-route preload requests. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="When PreloadPolicy is set to Only Routes Marked as Preload, this route is included in the critical-route preload set."))
	bool bPreload = false;

	/** DataTable soft reference resolved for this route. Row names are expected to match tag leaf strings. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="DataTable used when a tag resolves to this RootTag route."))
	TSoftObjectPtr<UDataTable> DataTable;
};

UENUM(BlueprintType)
enum class ETagKeyPreloadPolicy : uint8
{
	None UMETA(DisplayName = "Never", ToolTip = "Do not preload tables automatically. Tables load on first use or through explicit preload calls."),
	CriticalRoots UMETA(DisplayName = "Only Routes Marked as Preload", ToolTip = "Preload only routes in ProjectRoutes with bPreload enabled when a preload pass is requested."),
	AllRoutes UMETA(DisplayName = "All Routes", ToolTip = "Preload all configured routes when a preload pass is requested.")
};

USTRUCT(BlueprintType)
struct TAGKEY_API FTagKeyDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="True when the current resolver configuration is valid (routes parsed, no conflicts)."))
	bool bIsConfigurationValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Number of active routes after configuration/provider merge."))
	int32 RouteCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="How many DataTables are currently loaded in cache."))
	int32 LoadedTableCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Cached successes: tags already resolved to a route/row name."))
	int32 ResolvedTagCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Cached failures: tags that previously failed to resolve."))
	int32 UnresolvedTagCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Leaf row name cache size (tag -> row name lookups)."))
	int32 LeafRowCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TagKey", meta=(ToolTip="Count of unique failure messages remembered when deduplication is enabled."))
	int32 LoggedFailureCount = 0;
};
