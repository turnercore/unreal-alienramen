#include "ARPlayerCharacterScrapyard.h"

#include "ARShopCarryComponent.h"

AARPlayerCharacterScrapyard::AARPlayerCharacterScrapyard()
{
	ScrapyardCarryComponent = CreateDefaultSubobject<UARShopCarryComponent>(TEXT("ScrapyardCarryComponent"));
}

bool AARPlayerCharacterScrapyard::IsCarryingScrapyardItem() const
{
	return ScrapyardCarryComponent && ScrapyardCarryComponent->HasHeldActor();
}

AActor* AARPlayerCharacterScrapyard::GetHeldScrapyardActor() const
{
	return ScrapyardCarryComponent ? ScrapyardCarryComponent->GetHeldActor() : nullptr;
}
