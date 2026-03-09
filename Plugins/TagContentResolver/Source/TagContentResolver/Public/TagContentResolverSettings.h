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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Routes", meta=(ToolTip="Project-owned routes. Other plugins can still contribute additional routes via providers."))
	TArray<FTagContentResolverProjectRoute> ProjectRoutes;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Performance", meta=(ToolTip="Controls which DataTables are preloaded at subsystem startup."))
	ETagContentResolverPreloadPolicy PreloadPolicy = ETagContentResolverPreloadPolicy::None;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta=(ToolTip="When enabled, repeated identical resolver failures are logged once to reduce spam."))
	bool bDeduplicateFailureLogs = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta=(ClampMin="16", UIMin="16", ToolTip="Maximum number of unique failure messages remembered for deduplication before cache reset."))
	int32 MaxRememberedFailureLogs = 256;
};
