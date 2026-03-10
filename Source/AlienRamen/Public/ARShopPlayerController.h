/**
 * @file ARShopPlayerController.h
 * @brief ARShopPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARShopPlayerController.generated.h"

class AARShopDispenserActor;

UCLASS()
class ALIENRAMEN_API AARShopPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARShopPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, FGameplayTag ItemTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, FGameplayTag ItemTag);
};
