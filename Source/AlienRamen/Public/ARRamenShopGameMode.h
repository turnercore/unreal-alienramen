#pragma once

#include "CoreMinimal.h"
#include "ARShopGameMode.h"
#include "ARRamenShopGameMode.generated.h"

// Legacy compatibility wrapper. Prefer AARShopGameMode.
UCLASS()
class ALIENRAMEN_API AARRamenShopGameMode : public AARShopGameMode
{
	GENERATED_BODY()

public:
	AARRamenShopGameMode();
};
