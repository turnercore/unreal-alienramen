#include "ARShopPlayerController.h"

#include "ARLog.h"
#include "ARNPCCharacterBase.h"
#include "ARShopCarryComponent.h"
#include "ARShopCarryItemBase.h"
#include "ARShopDispenserActor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	static UARShopCarryComponent* ResolveShopCarryComponentFromController(AARShopPlayerController* Controller)
	{
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	}

	static UPrimitiveComponent* ResolveCarryPhysicsPrimitive(AActor* Actor)
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

		UPrimitiveComponent* PhysicsPrimitive = ResolveCarryPhysicsPrimitive(ReleasedActor);
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

void AARShopPlayerController::RequestShopDispenseFromDispenser(AARShopDispenserActor* DispenserActor, const FGameplayTag ItemTag)
{
	if (!DispenserActor)
	{
		return;
	}

	if (HasAuthority())
	{
		DispenserActor->TryDispenseToController(this, ItemTag);
		return;
	}

	ServerRequestShopDispenseFromDispenser(DispenserActor, ItemTag);
}

void AARShopPlayerController::ServerRequestShopDispenseFromDispenser_Implementation(AARShopDispenserActor* DispenserActor, const FGameplayTag ItemTag)
{
	RequestShopDispenseFromDispenser(DispenserActor, ItemTag);
}

void AARShopPlayerController::RequestShopPickupCarryItem(AARShopCarryItemBase* CarryItemActor)
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

	if (HasAuthority())
	{
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

void AARShopPlayerController::ServerRequestShopPickupCarryItem_Implementation(AARShopCarryItemBase* CarryItemActor)
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

		if (UPrimitiveComponent* PhysicsPrimitive = ResolveCarryPhysicsPrimitive(ReleasedActor))
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
		ApplyThrowImpulse(ReleasedActor, ThrowDirection, ThrowStrength);
		return;
	}

	ServerRequestShopThrowHeldCarryItem(ThrowStrength);
}

void AARShopPlayerController::ServerRequestShopThrowHeldCarryItem_Implementation(const float ThrowStrength)
{
	RequestShopThrowHeldCarryItem(ThrowStrength);
}
