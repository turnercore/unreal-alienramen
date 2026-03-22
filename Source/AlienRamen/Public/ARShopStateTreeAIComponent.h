/**
 * @file ARShopStateTreeAIComponent.h
 * @brief Shop customer/speaker StateTree component exposing active runtime state tags.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARStateTreeAIComponent.h"
#include "ARShopStateTreeAIComponent.generated.h"

class UStateTreeSchema;

UCLASS(ClassGroup = AI, Blueprintable, meta = (BlueprintSpawnableComponent))
class ALIENRAMEN_API UARShopStateTreeAIComponent : public UARStateTreeAIComponent
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UStateTreeSchema> GetSchema() const override;
};
