#include "ARShopCarryItemBase.h"

AARShopCarryItemBase::AARShopCarryItemBase()
{
	bReplicates = true;
}

void AARShopCarryItemBase::ReleaseCarryItem_Implementation()
{
	Destroy();
}
