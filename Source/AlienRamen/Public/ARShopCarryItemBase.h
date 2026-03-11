/**
 * @file ARShopCarryItemBase.h
 * @brief Base class for carryable shop-mode actors (for example bowl/meat).
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARShopCarryItemBase.generated.h"

class AActor;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARShopCarryItemBase : public AActor
{
	GENERATED_BODY()

public:
	AARShopCarryItemBase();

	// Optional forwarding helper for BI_Interactable-style calls. Accepts pawn/controller and routes to pickup request.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Forward Use To Controller"))
	void ForwardUseToController(AActor* UsingActor);

	// Final lifecycle release step for shop carry item cleanup. Override for pooling.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Shop|Carry|Lifecycle")
	void ReleaseCarryItem();

protected:
	void ReleaseCarryItem_Implementation();
};
