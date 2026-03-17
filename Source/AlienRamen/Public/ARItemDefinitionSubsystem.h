/**
 * @file ARItemDefinitionSubsystem.h
 * @brief Shared item-definition resolver for Shop + Scrapyard flows.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARScrapyardTypes.h"
#include "ARShopRamenTypes.h"
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

	/** Resolves the canonical meat definition row for an Item.Meat tag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items|Meat")
	bool ResolveMeatDefinition(FGameplayTag MeatTag, FARMeatDefinitionRow& OutMeatDef) const;

	/** Resolves the first deterministic meat definition row that matches the requested color. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items|Meat")
	bool ResolveFirstMeatDefinitionForColor(EARAffinityColor Color, FARMeatDefinitionRow& OutMeatDef) const;

	/** Resolves the first deterministic Item.Meat tag that matches the requested color. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items|Meat")
	bool ResolveFirstMeatTagForColor(EARAffinityColor Color, FGameplayTag& OutMeatTag) const;

	/** Returns all Item.Meat tags matching the requested color in deterministic row-name order. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items|Meat")
	bool GetMeatTagsForColor(EARAffinityColor Color, TArray<FGameplayTag>& OutMeatTags) const;

	/** Resolves summed item sell value for all meat tags authored on a completed bowl. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Items|Meat")
	int32 ResolveCombinedMeatItemValue(const FARRamenBowlSpec& BowlSpec) const;

private:
	bool ResolveItemDefinition_Internal(FGameplayTag ItemTag, FARScrapyardItemDefRow& OutItemDef) const;
	bool ResolveMeatDefinition_Internal(FGameplayTag MeatTag, FARMeatDefinitionRow& OutMeatDef) const;
	bool GatherAllMeatDefinitions(TArray<TPair<FName, FARMeatDefinitionRow>>& OutDefinitions) const;
	static void ApplyKnowledgeTextFallback(FGameplayTag ItemTag, FARScrapyardItemDefRow& InOutDef);
};
