#include "ARShopStateTreeAIComponent.h"

#include "ARShopStateTreeAIComponentSchema.h"

TSubclassOf<UStateTreeSchema> UARShopStateTreeAIComponent::GetSchema() const
{
	return UARShopStateTreeAIComponentSchema::StaticClass();
}
