/**
 * @file ARFactionSettings.h
 * @brief Faction system settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARFactionSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Factions"))
class ALIENRAMEN_API UARFactionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// Root used for ContentLookup route resolution and row tag reconstruction.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FGameplayTag FactionDefinitionRootTag;
};
