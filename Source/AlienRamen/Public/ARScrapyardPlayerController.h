/**
 * @file ARScrapyardPlayerController.h
 * @brief ARScrapyardPlayerController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerController.h"
#include "ARScrapyardPlayerController.generated.h"

class AARScrapyardCarryItemBase;
class AARScrapyardExitZoneActor;

UCLASS()
class ALIENRAMEN_API AARScrapyardPlayerController : public AARPlayerController
{
	GENERATED_BODY()

public:
	AARScrapyardPlayerController();

	/** Authority-routed pickup request for scrapyard carryables in reach. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Interaction")
	void RequestScrapyardPickupCarryItem(AARScrapyardCarryItemBase* CarryItemActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestScrapyardPickupCarryItem(AARScrapyardCarryItemBase* CarryItemActor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Interaction")
	void RequestScrapyardDropHeldCarryItem();

	UFUNCTION(Server, Reliable)
	void ServerRequestScrapyardDropHeldCarryItem();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Interaction")
	void RequestScrapyardThrowHeldCarryItem(float ThrowStrength = 900.0f);

	UFUNCTION(Server, Reliable)
	void ServerRequestScrapyardThrowHeldCarryItem(float ThrowStrength = 900.0f);

	/** Generic held secondary action route. Defers behavior to currently held carry item. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Interaction")
	void RequestUseSecondaryOnHeldCarryItem();

	UFUNCTION(Server, Reliable)
	void ServerRequestUseSecondaryOnHeldCarryItem();

	/** Deposit a reserved item into an exit zone (consumes reserved scrap). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Interaction")
	void RequestScrapyardDepositToExit(AARScrapyardExitZoneActor* ExitZone);

	UFUNCTION(Server, Reliable)
	void ServerRequestScrapyardDepositToExit(AARScrapyardExitZoneActor* ExitZone);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Interaction")
	void RequestScrapyardWithdrawFromExit(AARScrapyardExitZoneActor* ExitZone, AARScrapyardCarryItemBase* ItemActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestScrapyardWithdrawFromExit(AARScrapyardExitZoneActor* ExitZone, AARScrapyardCarryItemBase* ItemActor);
};
