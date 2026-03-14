/**
 * @file ARCustomerOrderWidgetBase.h
 * @brief Blueprintable base widget for shop customer order display driven by UARCustomerComponent.
 */
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARShopRamenTypes.h"
#include "ARCustomerOrderWidgetBase.generated.h"

class UARCustomerComponent;

UCLASS(Abstract, BlueprintType, Blueprintable)
class ALIENRAMEN_API UARCustomerOrderWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer|UI")
	void InitializeFromCustomer(UARCustomerComponent* InCustomerComponent);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer|UI")
	UARCustomerComponent* GetBoundCustomerComponent() const { return BoundCustomerComponent.Get(); }

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Shop|Customer|UI")
	void BP_OnCustomerWidgetInitialized(UARCustomerComponent* CustomerComponent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Shop|Customer|UI")
	void BP_OnCustomerOrderChanged(const FARRamenOrderRequest& NewOrder, bool bHasActiveOrder);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Shop|Customer|UI")
	void BP_OnCustomerOrderGeneratedDetailed(const FARRamenOrderRequest& NewOrder, int32 OrdersGeneratedCount, int32 OrdersServedCount, int32 RemainingOrdersToGenerate);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Shop|Customer|UI")
	void BP_OnCustomerOrderResolved(const FARRamenServeResult& ServeResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Shop|Customer|UI")
	void BP_OnCustomerDoneOrdering(int32 OrdersGeneratedCount, int32 OrdersServedCount, int32 RemainingOrdersToGenerate);

private:
	UFUNCTION()
	void HandleCustomerOrderChanged(const FARRamenOrderRequest& NewOrder);

	UFUNCTION()
	void HandleCustomerOrderGeneratedDetailed(const FARRamenOrderRequest& NewOrder, int32 OrdersGeneratedCount, int32 OrdersServedCount, int32 RemainingOrdersToGenerate);

	UFUNCTION()
	void HandleCustomerOrderResolved(const FARRamenServeResult& ServeResult);

	UFUNCTION()
	void HandleCustomerDoneOrdering(int32 OrdersGeneratedCount, int32 OrdersServedCount, int32 RemainingOrdersToGenerate);

	void UnbindFromCustomer();

	UPROPERTY(Transient)
	TWeakObjectPtr<UARCustomerComponent> BoundCustomerComponent;
};

