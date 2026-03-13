#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TagContentResolverTypes.h"
#include "TagContentResolverSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Tag Content Resolver"))
class TAGCONTENTRESOLVER_API UTagContentResolverSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Routes", meta=(ToolTip="Project-owned routes. Other plugins can still contribute additional routes via providers. Each entry maps a root tag (e.g., Dialogue.Speaker) to a DataTable soft reference."))
	TArray<FTagContentResolverProjectRoute> ProjectRoutes;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Routes", meta=(ToolTip="Controls which route tables are preloaded at subsystem startup. Never = none, Only Routes Marked as Preload = project routes with bPreload=true, All Routes = every configured route."))
	ETagContentResolverPreloadPolicy PreloadPolicy = ETagContentResolverPreloadPolicy::CriticalRoots;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Async Loading", meta=(ToolTip="When enabled, successful row resolves request async loads for unresolved soft object/class references found in the resolved row data. Keeps future resolves hitch-free at the cost of background IO."))
	bool bAutoPreloadRowSoftReferences = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta=(ToolTip="When enabled, repeated identical resolver failures are logged once to reduce spam."))
	bool bDeduplicateFailureLogs = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta=(ClampMin="16", UIMin="16", ToolTip="Maximum number of unique failure messages remembered for deduplication before cache reset."))
	int32 MaxRememberedFailureLogs = 256;
};
