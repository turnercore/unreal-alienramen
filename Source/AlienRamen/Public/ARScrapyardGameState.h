#pragma once
/**
 * @file ARScrapyardGameState.h
 * @brief ARScrapyardGameState header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARScrapyardGameState.generated.h"

UCLASS()
class ALIENRAMEN_API AARScrapyardGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARScrapyardGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;
};
