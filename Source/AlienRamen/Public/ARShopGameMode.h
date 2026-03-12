/**
 * @file ARShopGameMode.h
 * @brief ARShopGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARShopGameMode.generated.h"

class AAREnergyDrinkCarryItem;
class UARSaveGame;
class UARSaveSubsystem;

UCLASS()
class ALIENRAMEN_API AARShopGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARShopGameMode();

protected:
	virtual void BeginPlay() override;
	virtual bool PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink")
	FName EnergyDrinkSpawnAnchorActorTag = TEXT("Shop.EnergyDrink.Spawn");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EnergyDrinkStackedSpawnZOffset = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink")
	TSubclassOf<AAREnergyDrinkCarryItem> FallbackEnergyDrinkCarryItemClass;

private:
	bool RestoreTransientShopCarryables(UARSaveGame* SaveGame) const;
	bool SpawnStoredEnergyDrinksAtAnchors(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem) const;
	void ClearShopTransientCarryablesForRunStart(UARSaveSubsystem* SaveSubsystem) const;
};
