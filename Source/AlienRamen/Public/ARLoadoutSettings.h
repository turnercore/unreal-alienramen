#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARLoadoutSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Loadout"))
class ALIENRAMEN_API UARLoadoutSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// Slot roots listed here allow multiple tags at once. Any root not listed is treated as single-slot.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagContainer MultiSlotLoadoutRoots;
};
