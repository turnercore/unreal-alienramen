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
	// When true, internet/platform session flows are blocked (host/find/join/invite + online PreLogin).
	// LAN/local session flows remain available.
	// Full backend module deactivation may still require restart when a platform OSS module was already initialized.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Network")
	bool bStayOffline = false;

	// UI hint: when true, runtime can show a restart recommendation after toggling bStayOffline.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Network")
	bool bShowRestartRecommendedHint = true;
};
