#include "ARShopPlayerController.h"

#include "ARShopDispenserActor.h"

AARShopPlayerController::AARShopPlayerController()
{
}

void AARShopPlayerController::RequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, const FGameplayTag ItemTag)
{
	if (!DispenserActor)
	{
		return;
	}

	if (HasAuthority())
	{
		DispenserActor->TryDispenseToController(this, ItemTag);
		return;
	}

	ServerRequestShopDispenseFromDispenser(DispenserActor, ItemTag);
}

void AARShopPlayerController::ServerRequestShopDispenseFromDispenser_Implementation(AARShopDispenserActor* DispenserActor, const FGameplayTag ItemTag)
{
	RequestShopDispenseFromDispenser(DispenserActor, ItemTag);
}
