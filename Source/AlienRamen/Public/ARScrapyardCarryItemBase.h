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
	virtual void ForwardUseToController(AActor* UsingActor) override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Item")
	FGameplayTag GetScrapyardItemTag() const { return ScrapyardItemTag; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Item", meta = (BlueprintAuthorityOnly))
	void SetScrapyardItemTag(FGameplayTag NewItemTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Item", meta = (BlueprintAuthorityOnly))
	void SetFallbackScrapCost(int32 NewFallbackCost);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Item")
	int32 GetFallbackScrapCost() const { return FallbackScrapCost; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Tag key resolved through Scrapyard.Item route.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item")
	FGameplayTag ScrapyardItemTag;

	// Used only when item definition resolution fails.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item", meta = (ClampMin = "0", UIMin = "0"))
	int32 FallbackScrapCost = 0;
};
