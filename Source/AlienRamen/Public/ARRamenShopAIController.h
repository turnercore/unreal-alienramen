#pragma once
/**
 * @file ARRamenShopAIController.h
 * @brief ARRamenShopAIController header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "ARShopAIController.h"
#include "ARRamenShopAIController.generated.h"

// Legacy compatibility wrapper. Prefer AARShopAIController.
UCLASS()
class ALIENRAMEN_API AARRamenShopAIController : public AARShopAIController
{
	GENERATED_BODY()

public:
	AARRamenShopAIController();
};
