#include "ARShopStateTreeAIComponentSchema.h"

#include "ARNPCCharacterBase.h"
#include "ARShopAIController.h"

UARShopStateTreeAIComponentSchema::UARShopStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SyncContextDescriptorTypes();
}

UClass* UARShopStateTreeAIComponentSchema::ResolveContextActorClass() const
{
	return AARNPCCharacterBase::StaticClass();
}

UClass* UARShopStateTreeAIComponentSchema::ResolveAIControllerClass() const
{
	return AARShopAIController::StaticClass();
}
