/**
 * @file ARInvaderGameState.h
 * @brief ARInvaderGameState header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARInvaderGameState.generated.h"

UCLASS()
class ALIENRAMEN_API AARInvaderGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARInvaderGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;
};
