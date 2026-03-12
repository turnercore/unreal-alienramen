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

	// Optional authored visual model class used when item DT class is not a carry-item subclass.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Item", meta = (BlueprintAuthorityOnly))
	void SetVisualModelClass(TSoftClassPtr<AActor> NewVisualModelClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_VisualModelClass();

	void RefreshVisualModelActor();
	void DestroyVisualModelActor();

	// Tag key resolved through Scrapyard.Item route.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item")
	FGameplayTag ScrapyardItemTag;

	// Used only when item definition resolution fails.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item", meta = (ClampMin = "0", UIMin = "0"))
	int32 FallbackScrapCost = 0;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_VisualModelClass, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item")
	TSoftClassPtr<AActor> VisualModelClass;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedVisualModelActor = nullptr;
};
