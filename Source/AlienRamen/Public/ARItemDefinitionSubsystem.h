/**
 * @file ARItemDefinitionSubsystem.h
 * @brief Shared item-definition resolver for Shop + Scrapyard flows.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARScrapyardTypes.h"
#include "ARItemDefinitionSubsystem.generated.h"

class AActor;

UCLASS()
class ALIENRAMEN_API UARItemDefinitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Resolves the canonical shared item definition row for ItemTag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items")
	bool ResolveItemDefinition(FGameplayTag ItemTag, FARScrapyardItemDefRow& OutItemDef) const;

	/** Resolves energy-drink payload definition for ItemTag (or linked item-row fallback). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items")
	bool ResolveEnergyDrinkDefinition(FGameplayTag ItemTag, FAREnergyDrinkDefRow& OutEnergyDrinkDef) const;

	/** Resolves authored actor class from shared item definition (ItemModelClass). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items")
	bool ResolveItemActorClass(FGameplayTag ItemTag, TSubclassOf<AActor>& OutActorClass) const;

	/** Applies shared physical values (for example weight->mass) to a spawned actor. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Items")
	bool ApplyItemPhysicsProperties(AActor* Actor, FGameplayTag ItemTag) const;

private:
	bool ResolveItemDefinition_Internal(FGameplayTag ItemTag, FARScrapyardItemDefRow& OutItemDef) const;
	static void ApplyKnowledgeTextFallback(FGameplayTag ItemTag, FARScrapyardItemDefRow& InOutDef);
};
