/**
 * @file ARRamenMeatActor.h
 * @brief Replicated shop meat pickup used by station loading and storage boxes.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "ARPickupBase.h"
#include "ARRamenMeatActor.generated.h"

UCLASS(Blueprintable)
class ALIENRAMEN_API AARRamenMeatActor : public AARPickupBase
{
	GENERATED_BODY()

public:
	AARRamenMeatActor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Meat", meta = (BlueprintAuthorityOnly))
	void SetMeatData(EARAffinityColor NewColor, int32 NewAmount);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Meat")
	EARAffinityColor GetMeatColor() const { return MeatColor; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Meat")
	int32 GetMeatAmount() const { return MeatAmount; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Meat")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Alien Ramen|Shop|Meat", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmount = 1;
};
