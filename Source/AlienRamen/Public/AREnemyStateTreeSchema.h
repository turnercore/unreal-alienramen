#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeAIComponentSchema.h"

#include "AREnemyStateTreeSchema.generated.h"

/**
 * StateTree AI schema preconfigured for Alien Ramen enemies.
 * Defaults:
 * - AIControllerClass: AAREnemyAIController
 * - ContextActorClass: AAREnemyBase
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "AR Enemy StateTree AI Component", CommonSchema))
class ALIENRAMEN_API UAREnemyStateTreeSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UAREnemyStateTreeSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:
	void SyncContextDescriptorTypes();
};
