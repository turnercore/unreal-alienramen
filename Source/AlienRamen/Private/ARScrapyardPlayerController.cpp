#include "ARScrapyardPlayerController.h"

#include "ARLog.h"
#include "ARCarryItemBase.h"
#include "ARScrapyardExitZoneActor.h"
#include "ARScrapyardGameState.h"
#include "ARShopCarryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	static UARShopCarryComponent* ResolveScrapyardCarryComponent(AARScrapyardPlayerController* Controller)
	{
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	}

	static UPrimitiveComponent* ResolveScrapyardCarryPhysicsPrimitive(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			return RootPrimitive;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				return Primitive;
			}
		}

		return PrimitiveComponents.Num() > 0 ? PrimitiveComponents[0] : nullptr;
	}
}

AARScrapyardPlayerController::AARScrapyardPlayerController()
{
}

void AARScrapyardPlayerController::RequestScrapyardPickupCarryItem(AARCarryItemBase* CarryItemActor)
{
	if (!CarryItemActor)
	{
		RequestScrapyardDropHeldCarryItem();
		return;
	}

	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveScrapyardCarryComponent(this);
		if (!CarryComponent || CarryComponent->HasHeldActor())
		{
			return;
		}

		if (UWorld* World = GetWorld())
		{
			if (AARScrapyardGameState* ScrapyardGameState = World->GetGameState<AARScrapyardGameState>())
			{
				if (ScrapyardGameState->IsItemReservedForExtraction(CarryItemActor))
				{
					UE_LOG(
						ARLog,
						Verbose,
						TEXT("[Scrapyard|Carry] Pickup blocked for reserved extraction item '%s'. Use exit-zone withdraw."),
						*GetNameSafe(CarryItemActor));
					return;
				}
			}
		}

		CarryComponent->TrySetHeldActor(CarryItemActor);
		return;
	}

	ServerRequestScrapyardPickupCarryItem(CarryItemActor);
}

void AARScrapyardPlayerController::ServerRequestScrapyardPickupCarryItem_Implementation(AARCarryItemBase* CarryItemActor)
{
	RequestScrapyardPickupCarryItem(CarryItemActor);
}

void AARScrapyardPlayerController::RequestScrapyardDropHeldCarryItem()
{
	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveScrapyardCarryComponent(this);
		if (!CarryComponent)
		{
			return;
		}

		AActor* ReleasedActor = CarryComponent->ClearHeldActor(true);
		if (!ReleasedActor)
		{
			return;
		}

		if (UPrimitiveComponent* PhysicsPrimitive = ResolveScrapyardCarryPhysicsPrimitive(ReleasedActor))
		{
			if (!PhysicsPrimitive->IsSimulatingPhysics())
			{
				PhysicsPrimitive->SetSimulatePhysics(true);
			}
			PhysicsPrimitive->SetEnableGravity(true);
			PhysicsPrimitive->WakeAllRigidBodies();
		}
		return;
	}

	ServerRequestScrapyardDropHeldCarryItem();
}

void AARScrapyardPlayerController::ServerRequestScrapyardDropHeldCarryItem_Implementation()
{
	RequestScrapyardDropHeldCarryItem();
}

void AARScrapyardPlayerController::RequestScrapyardThrowHeldCarryItem(float ThrowStrength)
{
	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveScrapyardCarryComponent(this);
		APawn* ControlledPawn = GetPawn();
		if (!CarryComponent || !ControlledPawn)
		{
			return;
		}

		AActor* ReleasedActor = CarryComponent->ClearHeldActor(true);
		if (!ReleasedActor)
		{
			return;
		}

		if (UPrimitiveComponent* PhysicsPrimitive = ResolveScrapyardCarryPhysicsPrimitive(ReleasedActor))
		{
			if (!PhysicsPrimitive->IsSimulatingPhysics())
			{
				PhysicsPrimitive->SetSimulatePhysics(true);
			}
			PhysicsPrimitive->SetEnableGravity(true);
			PhysicsPrimitive->WakeAllRigidBodies();
			PhysicsPrimitive->AddImpulse(ControlledPawn->GetActorForwardVector() * FMath::Max(50.0f, ThrowStrength), NAME_None, true);
		}
		NotifyInteractionActionCue(EARInteractionActionCue::Throw, ReleasedActor);
		return;
	}

	ServerRequestScrapyardThrowHeldCarryItem(ThrowStrength);
}

void AARScrapyardPlayerController::ServerRequestScrapyardThrowHeldCarryItem_Implementation(float ThrowStrength)
{
	RequestScrapyardThrowHeldCarryItem(ThrowStrength);
}

void AARScrapyardPlayerController::RequestUseSecondaryOnHeldCarryItem()
{
	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveScrapyardCarryComponent(this);
		AARCarryItemBase* HeldCarryItem = CarryComponent ? Cast<AARCarryItemBase>(CarryComponent->GetHeldActor()) : nullptr;
		if (!HeldCarryItem)
		{
			return;
		}

		const bool bHandled = HeldCarryItem->UseSecondaryByController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Scrapyard|Carry] SecondaryUse controller='%s' actor='%s' handled=%d."),
			*GetNameSafe(this),
			*GetNameSafe(HeldCarryItem),
			bHandled ? 1 : 0);
		return;
	}

	ServerRequestUseSecondaryOnHeldCarryItem();
}

void AARScrapyardPlayerController::ServerRequestUseSecondaryOnHeldCarryItem_Implementation()
{
	RequestUseSecondaryOnHeldCarryItem();
}

void AARScrapyardPlayerController::RequestScrapyardDepositToExit(AARScrapyardExitZoneActor* ExitZone)
{
	if (!ExitZone)
	{
		return;
	}

	if (HasAuthority())
	{
		const bool bDeposited = ExitZone->TryDepositHeldItem(this);
		if (!bDeposited)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Scrapyard|Exit] Deposit failed controller='%s' zone='%s'."),
				*GetNameSafe(this),
				*GetNameSafe(ExitZone));
		}
		return;
	}

	ServerRequestScrapyardDepositToExit(ExitZone);
}

void AARScrapyardPlayerController::ServerRequestScrapyardDepositToExit_Implementation(AARScrapyardExitZoneActor* ExitZone)
{
	RequestScrapyardDepositToExit(ExitZone);
}

void AARScrapyardPlayerController::RequestScrapyardWithdrawFromExit(AARScrapyardExitZoneActor* ExitZone, AARCarryItemBase* ItemActor)
{
	if (!ExitZone || !ItemActor)
	{
		return;
	}

	if (HasAuthority())
	{
		const bool bWithdrew = ExitZone->TryWithdrawDepositedItem(this, ItemActor);
		if (!bWithdrew)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Scrapyard|Exit] Withdraw failed controller='%s' zone='%s' item='%s'."),
				*GetNameSafe(this),
				*GetNameSafe(ExitZone),
				*GetNameSafe(ItemActor));
		}
		return;
	}

	ServerRequestScrapyardWithdrawFromExit(ExitZone, ItemActor);
}

void AARScrapyardPlayerController::ServerRequestScrapyardWithdrawFromExit_Implementation(AARScrapyardExitZoneActor* ExitZone, AARCarryItemBase* ItemActor)
{
	RequestScrapyardWithdrawFromExit(ExitZone, ItemActor);
}
