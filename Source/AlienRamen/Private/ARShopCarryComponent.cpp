#include "ARShopCarryComponent.h"

#include "ARLog.h"
#include "ARRamenBowlActor.h"
#include "ARRamenMeatActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static UPrimitiveComponent* ResolveShopCarryPhysicsPrimitive(AActor* Actor)
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
}

UARShopCarryComponent::UARShopCarryComponent()
{
	SetIsReplicatedByDefault(true);
}

AARRamenMeatActor* UARShopCarryComponent::GetHeldMeatActor() const
{
	return Cast<AARRamenMeatActor>(HeldActor);
}

AARRamenBowlActor* UARShopCarryComponent::GetHeldBowlActor() const
{
	return Cast<AARRamenBowlActor>(HeldActor);
}

bool UARShopCarryComponent::TrySetHeldActor(AActor* NewHeldActor)
{
	if (!IsAuthorityOwner() || !IsValid(NewHeldActor) || HeldActor != nullptr)
	{
		return false;
	}

	AActor* OldHeldActor = HeldActor;
	HeldActor = NewHeldActor;
	ApplyHoldPresentation(HeldActor);
	OnHeldActorChanged.Broadcast(HeldActor, OldHeldActor);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	return true;
}

AActor* UARShopCarryComponent::ClearHeldActor(const bool bDropInWorld)
{
	if (!IsAuthorityOwner() || HeldActor == nullptr)
	{
		return nullptr;
	}

	AActor* Released = HeldActor;
	ClearHoldPresentation(Released, bDropInWorld);
	HeldActor = nullptr;
	OnHeldActorChanged.Broadcast(nullptr, Released);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	return Released;
}

AActor* UARShopCarryComponent::ReleaseHeldActorForTransfer()
{
	if (!IsAuthorityOwner() || HeldActor == nullptr)
	{
		return nullptr;
	}

	AActor* Released = HeldActor;
	HeldActor = nullptr;
	OnHeldActorChanged.Broadcast(nullptr, Released);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	return Released;
}

bool UARShopCarryComponent::HasCompletedHeldBowl(FARRamenBowlSpec& OutBowlSpec, AARRamenBowlActor*& OutBowlActor) const
{
	OutBowlActor = Cast<AARRamenBowlActor>(HeldActor);
	if (!OutBowlActor || !OutBowlActor->IsComplete())
	{
		return false;
	}

	OutBowlSpec = OutBowlActor->GetBowlSpec();
	return true;
}

void UARShopCarryComponent::OnRep_HeldActor(AActor* OldHeldActor)
{
	if (OldHeldActor)
	{
		ClearHoldPresentation(OldHeldActor, true);
	}
	if (HeldActor)
	{
		ApplyHoldPresentation(HeldActor);
	}

	OnHeldActorChanged.Broadcast(HeldActor, OldHeldActor);
}

void UARShopCarryComponent::ApplyHoldPresentation(AActor* ActorToHold) const
{
	if (!ActorToHold)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	USceneComponent* OwnerRoot = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
	USceneComponent* HeldRoot = ActorToHold->GetRootComponent();

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	ActorToHold->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		if (Primitive->IsSimulatingPhysics())
		{
			Primitive->SetSimulatePhysics(false);
		}

		Primitive->SetEnableGravity(false);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (OwnerRoot && HeldRoot)
	{
		HeldRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		const bool bAttached = HeldRoot->AttachToComponent(OwnerRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HoldAttachSocketName);
		if (bAttached)
		{
			HeldRoot->SetRelativeLocation(HoldRelativeLocation);
			HeldRoot->SetRelativeRotation(HoldRelativeRotation);
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Shop|Carry] Failed to attach '%s' to owner '%s'."),
				*GetNameSafe(ActorToHold),
				*GetNameSafe(OwnerActor));
			ActorToHold->SetActorLocation(OwnerRoot->GetComponentTransform().TransformPosition(HoldRelativeLocation));
		}
	}

	ActorToHold->SetActorEnableCollision(false);
}

void UARShopCarryComponent::ClearHoldPresentation(AActor* ActorToRelease, const bool bDropInWorld) const
{
	if (!ActorToRelease)
	{
		return;
	}

	bool bWasAttachedToOwner = false;
	bool bShouldRestoreForWorldDrop = false;
	if (USceneComponent* HeldRoot = ActorToRelease->GetRootComponent())
	{
		const USceneComponent* OwnerRoot = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
		const USceneComponent* AttachParent = HeldRoot->GetAttachParent();
		bWasAttachedToOwner = (AttachParent == OwnerRoot);
		const bool bHasAttachParent = AttachParent != nullptr;
		bShouldRestoreForWorldDrop = bWasAttachedToOwner || (bDropInWorld && !bHasAttachParent);
		if (bWasAttachedToOwner)
		{
			HeldRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}

		if (bShouldRestoreForWorldDrop)
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			ActorToRelease->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
			for (UPrimitiveComponent* Primitive : PrimitiveComponents)
			{
				if (!Primitive)
				{
					continue;
				}

				Primitive->SetCollisionEnabled(bDropInWorld ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
				Primitive->SetEnableGravity(bDropInWorld);
			}

			if (UPrimitiveComponent* PhysicsPrimitive = ResolveShopCarryPhysicsPrimitive(ActorToRelease))
			{
				if (bDropInWorld)
				{
					if (!PhysicsPrimitive->IsSimulatingPhysics())
					{
						PhysicsPrimitive->SetSimulatePhysics(true);
					}
					PhysicsPrimitive->WakeAllRigidBodies();
				}
				else if (PhysicsPrimitive->IsSimulatingPhysics())
				{
					PhysicsPrimitive->SetSimulatePhysics(false);
				}
			}
		}
	}

	if (bShouldRestoreForWorldDrop)
	{
		ActorToRelease->SetActorEnableCollision(bDropInWorld);
	}
}

bool UARShopCarryComponent::IsAuthorityOwner() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UARShopCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UARShopCarryComponent, HeldActor);
}
