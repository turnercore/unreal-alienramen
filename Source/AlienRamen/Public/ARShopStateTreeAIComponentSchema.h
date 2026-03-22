/**
 * @file ARShopStateTreeAIComponentSchema.h
 * @brief StateTree schema for shop customer/speaker AI runtime.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARTypedStateTreeAIComponentSchema.h"
#include "ARShopStateTreeAIComponentSchema.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "AR Shop StateTree AI Schema", CommonSchema))
class ALIENRAMEN_API UARShopStateTreeAIComponentSchema : public UARTypedStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UARShopStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	virtual UClass* ResolveContextActorClass() const override;
	virtual UClass* ResolveAIControllerClass() const override;
};
