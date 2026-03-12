/**
 * @file ARScrapyardCarryItemBase.h
 * @brief Carryable scrapyard item actor tagged for extraction/reward resolution.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARShopCarryItemBase.h"
#include "ARScrapyardCarryItemBase.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARScrapyardCarryItemBase : public AARShopCarryItemBase
{
	GENERATED_BODY()

public:
	AARScrapyardCarryItemBase();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Item")
	FGameplayTag GetScrapyardItemTag() const { return ScrapyardItemTag; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Item")
	int32 GetFallbackScrapCost() const { return FallbackScrapCost; }

protected:
	// Tag key resolved through Scrapyard.Item route.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item")
	FGameplayTag ScrapyardItemTag;

	// Used only when item definition resolution fails.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item", meta = (ClampMin = "0", UIMin = "0"))
	int32 FallbackScrapCost = 0;
};
