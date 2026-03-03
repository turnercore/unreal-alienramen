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

	// Applied to new/empty player loadouts (for example first-join setup and empty-hydration recovery).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	FGameplayTagContainer DefaultPlayerLoadoutTags;

	// Baseline unlocks for new saves and empty-unlock hydration paths.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	FGameplayTagContainer DefaultStartingUnlocks;

	// Returns starting unlocks with any missing default loadout tags auto-included.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Loadout")
	FGameplayTagContainer GetEffectiveDefaultStartingUnlocks() const;

	// Returns true when all default loadout tags are already present in starting unlocks.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Loadout")
	bool AreDefaultTagsConsistent() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	static int32 MergeMissingLoadoutTagsIntoUnlocks(FGameplayTagContainer& InOutUnlocks, const FGameplayTagContainer& InLoadoutTags);
};
