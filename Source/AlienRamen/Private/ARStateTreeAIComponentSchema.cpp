#include "ARStateTreeAIComponentSchema.h"

#include "AREnemyAIController.h"
#include "AREnemyBase.h"

UARStateTreeAIComponentSchema::UARStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SyncContextDescriptorTypes();
}

UClass* UARStateTreeAIComponentSchema::ResolveContextActorClass() const
{
	return AAREnemyBase::StaticClass();
}

UClass* UARStateTreeAIComponentSchema::ResolveAIControllerClass() const
{
	return AAREnemyAIController::StaticClass();
}
