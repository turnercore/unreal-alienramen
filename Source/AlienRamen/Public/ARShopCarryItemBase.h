/**
 * @file ARShopCarryItemBase.h
 * @brief Base class for carryable shop-mode actors (for example bowl/meat).
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARShopCarryItemBase.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARShopCarryItemBase : public AActor
{
	GENERATED_BODY()

public:
	AARShopCarryItemBase();

	// Final lifecycle release step for shop carry item cleanup. Override for pooling.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Shop|Carry|Lifecycle")
	void ReleaseCarryItem();

protected:
	void ReleaseCarryItem_Implementation();
};
