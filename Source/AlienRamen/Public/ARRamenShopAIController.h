#pragma once

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
