#include "ARShopCarryComponent.h"

#include "ARRamenBowlActor.h"
#include "ARRamenMeatActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

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
	if (OwnerRoot && HeldRoot)
	{
		HeldRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		HeldRoot->AttachToComponent(OwnerRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HoldAttachSocketName);
		HeldRoot->SetRelativeLocation(HoldRelativeLocation);
		HeldRoot->SetRelativeRotation(HoldRelativeRotation);
	}

	if (UPrimitiveComponent* PrimitiveRoot = Cast<UPrimitiveComponent>(HeldRoot))
	{
		PrimitiveRoot->SetSimulatePhysics(false);
		PrimitiveRoot->SetEnableGravity(false);
		PrimitiveRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	if (USceneComponent* HeldRoot = ActorToRelease->GetRootComponent())
	{
		const USceneComponent* OwnerRoot = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
		bWasAttachedToOwner = (HeldRoot->GetAttachParent() == OwnerRoot);
		if (bWasAttachedToOwner)
		{
			HeldRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			if (UPrimitiveComponent* PrimitiveRoot = Cast<UPrimitiveComponent>(HeldRoot))
			{
				PrimitiveRoot->SetCollisionEnabled(bDropInWorld ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			}
		}
	}

	if (bWasAttachedToOwner)
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
