/**
 * @file ARCarryItemBase.h
 * @brief Shared carryable actor base used by Shop and Scrapyard runtime items.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ARCarryItemBase.generated.h"

class AActor;
class AARPlayerController;
class AARShopPlayerController;
class AARScrapyardPlayerController;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARCarryItemBase : public AActor
{
	GENERATED_BODY()

public:
	AARCarryItemBase();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Carry|Physics")
	float GetWeightKg() const { return WeightKg; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Carry|Physics")
	float GetResolvedWeightKg() const;

	// Set <= 0 to restore native component mass behavior (no explicit override).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Carry|Physics", meta = (BlueprintAuthorityOnly))
	void SetWeightKg(float NewWeightKg);

	// Optional forwarding helper for BI_Interactable-style calls. Accepts pawn/controller and routes to pickup request.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Forward Use To Controller"))
	virtual void ForwardUseToController(AActor* UsingActor);

	// Optional forwarding helper for BI_Interactable-style secondary calls.
	// Resolves controller from pawn/controller source and routes to held-secondary behavior.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Forward Secondary Use To Controller"))
	virtual void ForwardSecondaryUseToController(AActor* UsingActor);

	// Optional forwarding helper for BI_Interactable-style kick calls on world items.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Forward Kick To Controller"))
	virtual void ForwardKickToController(AActor* UsingActor);

	// Final lifecycle release step for carry item cleanup. Override for pooling.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Carry|Lifecycle")
	void ReleaseCarryItem();

	// Generic held-secondary action entrypoint. Default behavior routes to throw for throwable carryables.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Carry|Interaction")
	bool UseSecondaryByController(AARPlayerController* UsingController);

	// Generic world-secondary action entrypoint for non-held items.
	// Default behavior applies a "kick" impulse based on interacting controller strength.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Carry|Interaction")
	bool UseSecondaryInWorldByController(AARPlayerController* UsingController);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Scrapyard|Item", meta = (ToolTip = "Item identity tag looked up via the shared Item route to resolve scrapyard definitions and rewards."))
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

	void ReleaseCarryItem_Implementation();
	virtual bool UseSecondaryByController_Implementation(AARPlayerController* UsingController);
	virtual bool UseSecondaryInWorldByController_Implementation(AARPlayerController* UsingController);

	UFUNCTION()
	void OnRep_WeightKg();

	UFUNCTION()
	void OnRep_VisualModelClass();

	void ApplyWeightToPrimitiveComponents() const;
	float ResolveDefaultWeightKg() const;
	void RefreshVisualModelActor();
	void DestroyVisualModelActor();

	// 0 uses native primitive default mass. >0 applies explicit mass override in kg.
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_WeightKg, BlueprintReadOnly, Category = "Alien Ramen|Carry|Physics", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WeightKg = 0.0f;

	// Tag key resolved through the shared Item route.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item", meta = (ToolTip = "Item identity tag looked up via the Item route to resolve definitions/rewards."))
	FGameplayTag ScrapyardItemTag;

	// Used only when item definition resolution fails.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Fallback scrap cost applied if item definition lookup fails at runtime."))
	int32 FallbackScrapCost = 0;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_VisualModelClass, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Item", meta = (ToolTip = "Optional model class spawned for cosmetics when the resolved item class is not itself a carryable."))
	TSoftClassPtr<AActor> VisualModelClass;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedVisualModelActor = nullptr;
};
