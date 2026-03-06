/**
 * @file ARNetworkUserSettings.h
 * @brief Per-user network runtime settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ARNetworkUserSettings.generated.h"

UCLASS(Config=GameUserSettings, DefaultConfig, BlueprintType)
class ALIENRAMEN_API UARNetworkUserSettings : public UObject
{
	GENERATED_BODY()

public:
	// When true, runtime networking features are blocked (no host/find/join/advertise).
	// Full Steam deactivation may still require restart when Steam was already initialized.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Network")
	bool bStayOffline = false;

	// UI hint: when true, runtime can show a restart recommendation after toggling bStayOffline.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Network")
	bool bShowRestartRecommendedHint = true;
};
