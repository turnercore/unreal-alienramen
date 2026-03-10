#include "ARShopStateTreeAIComponentSchema.h"

#include "ARNPCCharacterBase.h"
#include "ARShopAIController.h"

UARShopStateTreeAIComponentSchema::UARShopStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AARShopAIController::StaticClass();
	ContextActorClass = AARNPCCharacterBase::StaticClass();
	SyncContextDescriptorTypes();
}

void UARShopStateTreeAIComponentSchema::PostLoad()
{
	Super::PostLoad();
	SyncContextDescriptorTypes();
}

#if WITH_EDITOR
void UARShopStateTreeAIComponentSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	SyncContextDescriptorTypes();
}
#endif

void UARShopStateTreeAIComponentSchema::SyncContextDescriptorTypes()
{
	ContextActorClass = AARNPCCharacterBase::StaticClass();
	AIControllerClass = AARShopAIController::StaticClass();

	if (ContextDataDescs.IsValidIndex(0))
	{
		ContextDataDescs[0].Struct = ContextActorClass.Get();
	}
	if (ContextDataDescs.IsValidIndex(1))
	{
		ContextDataDescs[1].Struct = AIControllerClass.Get();
	}
}
