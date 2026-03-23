#include "ARShopPlayerController.h"

#include "AREnergyDrinkCarryItem.h"
#include "ARLog.h"
#include "ARAttributeSetCore.h"
#include "ARCarryItemBase.h"
#include "ARAttributeSetPlayer.h"
#include "ARNPCCharacterBase.h"
#include "ARPlayerStateBase.h"
#include "ARRamenMeatActor.h"
#include "ARShopCarryComponent.h"
#include "ARShopDispenserActor.h"
#include "ARShopGameState.h"
#include "ARShopStationActor.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	static UARShopCarryComponent* ResolveShopCarryComponentFromController(AARShopPlayerController* Controller)
	{
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	}

	static UPrimitiveComponent* ResolveCarryPhysicsPrimitiveForController(AActor* Actor)
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

		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				return Primitive;
			}
		}

		return PrimitiveComponents.Num() > 0 ? PrimitiveComponents[0] : nullptr;
	}

	static bool IsAttachedToOtherActor(const AActor* ActorToCheck, const AActor* AllowedAttachParentActor)
	{
		if (!ActorToCheck)
		{
			return false;
		}

		const USceneComponent* Root = ActorToCheck->GetRootComponent();
		const AActor* AttachParentActor = Root && Root->GetAttachParent() ? Root->GetAttachParent()->GetOwner() : nullptr;
		return AttachParentActor && AttachParentActor != AllowedAttachParentActor;
	}

	static void ApplyThrowImpulse(AActor* ReleasedActor, const FVector& ForwardDirection, const float ThrowStrength)
	{
		if (!ReleasedActor)
		{
			return;
		}

		UPrimitiveComponent* PhysicsPrimitive = ResolveCarryPhysicsPrimitiveForController(ReleasedActor);
		if (!PhysicsPrimitive)
		{
			return;
		}

		if (!PhysicsPrimitive->IsSimulatingPhysics())
		{
			PhysicsPrimitive->SetSimulatePhysics(true);
		}

		PhysicsPrimitive->SetEnableGravity(true);
		PhysicsPrimitive->WakeAllRigidBodies();
		PhysicsPrimitive->AddImpulse(ForwardDirection.GetSafeNormal() * FMath::Max(50.0f, ThrowStrength), NAME_None, true);
	}

	static float ResolveThrowStrengthForController(const AARShopPlayerController* Controller, const float RequestedThrowStrength)
	{
		if (RequestedThrowStrength > 0.0f)
		{
			return RequestedThrowStrength;
		}

		float Strength = 10.0f;
		const AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetASC() : nullptr;
		if (ASC)
		{
			Strength = ASC->GetNumericAttribute(UARAttributeSetPlayer::GetStrengthAttribute());
		}

		return FMath::Max(0.0f, Strength) * 100.0f;
	}
}

AARShopPlayerController::AARShopPlayerController()
{
}

void AARShopPlayerController::RequestShopUseOrDrop(AActor* InteractableActor)
{
	if (!InteractableActor)
	{
		RequestShopPickupCarryItem(nullptr);
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(InteractableActor, TEXT("Shop|UseOrDrop")))
		{
			return;
		}

		static const FName ForwardUseFunctionName(TEXT("ForwardUseToController"));
		if (UFunction* ForwardUseFunction = InteractableActor->FindFunction(ForwardUseFunctionName))
		{
			struct FForwardUseToControllerParams
			{
				AActor* UsingActor = nullptr;
			};

			FForwardUseToControllerParams Params;
			Params.UsingActor = this;
			InteractableActor->ProcessEvent(ForwardUseFunction, &Params);
			return;
		}

		if (AARNPCCharacterBase* NPCCharacter = Cast<AARNPCCharacterBase>(InteractableActor))
		{
			RequestInteractWithCharacter(NPCCharacter);
			return;
		}

		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Carry] UseOrDrop ignored on '%s': target '%s' has no ForwardUseToController handler."),
			*GetNameSafe(this),
			*GetNameSafe(InteractableActor));
		return;
	}

	ServerRequestShopUseOrDrop(InteractableActor);
}

void AARShopPlayerController::ServerRequestShopUseOrDrop_Implementation(AActor* InteractableActor)
{
	RequestShopUseOrDrop(InteractableActor);
}

void AARShopPlayerController::RequestShopStationPlaceHeldMeat(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] PlaceHeldMeat ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|PlaceHeldMeat")))
		{
			return;
		}

		const bool bPlaced = StationActor->TryPlaceHeldMeatFromController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] PlaceHeldMeat controller='%s' station='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StationActor),
			bPlaced ? 1 : 0);
		return;
	}

	ServerRequestShopStationPlaceHeldMeat(StationActor);
}

