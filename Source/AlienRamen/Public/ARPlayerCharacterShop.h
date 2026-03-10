/**
 * @file ARPlayerCharacterShop.h
 * @brief ARPlayerCharacterShop header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerCharacterBase.h"
#include "ARPlayerCharacterShop.generated.h"

class UARShopCarryComponent;

UCLASS()
class ALIENRAMEN_API AARPlayerCharacterShop : public AARPlayerCharacterBase
{
	GENERATED_BODY()

public:
	AARPlayerCharacterShop();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry")
	UARShopCarryComponent* GetShopCarryComponent() const { return ShopCarryComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry")
	TObjectPtr<UARShopCarryComponent> ShopCarryComponent;
};
