/**
 * @file ARRamenBowlActor.h
 * @brief Replicated ramen bowl build state for shop serving.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARCarryItemBase.h"
#include "ARShopRamenTypes.h"
#include "ARRamenBowlActor.generated.h"

/** Broadcasts whenever the bowl fill step changes, locally or via replication. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARRamenBowlFillStepChangedSignature, int32, PreviousFillStep, int32, NewFillStep);

UCLASS(Blueprintable)
class ALIENRAMEN_API AARRamenBowlActor : public AARCarryItemBase
{
	GENERATED_BODY()

public:
	AARRamenBowlActor();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Bowl")
	const FARRamenBowlSpec& GetBowlSpec() const { return BowlSpec; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Bowl")
	int32 GetFillStep() const { return FillStep; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Bowl")
	bool IsComplete() const { return FillStep >= 3; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Bowl")
	EARRamenStationType GetNextRequiredStationType() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Bowl", meta = (BlueprintAuthorityOnly))
	bool TryApplyFillFromStation(EARRamenStationType StationType, EARAffinityColor StationColor, FGameplayTag StationMeatTag, EARVendingQualityTier StationQualityTier = EARVendingQualityTier::Standard);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Bowl", meta = (BlueprintAuthorityOnly))
	void ClearBowl();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Handles replicated fill-step updates and forwards them to Blueprint listeners. */
	UFUNCTION()
	void OnRep_FillStep(int32 PreviousFillStep);

	/** Emits fill-step changed notifications if the value actually changed. */
	void BroadcastFillStepChanged(int32 PreviousFillStep, int32 NewFillStep);

public:
	/**
	 * Event dispatcher fired whenever FillStep changes.
	 * Bind animation/UI updates here to react to ramen assembly progress.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Shop|Bowl|Events")
	FARRamenBowlFillStepChangedSignature OnFillStepChanged;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Bowl")
	FARRamenBowlSpec BowlSpec;

	// 0=Noodles, 1=Broth, 2=Toppings, 3=Complete.
	UPROPERTY(ReplicatedUsing = OnRep_FillStep, BlueprintReadOnly, Category = "Alien Ramen|Shop|Bowl")
	int32 FillStep = 0;
};
