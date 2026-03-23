/**
 * @file ARTypedStateTreeAIComponentSchema.h
 * @brief Shared typed base for AR StateTree AI schemas.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "ARTypedStateTreeAIComponentSchema.generated.h"

/**
 * Shared base class that keeps StateTree context descriptors synchronized with
 * concrete actor/controller classes supplied by derived schemas.
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, CollapseCategories)
class ALIENRAMEN_API UARTypedStateTreeAIComponentSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UARTypedStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual UClass* ResolveContextActorClass() const PURE_VIRTUAL(UARTypedStateTreeAIComponentSchema::ResolveContextActorClass, return nullptr;);
	virtual UClass* ResolveAIControllerClass() const PURE_VIRTUAL(UARTypedStateTreeAIComponentSchema::ResolveAIControllerClass, return nullptr;);
	void SyncContextDescriptorTypes();
};
