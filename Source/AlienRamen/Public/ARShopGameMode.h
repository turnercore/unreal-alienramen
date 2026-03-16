/**
 * @file ARShopGameMode.h
 * @brief ARShopGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARShopGameMode.generated.h"

class AAREnergyDrinkCarryItem;
class AARRamenBowlActor;
class UARShopCarryComponent;
class UARSaveGame;
class UARSaveSubsystem;
class AController;
class APawn;
struct FARCharacterHeldShopItemSnapshot;

UCLASS()
class ALIENRAMEN_API AARShopGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARShopGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual bool PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks) override;

	// Pawn-class overrides keyed by canonical shop character tags (for example Shop.Character.Brother / Shop.Character.Sister).
	// Legacy Parley/Customer keys are normalized at runtime, but new content should only author Shop.Character.* keys.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Spawn")
	TMap<FGameplayTag, TSubclassOf<APawn>> ShopPawnClassByCharacterTag;

	// Fallback pawn class used when no character-tag override is configured.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Spawn")
	TSubclassOf<APawn> FallbackShopPawnClass;

	// Actor tag used to find shop anchor actors where stored energy drinks should materialize on shop entry.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink", meta = (ToolTip = "Actor tag used to find energy-drink spawn anchors in the shop."))
	FName EnergyDrinkSpawnAnchorActorTag = TEXT("Shop.EnergyDrink.Spawn");

	// Vertical spacing applied when multiple stored drinks stack on the same anchor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Vertical spacing used when multiple stored drinks stack on the same spawn anchor."))
	float EnergyDrinkStackedSpawnZOffset = 12.0f;

	// Fallback actor class used when an item definition does not provide a more specific energy-drink actor class.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink", meta = (ToolTip = "Fallback class to spawn when a stored energy drink does not resolve a specific actor class from item definitions."))
	TSubclassOf<AAREnergyDrinkCarryItem> FallbackEnergyDrinkCarryItemClass;

private:
	bool ShouldApplyFreshLoadCharacterRestore(const UARSaveSubsystem* SaveSubsystem, const UARSaveGame* SaveGame) const;
	bool TryRestoreFreshLoadCharacterStates(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem) const;
	bool TryRestoreCharacterShopStateForController(AController* Controller, UARSaveGame* SaveGame) const;
	bool RestoreTransientShopCarryables(UARSaveGame* SaveGame) const;
	bool RestoreHeldShopItemSnapshot(class UARShopCarryComponent* CarryComponent, const FARCharacterHeldShopItemSnapshot& Snapshot) const;
	bool RestoreBowlSnapshot(class AARRamenBowlActor* BowlActor, const FARCharacterHeldShopItemSnapshot& Snapshot) const;
	bool SpawnStoredEnergyDrinksAtAnchors(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem) const;
	void ClearShopTransientCarryablesForRunStart(UARSaveSubsystem* SaveSubsystem) const;
	bool ShouldPersistCanonicalShopEntry(const UARSaveGame* SaveGame) const;
	void PersistCanonicalShopEntryIfNeeded(UARSaveSubsystem* SaveSubsystem, const UARSaveGame* SaveGame) const;
};
