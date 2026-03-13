#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "TagContentResolverTypes.generated.h"

class UDataTable;

USTRUCT(BlueprintType)
struct TAGCONTENTRESOLVER_API FTagContentResolverRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Root gameplay tag prefix this route handles. Example: Dialogue.Speaker"))
	FGameplayTag RootTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="DataTable used when a tag resolves to this RootTag route."))
	TSoftObjectPtr<UDataTable> DataTable;
};

USTRUCT(BlueprintType)
struct TAGCONTENTRESOLVER_API FTagContentResolverProjectRoute
{
	GENERATED_BODY()

	/** Root gameplay tag prefix this route handles. Example: Dialogue.Speaker */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Root gameplay tag prefix this route handles. Example: Dialogue.Speaker"))
	FGameplayTag RootTag;

	/** When true, this route is preloaded when PreloadPolicy = CriticalRoots. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="When PreloadPolicy is set to Critical Routes, this route's table is loaded during subsystem startup."))
	bool bPreload = false;

	/** DataTable soft reference resolved for this route. Row names are expected to match tag leaf strings. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="DataTable used when a tag resolves to this RootTag route."))
	TSoftObjectPtr<UDataTable> DataTable;
};

UENUM(BlueprintType)
enum class ETagContentResolverPreloadPolicy : uint8
{
	None UMETA(DisplayName = "Never", ToolTip = "Do not preload tables at startup. Tables load on first use."),
	CriticalRoots UMETA(DisplayName = "Only Routes Marked as Preload", ToolTip = "Preload only routes in ProjectRoutes with bPreload enabled."),
	AllRoutes UMETA(DisplayName = "All Routes", ToolTip = "Preload all configured routes at startup.")
};

USTRUCT(BlueprintType)
struct TAGCONTENTRESOLVER_API FTagContentResolverDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="True when the current resolver configuration is valid (routes parsed, no conflicts)."))
	bool bIsConfigurationValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Number of active routes after configuration/provider merge."))
	int32 RouteCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="How many DataTables are currently loaded in cache."))
	int32 LoadedTableCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Cached successes: tags already resolved to a route/row name."))
	int32 ResolvedTagCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Cached failures: tags that previously failed to resolve."))
	int32 UnresolvedTagCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Leaf row name cache size (tag -> row name lookups)."))
	int32 LeafRowCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tag Content Resolver", meta=(ToolTip="Count of unique failure messages remembered when deduplication is enabled."))
	int32 LoggedFailureCount = 0;
};
