/**
 * @file ARShopCarryItemBase.h
 * @brief Base class for carryable shop-mode actors (for example bowl/meat).
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARShopCarryItemBase.generated.h"

class AActor;
class AARPlayerController;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARShopCarryItemBase : public AActor
{
	GENERATED_BODY()

public:
	AARShopCarryItemBase();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry|Physics")
	float GetWeightKg() const { return WeightKg; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Carry|Physics")
	float GetResolvedWeightKg() const;

	// Set <= 0 to restore native component mass behavior (no explicit override).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Carry|Physics", meta = (BlueprintAuthorityOnly))
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

	// Final lifecycle release step for shop carry item cleanup. Override for pooling.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Shop|Carry|Lifecycle")
	void ReleaseCarryItem();

	// Generic held-secondary action entrypoint. Default behavior routes to throw for throwable carryables.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Shop|Carry|Interaction")
	bool UseSecondaryByController(AARPlayerController* UsingController);

	// Generic world-secondary action entrypoint for non-held items.
	// Default behavior applies a "kick" impulse based on interacting controller strength.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Alien Ramen|Shop|Carry|Interaction")
	bool UseSecondaryInWorldByController(AARPlayerController* UsingController);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ReleaseCarryItem_Implementation();
	virtual bool UseSecondaryByController_Implementation(AARPlayerController* UsingController);
	virtual bool UseSecondaryInWorldByController_Implementation(AARPlayerController* UsingController);

	UFUNCTION()
	void OnRep_WeightKg();

	void ApplyWeightToPrimitiveComponents() const;
	float ResolveDefaultWeightKg() const;

	// 0 uses native primitive default mass. >0 applies explicit mass override in kg.
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_WeightKg, BlueprintReadOnly, Category = "Alien Ramen|Shop|Carry|Physics", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WeightKg = 0.0f;
};
