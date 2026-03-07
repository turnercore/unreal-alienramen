#include "ARSaveTypesLibrary.h"

int32 UARSaveTypesLibrary::GetTotalMeatAmount(const FARMeatState& MeatState)
{
	return MeatState.GetTotalAmount();
}
