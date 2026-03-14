/**
 * @file ParleyFactionSettings.h
 * @brief Faction system settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ParleyFactionSettings.generated.h"

UCLASS(Config=Parley, DefaultConfig, meta=(DisplayName="Factions"))
class PARLEY_API UParleyFactionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Factions"); }

	// Root used for TagContentResolver route resolution and row tag reconstruction.
	// Example: Faction.Definition (route rows to build candidate lists).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction", meta = (ToolTip = "Root faction definition tag used for TagContentResolver lookups."))
	FGameplayTag FactionDefinitionRootTag;
};
