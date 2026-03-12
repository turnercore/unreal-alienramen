/**
 * @file ARPlayerCharacterScrapyard.h
 * @brief ARPlayerCharacterScrapyard header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerCharacterBase.h"
#include "ARPlayerCharacterScrapyard.generated.h"

class UARShopCarryComponent;
class AActor;

UCLASS()
class ALIENRAMEN_API AARPlayerCharacterScrapyard : public AARPlayerCharacterBase
{
	GENERATED_BODY()

public:
	AARPlayerCharacterScrapyard();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Carry")
	UARShopCarryComponent* GetScrapyardCarryComponent() const { return ScrapyardCarryComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Carry")
	bool IsCarryingScrapyardItem() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Carry")
	AActor* GetHeldScrapyardActor() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Carry")
	TObjectPtr<UARShopCarryComponent> ScrapyardCarryComponent;
};
