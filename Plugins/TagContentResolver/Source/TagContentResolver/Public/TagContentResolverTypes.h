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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver", meta=(ToolTip="Root gameplay tag prefix this route handles. Example: Dialogue.Speaker"))
	FGameplayTag RootTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver", meta=(ToolTip="DataTable used when a tag resolves to this RootTag route."))
	TSoftObjectPtr<UDataTable> DataTable;
};

USTRUCT(BlueprintType)
struct TAGCONTENTRESOLVER_API FTagContentResolverProjectRoute
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver", meta=(ShowOnlyInnerProperties, ToolTip="Route definition stored in project settings."))
	FTagContentResolverRoute Route;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver", meta=(ToolTip="When PreloadPolicy is set to Critical Routes, this route's table is loaded during subsystem startup."))
	bool bPreload = false;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	bool bIsConfigurationValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	int32 RouteCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	int32 LoadedTableCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	int32 ResolvedTagCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	int32 UnresolvedTagCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	int32 LeafRowCacheCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Tag Content Resolver")
	int32 LoggedFailureCount = 0;
};