void AARShopPlayerController::ServerRequestShopStationPlaceHeldMeat_Implementation(AARShopStationActor* StationActor)
{
	RequestShopStationPlaceHeldMeat(StationActor);
}

void AARShopPlayerController::RequestShopStationPickupMeat(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] PickupMeat ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|PickupMeat")))
		{
			return;
		}

		const bool bPickedUp = StationActor->TryPickupSlottedMeatToController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] PickupMeat controller='%s' station='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StationActor),
			bPickedUp ? 1 : 0);
		return;
	}

	ServerRequestShopStationPickupMeat(StationActor);
}

void AARShopPlayerController::ServerRequestShopStationPickupMeat_Implementation(AARShopStationActor* StationActor)
{
	RequestShopStationPickupMeat(StationActor);
}

void AARShopPlayerController::RequestShopStationStartProcessing(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] StartProcessing ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|StartProcessing")))
		{
			return;
		}

		const bool bStarted = StationActor->StartProcessingByController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] StartProcessing controller='%s' station='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StationActor),
			bStarted ? 1 : 0);
		return;
	}

	ServerRequestShopStationStartProcessing(StationActor);
}

void AARShopPlayerController::ServerRequestShopStationStartProcessing_Implementation(AARShopStationActor* StationActor)
{
	RequestShopStationStartProcessing(StationActor);
}

void AARShopPlayerController::RequestShopStationTapProcessing(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] TapProcessing ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|TapProcessing")))
		{
			return;
		}

		const bool bTapped = StationActor->TapProcessByController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] TapProcessing controller='%s' station='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StationActor),
			bTapped ? 1 : 0);
		return;
	}

	ServerRequestShopStationTapProcessing(StationActor);
}

void AARShopPlayerController::ServerRequestShopStationTapProcessing_Implementation(AARShopStationActor* StationActor)
{
	RequestShopStationTapProcessing(StationActor);
}

void AARShopPlayerController::RequestShopStationStopProcessing(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] StopProcessing ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|StopProcessing")))
		{
			return;
		}

		const bool bStopped = StationActor->StopProcessingByController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] StopProcessing controller='%s' station='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StationActor),
			bStopped ? 1 : 0);
		return;
	}

	ServerRequestShopStationStopProcessing(StationActor);
}

void AARShopPlayerController::ServerRequestShopStationStopProcessing_Implementation(AARShopStationActor* StationActor)
{
	RequestShopStationStopProcessing(StationActor);
}

void AARShopPlayerController::RequestShopStationInteract(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] Interact ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|Interact")))
		{
			return;
		}

		APawn* ControlledPawn = GetPawn();
		UARShopCarryComponent* CarryComponent = ControlledPawn ? ControlledPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
		if (!CarryComponent)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Station] Interact ignored on '%s': missing carry component (pawn='%s', station='%s')."),
				*GetNameSafe(this),
				*GetNameSafe(ControlledPawn),
				*GetNameSafe(StationActor));
			return;
		}

		if (CarryComponent->GetHeldBowlActor())
		{
			UE_LOG(ARLog, Verbose, TEXT("[Shop|Station] Interact route: held bowl -> fill station '%s'."), *GetNameSafe(StationActor));
			RequestShopFillHeldBowlFromStation(StationActor);
			return;
		}

		if (CarryComponent->GetHeldMeatActor())
		{
			if (!StationActor->GetSlottedMeatActor())
			{
				UE_LOG(ARLog, Verbose, TEXT("[Shop|Station] Interact route: held meat -> place into station '%s'."), *GetNameSafe(StationActor));
				RequestShopStationPlaceHeldMeat(StationActor);
			}
			else
			{
				UE_LOG(
					ARLog,
					Verbose,
					TEXT("[Shop|Station] Interact route blocked: held meat but station '%s' already has slotted meat '%s'."),
					*GetNameSafe(StationActor),
					*GetNameSafe(StationActor->GetSlottedMeatActor()));
			}
			return;
		}

		if (!CarryComponent->HasHeldActor() && StationActor->GetSlottedMeatActor())
		{
			UE_LOG(ARLog, Verbose, TEXT("[Shop|Station] Interact route: empty hands -> pickup slotted meat from station '%s'."), *GetNameSafe(StationActor));
			RequestShopStationPickupMeat(StationActor);
			return;
		}

		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] Interact no-op on '%s': HeldActor='%s' SlottedMeat='%s'."),
			*GetNameSafe(StationActor),
			*GetNameSafe(CarryComponent->GetHeldActor()),
			*GetNameSafe(StationActor->GetSlottedMeatActor()));
		return;
	}

	ServerRequestShopStationInteract(StationActor);
}

