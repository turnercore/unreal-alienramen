/**
 * @file ARInvaderAIController.h
 * @brief ARInvaderAIController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "AREnemyAIController.h"
#include "ARInvaderAIController.generated.h"

UCLASS()
class ALIENRAMEN_API AARInvaderAIController : public AAREnemyAIController
{
	GENERATED_BODY()

public:
	AARInvaderAIController();
};
