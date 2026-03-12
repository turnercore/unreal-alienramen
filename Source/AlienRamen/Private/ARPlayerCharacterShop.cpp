#include "ARPlayerCharacterShop.h"

#include "ARShopCarryComponent.h"

AARPlayerCharacterShop::AARPlayerCharacterShop()
{
	ShopCarryComponent = CreateDefaultSubobject<UARShopCarryComponent>(TEXT("ShopCarryComponent"));
}

bool AARPlayerCharacterShop::IsCarryingShopItem() const
{
	return ShopCarryComponent && ShopCarryComponent->HasHeldActor();
}

AActor* AARPlayerCharacterShop::GetHeldShopActor() const
{
	return ShopCarryComponent ? ShopCarryComponent->GetHeldActor() : nullptr;
}
