#pragma once
/**
 * @file ARRamenShopCharacterBase.h
 * @brief ARRamenShopCharacterBase header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "ARShopCharacterBase.h"
#include "ARRamenShopCharacterBase.generated.h"

// Legacy compatibility wrapper. Prefer AARShopCharacterBase.
UCLASS()
class ALIENRAMEN_API AARRamenShopCharacterBase : public AARShopCharacterBase
{
	GENERATED_BODY()

public:
	AARRamenShopCharacterBase();
};
