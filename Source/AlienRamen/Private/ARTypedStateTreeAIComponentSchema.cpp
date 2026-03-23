#include "ARTypedStateTreeAIComponentSchema.h"

#include "AIController.h"

UARTypedStateTreeAIComponentSchema::UARTypedStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UARTypedStateTreeAIComponentSchema::PostLoad()
{
	Super::PostLoad();
	SyncContextDescriptorTypes();
}

#if WITH_EDITOR
void UARTypedStateTreeAIComponentSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	SyncContextDescriptorTypes();
}
#endif

void UARTypedStateTreeAIComponentSchema::SyncContextDescriptorTypes()
{
	ContextActorClass = ResolveContextActorClass();
	AIControllerClass = ResolveAIControllerClass();

	if (ContextDataDescs.IsValidIndex(0))
	{
		ContextDataDescs[0].Struct = ContextActorClass.Get();
	}
	if (ContextDataDescs.IsValidIndex(1))
	{
		ContextDataDescs[1].Struct = AIControllerClass.Get();
	}
}
