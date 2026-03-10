/**
 * @file ARNetworkRoutingSettings.h
 * @brief Config-backed routing settings for online subsystem selection.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARNetworkRoutingSettings.generated.h"

/**
 * Project-level network routing settings used by UARSessionSubsystem.
 *
 * Internet actions (host/find/join/invite) use InternetSubsystemName first, then optional fallbacks.
 * LAN actions prefer LanSubsystemName first.
 */
UCLASS(Config = Game, DefaultConfig, BlueprintType, meta = (DisplayName = "AR Network Routing"))
class ALIENRAMEN_API UARNetworkRoutingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UARNetworkRoutingSettings();

	/** Preferred online subsystem for internet sessions/invites (default Steam). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Network|Routing")
	FName InternetSubsystemName;

	/** Preferred online subsystem for LAN sessions (default NULL subsystem). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Network|Routing")
	FName LanSubsystemName;

	/** Optional additional subsystem names to try for internet actions after InternetSubsystemName. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Network|Routing")
	TArray<FName> InternetSubsystemFallbackOrder;
};

