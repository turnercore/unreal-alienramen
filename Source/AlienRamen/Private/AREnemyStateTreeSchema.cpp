#include "AREnemyStateTreeSchema.h"

#include "AREnemyAIController.h"
#include "AREnemyBase.h"

UAREnemyStateTreeSchema::UAREnemyStateTreeSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AAREnemyAIController::StaticClass();
	ContextActorClass = AAREnemyBase::StaticClass();
	SyncContextDescriptorTypes();
}

void UAREnemyStateTreeSchema::PostLoad()
{
	Super::PostLoad();
	SyncContextDescriptorTypes();
}

#if WITH_EDITOR
void UAREnemyStateTreeSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	SyncContextDescriptorTypes();
}
#endif

void UAREnemyStateTreeSchema::SyncContextDescriptorTypes()
{
	ContextActorClass = AAREnemyBase::StaticClass();
	AIControllerClass = AAREnemyAIController::StaticClass();

	if (ContextDataDescs.IsValidIndex(0))
	{
		ContextDataDescs[0].Struct = ContextActorClass.Get();
	}
	if (ContextDataDescs.IsValidIndex(1))
	{
		ContextDataDescs[1].Struct = AIControllerClass.Get();
	}
}
