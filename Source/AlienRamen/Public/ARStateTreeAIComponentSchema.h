#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeAIComponentSchema.h"

#include "ARStateTreeAIComponentSchema.generated.h"

/**
 * StateTree AI schema for Alien Ramen enemy AI components.
 * Defaults:
 * - AIControllerClass: AAREnemyAIController
 * - ContextActorClass: AAREnemyBase
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "AR StateTree AI Schema", CommonSchema))
class ALIENRAMEN_API UARStateTreeAIComponentSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UARStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:
	void SyncContextDescriptorTypes();
};

