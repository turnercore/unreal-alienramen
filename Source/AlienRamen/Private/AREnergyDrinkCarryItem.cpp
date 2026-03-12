#include "AREnergyDrinkCarryItem.h"

#include "ARGameModeBase.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffSubsystem.h"
#include "ARShopCarryComponent.h"
#include "ARShopPlayerController.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

void AAREnergyDrinkCarryItem::SetEnergyDrinkItemTag(const FGameplayTag NewItemTag)
{
	if (!HasAuthority())
	{
		return;
	}

	EnergyDrinkItemTag = NewItemTag;
}

bool AAREnergyDrinkCarryItem::TryConsumeFromController(AARShopPlayerController* ShopController)
{
	if (!HasAuthority() || !ShopController || !EnergyDrinkItemTag.IsValid())
	{
		return false;
	}

	const AARGameModeBase* GameMode = GetWorld() ? Cast<AARGameModeBase>(GetWorld()->GetAuthGameMode()) : nullptr;
	const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	if (!GameMode || !ShopModeTag.IsValid() || GameMode->GetModeTag() != ShopModeTag)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[EnergyDrink] Consume rejected for '%s': mode is not shop."),
			*GetNameSafe(this));
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UARRunBuffSubsystem* RunBuffSubsystem = GameInstance ? GameInstance->GetSubsystem<UARRunBuffSubsystem>() : nullptr;
	AARPlayerStateBase* PlayerState = ShopController->GetPlayerState<AARPlayerStateBase>();
	if (!RunBuffSubsystem || !PlayerState)
	{
		return false;
	}

	if (!RunBuffSubsystem->ConsumeSpawnedEnergyDrinkForPlayerState(EnergyDrinkItemTag, PlayerState))
	{
		return false;
	}

	APawn* ControlledPawn = ShopController->GetPawn();
	if (UARShopCarryComponent* CarryComponent = ControlledPawn ? ControlledPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr)
	{
		if (CarryComponent->GetHeldActor() == this)
		{
			CarryComponent->ReleaseHeldActorForTransfer();
		}
	}

	ReleaseCarryItem();
	return true;
}
