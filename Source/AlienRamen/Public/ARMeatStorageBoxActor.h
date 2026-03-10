/**
 * @file ARMeatStorageBoxActor.h
 * @brief Shop meat storage dispenser backed by replicated GameState meat buckets.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "ARShopDispenserActor.h"
#include "ARMeatStorageBoxActor.generated.h"

class AARPlayerController;
class AARRamenMeatActor;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARMeatStorageBoxActor : public AARShopDispenserActor
{
	GENERATED_BODY()

public:
	AARMeatStorageBoxActor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|MeatStorage", meta = (BlueprintAuthorityOnly))
	bool TryDispenseMeat(AARPlayerController* RequestingController);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmountPerDispense = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage")
	TSubclassOf<AARRamenMeatActor> MeatActorClass;

	// Optional typed item tag forwarded into generic dispenser lookup.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|MeatStorage", meta = (Categories = "Shop.Item"))
	FGameplayTag MeatItemTag;

private:
	void SyncLegacyDefinition();
};
