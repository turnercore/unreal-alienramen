#include "ARStateTreeAIComponentSchema.h"

#include "ARInvaderAIController.h"
#include "AREnemyBase.h"

UARStateTreeAIComponentSchema::UARStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AARInvaderAIController::StaticClass();
	ContextActorClass = AAREnemyBase::StaticClass();
	SyncContextDescriptorTypes();
}

void UARStateTreeAIComponentSchema::PostLoad()
{
	Super::PostLoad();
	SyncContextDescriptorTypes();
}

#if WITH_EDITOR
void UARStateTreeAIComponentSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	SyncContextDescriptorTypes();
}
#endif

void UARStateTreeAIComponentSchema::SyncContextDescriptorTypes()
{
	ContextActorClass = AAREnemyBase::StaticClass();
	AIControllerClass = AARInvaderAIController::StaticClass();

	if (ContextDataDescs.IsValidIndex(0))
	{
		ContextDataDescs[0].Struct = ContextActorClass.Get();
	}
	if (ContextDataDescs.IsValidIndex(1))
	{
		ContextDataDescs[1].Struct = AIControllerClass.Get();
	}
}
