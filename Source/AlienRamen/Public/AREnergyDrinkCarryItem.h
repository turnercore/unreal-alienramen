/**
 * @file AREnergyDrinkCarryItem.h
 * @brief Replicated carryable energy drink actor consumed in shop mode.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARShopCarryItemBase.h"
#include "AREnergyDrinkCarryItem.generated.h"

class AARShopPlayerController;
class AARPlayerController;

UCLASS(Blueprintable)
class ALIENRAMEN_API AAREnergyDrinkCarryItem : public AARShopCarryItemBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Energy Drink")
	FGameplayTag GetEnergyDrinkItemTag() const { return EnergyDrinkItemTag; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Energy Drink", meta = (BlueprintAuthorityOnly))
	void SetEnergyDrinkItemTag(FGameplayTag NewItemTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Energy Drink", meta = (BlueprintAuthorityOnly))
	bool TryConsumeFromController(AARShopPlayerController* ShopController);

protected:
	virtual bool UseSecondaryByController_Implementation(AARPlayerController* UsingController) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Energy Drink")
	FGameplayTag EnergyDrinkItemTag;
};
