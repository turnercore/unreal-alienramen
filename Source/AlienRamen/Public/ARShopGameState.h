#pragma once
/**
 * @file ARShopGameState.h
 * @brief ARShopGameState header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARShopGameState.generated.h"

UCLASS()
class ALIENRAMEN_API AARShopGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARShopGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;
};