void AARShopPlayerController::ServerRequestShopStationInteract_Implementation(AARShopStationActor* StationActor)
{
	RequestShopStationInteract(StationActor);
}

void AARShopPlayerController::RequestShopFillHeldBowlFromStation(AARShopStationActor* StationActor)
{
	if (!StationActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Station] FillHeldBowl ignored on '%s': StationActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StationActor, TEXT("Shop|Station|FillHeldBowl")))
		{
			return;
		}

		const bool bFilled = StationActor->TryFillHeldBowlFromController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Station] FillHeldBowl controller='%s' station='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StationActor),
			bFilled ? 1 : 0);
		return;
	}

	ServerRequestShopFillHeldBowlFromStation(StationActor);
}

void AARShopPlayerController::ServerRequestShopFillHeldBowlFromStation_Implementation(AARShopStationActor* StationActor)
{
	RequestShopFillHeldBowlFromStation(StationActor);
}

void AARShopPlayerController::RequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, const FGameplayTag ItemTag)
{
	if (!DispenserActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Dispenser] Request ignored on '%s': DispenserActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(DispenserActor, TEXT("Shop|Dispenser")))
		{
			return;
		}

		const bool bDispensed = DispenserActor->TryDispenseToController(this, ItemTag);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Dispenser] Request controller='%s' dispenser='%s' item='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(DispenserActor),
			*ItemTag.ToString(),
			bDispensed ? 1 : 0);
		return;
	}

	ServerRequestShopDispenseFromDispenser(DispenserActor, ItemTag);
}

void AARShopPlayerController::ServerRequestShopDispenseFromDispenser_Implementation(AARShopDispenserActor* DispenserActor, const FGameplayTag ItemTag)
{
	RequestShopDispenseFromDispenser(DispenserActor, ItemTag);
}

void AARShopPlayerController::RequestShopPickupCarryItem(AARCarryItemBase* CarryItemActor)
{
	if (!CarryItemActor)
	{
		UARShopCarryComponent* CarryComponent = ResolveShopCarryComponentFromController(this);
		if (!CarryComponent || !CarryComponent->HasHeldActor())
		{
			return;
		}

		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Carry] Pickup request on '%s' had no target; dropping currently held actor '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(CarryComponent->GetHeldActor()));

		RequestShopDropHeldCarryItem();
		return;
	}

	if (!IsValid(CarryItemActor))
	{
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(CarryItemActor, TEXT("Shop|Carry|Pickup")))
		{
			return;
		}

		APawn* ControlledPawn = GetPawn();
		UARShopCarryComponent* CarryComponent = ResolveShopCarryComponentFromController(this);
		if (!ControlledPawn || !CarryComponent)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Carry] Pickup ignored for '%s': missing pawn or carry component."),
				*GetNameSafe(this));
			return;
		}

		if (CarryComponent->HasHeldActor())
		{
			UE_LOG(ARLog, Verbose, TEXT("[Shop|Carry] Pickup ignored for '%s': already holding '%s'."), *GetNameSafe(this), *GetNameSafe(CarryComponent->GetHeldActor()));
			return;
		}

		// Keep station-held and other attachment-owned items from being picked directly.
		if (IsAttachedToOtherActor(CarryItemActor, ControlledPawn))
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Carry] Pickup ignored for '%s': item '%s' is attached to another actor '%s'."),
				*GetNameSafe(this),
				*GetNameSafe(CarryItemActor),
				*GetNameSafe(CarryItemActor->GetAttachParentActor()));
			return;
		}

		CarryComponent->TrySetHeldActor(CarryItemActor);
		return;
	}

	ServerRequestShopPickupCarryItem(CarryItemActor);
}

void AARShopPlayerController::ServerRequestShopPickupCarryItem_Implementation(AARCarryItemBase* CarryItemActor)
{
	RequestShopPickupCarryItem(CarryItemActor);
}

void AARShopPlayerController::RequestShopDropHeldCarryItem()
{
	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveShopCarryComponentFromController(this);
		if (!CarryComponent)
		{
			return;
		}

		AActor* ReleasedActor = CarryComponent->ClearHeldActor(true);
		if (!ReleasedActor)
		{
			return;
		}

		if (UPrimitiveComponent* PhysicsPrimitive = ResolveCarryPhysicsPrimitiveForController(ReleasedActor))
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

	ServerRequestShopDropHeldCarryItem();
}

