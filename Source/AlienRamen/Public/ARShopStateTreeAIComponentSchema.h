/**
 * @file ARShopStateTreeAIComponentSchema.h
 * @brief StateTree schema for shop customer/speaker AI runtime.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "ARShopStateTreeAIComponentSchema.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "AR Shop StateTree AI Schema", CommonSchema))
class ALIENRAMEN_API UARShopStateTreeAIComponentSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UARShopStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:
	void SyncContextDescriptorTypes();
};
