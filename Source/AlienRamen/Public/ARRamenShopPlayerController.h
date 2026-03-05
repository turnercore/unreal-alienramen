#pragma once

#include "CoreMinimal.h"
#include "ARShopPlayerController.h"
#include "ARRamenShopPlayerController.generated.h"

// Legacy compatibility wrapper. Prefer AARShopPlayerController.
UCLASS()
class ALIENRAMEN_API AARRamenShopPlayerController : public AARShopPlayerController
{
	GENERATED_BODY()

public:
	AARRamenShopPlayerController();
};
