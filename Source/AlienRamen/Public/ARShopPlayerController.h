/**
 * @file ARShopPlayerController.h
 * @brief ARShopPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARShopPlayerController.generated.h"

class AARShopDispenserActor;
class AARShopCarryItemBase;
class AActor;

UCLASS()
class ALIENRAMEN_API AARShopPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARShopPlayerController();

	// Generic one-shot shop interact path:
	// - valid target: routes to target ForwardUseToController(UsingActor=this) when available
	// - null target: drops currently held carry item (if any)
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopUseOrDrop(AActor* InteractableActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopUseOrDrop(AActor* InteractableActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, FGameplayTag ItemTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, FGameplayTag ItemTag);

	// Picks up a world carry item (for example meat/bowl) into this controller's pawn carry slot.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopPickupCarryItem(AARShopCarryItemBase* CarryItemActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopPickupCarryItem(AARShopCarryItemBase* CarryItemActor);

	// Drops currently held carry item to world physics.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopDropHeldCarryItem();

	UFUNCTION(Server, Reliable)
	void ServerRequestShopDropHeldCarryItem();

	// Throws currently held carry item using physics impulse.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopThrowHeldCarryItem(float ThrowStrength = 900.0f);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopThrowHeldCarryItem(float ThrowStrength = 900.0f);
};
