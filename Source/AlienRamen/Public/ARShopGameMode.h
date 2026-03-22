/**
 * @file ARShopGameMode.h
 * @brief ARShopGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARShopRamenTypes.h"
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

	/** Base guaranteed payout per served bowl. Runtime value is mirrored to AARShopGameState for UI. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Economy")
	int32 GetBaseBowlPayout() const { return FMath::Max(0, BaseBowlPayout); }

	/** Returns configured reaction multiplier range used when sampling serve-time tip payouts. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Economy")
	bool GetTipRangeForReaction(EARRamenTasteReaction Reaction, FARShopReactionTipRange& OutRange) const;

	/** Server-authoritative payout calculation used when serving a bowl to a customer. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Economy", meta = (BlueprintAuthorityOnly))
	int32 CalculateServePayout(
		const FARRamenBowlSpec& ServedBowl,
		EARRamenTasteReaction Reaction,
		float& OutAppliedTipMultiplier,
		int32& OutCombinedMeatValue,
		int32& OutBasePayout,
		int32& OutTipPayout);

	/** Vending quality multiplier used in post-run settlement formula. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Economy")
	float GetVendingQualityMultiplier(EARVendingQualityTier QualityTier) const;

	/** Item quality multiplier used when resolving value from meat/item definitions. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Economy")
	float GetItemQualityMultiplier(EARVendingQualityTier QualityTier) const;

	/** Adds one completed vending bowl entry to the save-backed pending ledger for next shop-entry settlement. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Economy", meta = (BlueprintAuthorityOnly))
	bool QueueVendingStockedBowl(const FARVendingStockedBowlEntry& Entry);

protected:
	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual bool PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks) override;

	// Pawn-class overrides keyed by canonical shop character tags (for example Shop.Character.Brother / Shop.Character.Sister).
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

	/** Base guaranteed payout awarded whenever a completed bowl is served. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Base payout applied before tips when serving a completed bowl. Mirrored to shop game state at runtime for UI."))
	int32 BaseBowlPayout = 10;

	/** Hate reaction tip multiplier range sampled at serve-time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Tips", meta = (ToolTip = "Inclusive multiplier range sampled when customer reaction is Hate."))
	FARShopReactionTipRange HateTipMultiplierRange = { 0.0f, 0.1f };

	/** Ok reaction tip multiplier range sampled at serve-time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Tips", meta = (ToolTip = "Inclusive multiplier range sampled when customer reaction is Ok."))
	FARShopReactionTipRange OkTipMultiplierRange = { 0.1f, 0.25f };

	/** Like reaction tip multiplier range sampled at serve-time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Tips", meta = (ToolTip = "Inclusive multiplier range sampled when customer reaction is Like."))
	FARShopReactionTipRange LikeTipMultiplierRange = { 0.4f, 0.6f };

	/** Love reaction tip multiplier range sampled at serve-time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Tips", meta = (ToolTip = "Inclusive multiplier range sampled when customer reaction is Love."))
	FARShopReactionTipRange LoveTipMultiplierRange = { 0.8f, 1.1f };

	/** Vending quality multiplier for low-tier machines. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Vending", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VendingLowQualityMultiplier = 0.2f;

	/** Vending quality multiplier for standard-tier machines. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Vending", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VendingStandardQualityMultiplier = 0.4f;

	/** Vending quality multiplier for high-tier machines. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Vending", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VendingHighQualityMultiplier = 0.6f;

	/** Vending quality multiplier for premium-tier machines. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|Vending", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VendingPremiumQualityMultiplier = 1.0f;

	/** Item quality multiplier for low quality. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|ItemQuality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ItemQualityLowMultiplier = 0.25f;

	/** Item quality multiplier for average/standard quality. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|ItemQuality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ItemQualityStandardMultiplier = 1.0f;

	/** Item quality multiplier for high quality. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|ItemQuality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ItemQualityHighMultiplier = 1.25f;

	/** Item quality multiplier for legendary/premium quality. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Economy|ItemQuality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ItemQualityPremiumMultiplier = 2.0f;

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
	void FinalizePendingVendingPayout(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem, class AARShopGameState* ShopGameState) const;
	int32 ResolveCombinedMeatValue(const FARRamenBowlSpec& BowlSpec) const;
	int32 ResolveVendingBowlPayout(const FARVendingStockedBowlEntry& Entry) const;

	int32 ServeTipRollCounter = 0;
};
