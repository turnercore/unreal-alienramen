#include "AREnemyStateTreeSchema.h"

#include "AREnemyAIController.h"
#include "AREnemyBase.h"

UAREnemyStateTreeSchema::UAREnemyStateTreeSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AAREnemyAIController::StaticClass();
	ContextActorClass = AAREnemyBase::StaticClass();
}