void AARShopPlayerController::ServerRequestShopDropHeldCarryItem_Implementation()
{
	RequestShopDropHeldCarryItem();
}

void AARShopPlayerController::RequestShopThrowHeldCarryItem(const float ThrowStrength)
{
	if (HasAuthority())
	{
		APawn* ControlledPawn = GetPawn();
		UARShopCarryComponent* CarryComponent = ResolveShopCarryComponentFromController(this);
		if (!ControlledPawn || !CarryComponent)
		{
			return;
		}

		AActor* ReleasedActor = CarryComponent->ClearHeldActor(true);
		if (!ReleasedActor)
		{
			return;
		}

		const FVector ThrowDirection = GetControlRotation().Vector();
		const float EffectiveThrowStrength = ResolveThrowStrengthForController(this, ThrowStrength);
		ApplyThrowImpulse(ReleasedActor, ThrowDirection, EffectiveThrowStrength);
		NotifyInteractionActionCue(EARInteractionActionCue::Throw, ReleasedActor);
		return;
	}

	ServerRequestShopThrowHeldCarryItem(ThrowStrength);
}

void AARShopPlayerController::ServerRequestShopThrowHeldCarryItem_Implementation(const float ThrowStrength)
{
	RequestShopThrowHeldCarryItem(ThrowStrength);
}

void AARShopPlayerController::RequestUseSecondaryOnHeldCarryItem()
{
	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveShopCarryComponentFromController(this);
		AARCarryItemBase* HeldCarryItem = CarryComponent ? Cast<AARCarryItemBase>(CarryComponent->GetHeldActor()) : nullptr;
		if (!HeldCarryItem)
		{
			return;
		}

		const bool bHandled = HeldCarryItem->UseSecondaryByController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Carry] SecondaryUse controller='%s' actor='%s' handled=%d."),
			*GetNameSafe(this),
			*GetNameSafe(HeldCarryItem),
			bHandled ? 1 : 0);
		return;
	}

	ServerRequestUseSecondaryOnHeldCarryItem();
}

void AARShopPlayerController::ServerRequestUseSecondaryOnHeldCarryItem_Implementation()
{
	RequestUseSecondaryOnHeldCarryItem();
}

void AARShopPlayerController::RequestConsumeHeldEnergyDrink()
{
	if (HasAuthority())
	{
		UARShopCarryComponent* CarryComponent = ResolveShopCarryComponentFromController(this);
		AAREnergyDrinkCarryItem* HeldDrink = CarryComponent ? Cast<AAREnergyDrinkCarryItem>(CarryComponent->GetHeldActor()) : nullptr;
		if (!HeldDrink)
		{
			return;
		}

		const bool bConsumed = HeldDrink->TryConsumeFromController(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|EnergyDrink] Consume controller='%s' actor='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(HeldDrink),
			bConsumed ? 1 : 0);
		if (bConsumed)
		{
			NotifyInteractionActionCue(EARInteractionActionCue::Consume, HeldDrink);
		}
		return;
	}

	ServerRequestConsumeHeldEnergyDrink();
}

void AARShopPlayerController::ServerRequestConsumeHeldEnergyDrink_Implementation()
{
	RequestConsumeHeldEnergyDrink();
}

void AARShopPlayerController::RequestFinalizeShopRunAndTravelToInvader(const FString& InInvaderTravelURL)
{
	if (HasAuthority())
	{
		AARShopGameState* ShopGameState = GetWorld() ? GetWorld()->GetGameState<AARShopGameState>() : nullptr;
		if (!ShopGameState)
		{
			UE_LOG(ARLog, Warning, TEXT("[Shop|Travel] Finalize request ignored on '%s': missing AARShopGameState."), *GetNameSafe(this));
			return;
		}

		const bool bTravelStarted = ShopGameState->FinalizeShopRunAndTravelToInvader(InInvaderTravelURL);
		UE_LOG(
			ARLog,
			Log,
			TEXT("[Shop|Travel] Finalize request controller='%s' destination='%s' success=%d."),
			*GetNameSafe(this),
			InInvaderTravelURL.IsEmpty() ? TEXT("<default>") : *InInvaderTravelURL,
			bTravelStarted ? 1 : 0);
		return;
	}

	ServerRequestFinalizeShopRunAndTravelToInvader(InInvaderTravelURL);
}

void AARShopPlayerController::ServerRequestFinalizeShopRunAndTravelToInvader_Implementation(const FString& InInvaderTravelURL)
{
	RequestFinalizeShopRunAndTravelToInvader(InInvaderTravelURL);
}
