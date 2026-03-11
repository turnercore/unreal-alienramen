#include "ARShopCarryItemBase.h"

#include "ARLog.h"
#include "ARShopPlayerController.h"
#include "GameFramework/Pawn.h"

AARShopCarryItemBase::AARShopCarryItemBase()
{
	bReplicates = true;
}

void AARShopCarryItemBase::ForwardUseToController(AActor* UsingActor)
{
	AARShopPlayerController* UsingController = Cast<AARShopPlayerController>(UsingActor);
	if (!UsingController)
	{
		const APawn* UsingPawn = Cast<APawn>(UsingActor);
		UsingController = UsingPawn ? Cast<AARShopPlayerController>(UsingPawn->GetController()) : nullptr;
	}

	if (!UsingController)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Shop|Carry] '%s' use-forward ignored: could not resolve AARShopPlayerController from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	UsingController->RequestShopPickupCarryItem(this);
}

void AARShopCarryItemBase::ReleaseCarryItem_Implementation()
{
	Destroy();
}
