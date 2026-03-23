/**
 * @file ARStateTreeAIComponentSchema.h
 * @brief ARStateTreeAIComponentSchema header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARTypedStateTreeAIComponentSchema.h"

#include "ARStateTreeAIComponentSchema.generated.h"

/**
 * StateTree AI schema for Alien Ramen enemy AI components.
 * Defaults:
 * - AIControllerClass: AAREnemyAIController
 * - ContextActorClass: AAREnemyBase
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "AR StateTree AI Schema", CommonSchema))
class ALIENRAMEN_API UARStateTreeAIComponentSchema : public UARTypedStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UARStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	virtual UClass* ResolveContextActorClass() const override;
	virtual UClass* ResolveAIControllerClass() const override;
};
