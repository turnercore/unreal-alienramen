#include "ARPlayerCharacterShop.h"

#include "ARShopCarryComponent.h"

AARPlayerCharacterShop::AARPlayerCharacterShop()
{
	ShopCarryComponent = CreateDefaultSubobject<UARShopCarryComponent>(TEXT("ShopCarryComponent"));
}
