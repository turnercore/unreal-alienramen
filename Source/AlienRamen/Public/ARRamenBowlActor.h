/**
 * @file ARRamenBowlActor.h
 * @brief Replicated ramen bowl build state for shop serving.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARShopCarryItemBase.h"
#include "ARShopRamenTypes.h"
#include "ARRamenBowlActor.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARRamenBowlActor : public AARShopCarryItemBase
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
	bool TryApplyFillFromStation(EARRamenStationType StationType, EARAffinityColor StationColor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Bowl", meta = (BlueprintAuthorityOnly))
	void ClearBowl();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Bowl")
	FARRamenBowlSpec BowlSpec;

	// 0=Noodles, 1=Broth, 2=Toppings, 3=Complete.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Bowl")
	int32 FillStep = 0;
};
