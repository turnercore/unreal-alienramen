#include "ARCustomerOrderWidgetBase.h"

#include "ARCustomerComponent.h"

void UARCustomerOrderWidgetBase::InitializeFromCustomer(UARCustomerComponent* InCustomerComponent)
{
	UnbindFromCustomer();
	BoundCustomerComponent = InCustomerComponent;

	if (!InCustomerComponent)
	{
		BP_OnCustomerOrderChanged(FARRamenOrderRequest(), false);
		return;
	}

	InCustomerComponent->OnCustomerOrderChanged.AddDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerOrderChanged);
	InCustomerComponent->OnCustomerOrderGeneratedDetailed.AddDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerOrderGeneratedDetailed);
	InCustomerComponent->OnCustomerOrderResolved.AddDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerOrderResolved);
	InCustomerComponent->OnCustomerDoneOrdering.AddDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerDoneOrdering);

	BP_OnCustomerWidgetInitialized(InCustomerComponent);
	HandleCustomerOrderChanged(InCustomerComponent->GetActiveOrder());
}

void UARCustomerOrderWidgetBase::NativeDestruct()
{
	UnbindFromCustomer();
	Super::NativeDestruct();
}

void UARCustomerOrderWidgetBase::HandleCustomerOrderChanged(const FARRamenOrderRequest& NewOrder)
{
	const UARCustomerComponent* Customer = BoundCustomerComponent.Get();
	const bool bHasActiveOrder = Customer && Customer->HasActiveOrder();
	BP_OnCustomerOrderChanged(NewOrder, bHasActiveOrder);
}

void UARCustomerOrderWidgetBase::HandleCustomerOrderGeneratedDetailed(
	const FARRamenOrderRequest& NewOrder,
	const int32 OrdersGeneratedCount,
	const int32 OrdersServedCount,
	const int32 RemainingOrdersToGenerate)
{
	BP_OnCustomerOrderGeneratedDetailed(NewOrder, OrdersGeneratedCount, OrdersServedCount, RemainingOrdersToGenerate);
}

void UARCustomerOrderWidgetBase::HandleCustomerOrderResolved(const FARRamenServeResult& ServeResult)
{
	BP_OnCustomerOrderResolved(ServeResult);
}

void UARCustomerOrderWidgetBase::HandleCustomerDoneOrdering(const int32 OrdersGeneratedCount, const int32 OrdersServedCount, const int32 RemainingOrdersToGenerate)
{
	BP_OnCustomerDoneOrdering(OrdersGeneratedCount, OrdersServedCount, RemainingOrdersToGenerate);
}

void UARCustomerOrderWidgetBase::UnbindFromCustomer()
{
	UARCustomerComponent* Existing = BoundCustomerComponent.Get();
	if (Existing)
	{
		Existing->OnCustomerOrderChanged.RemoveDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerOrderChanged);
		Existing->OnCustomerOrderGeneratedDetailed.RemoveDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerOrderGeneratedDetailed);
		Existing->OnCustomerOrderResolved.RemoveDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerOrderResolved);
		Existing->OnCustomerDoneOrdering.RemoveDynamic(this, &UARCustomerOrderWidgetBase::HandleCustomerDoneOrdering);
	}

	BoundCustomerComponent = nullptr;
}

