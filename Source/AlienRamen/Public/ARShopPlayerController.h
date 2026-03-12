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
class AARShopStationActor;
class AAREnergyDrinkCarryItem;
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
	void RequestShopStationPlaceHeldMeat(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopStationPlaceHeldMeat(AARShopStationActor* StationActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopStationPickupMeat(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopStationPickupMeat(AARShopStationActor* StationActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopStationStartProcessing(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopStationStartProcessing(AARShopStationActor* StationActor);

	// Tap-processing entrypoint. Each call advances station processing progress by station tap amount.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopStationTapProcessing(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopStationTapProcessing(AARShopStationActor* StationActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopStationStopProcessing(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopStationStopProcessing(AARShopStationActor* StationActor);

	// Smart station interact helper:
	// - held bowl -> fill bowl from station
	// - held meat and empty station meat slot -> place held meat
	// - empty hands and station has slotted meat -> pickup slotted meat
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopStationInteract(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopStationInteract(AARShopStationActor* StationActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopFillHeldBowlFromStation(AARShopStationActor* StationActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopFillHeldBowlFromStation(AARShopStationActor* StationActor);

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
	// ThrowStrength <= 0 uses thrower Strength attribute mapping (Strength * 100.0f).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestShopThrowHeldCarryItem(float ThrowStrength = -1.0f);

	UFUNCTION(Server, Reliable)
	void ServerRequestShopThrowHeldCarryItem(float ThrowStrength = -1.0f);

	// Consumes held energy drink in shop mode. Returns false/no-op if held actor is not a drink.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Interaction")
	void RequestConsumeHeldEnergyDrink();

	UFUNCTION(Server, Reliable)
	void ServerRequestConsumeHeldEnergyDrink();
};
