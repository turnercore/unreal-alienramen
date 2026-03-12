#include "ARShopCarryItemBase.h"

#include "ARLog.h"
#include "ARShopPlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AARShopCarryItemBase::AARShopCarryItemBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void AARShopCarryItemBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyWeightToPrimitiveComponents();
}

float AARShopCarryItemBase::GetResolvedWeightKg() const
{
	return WeightKg > 0.0f ? WeightKg : ResolveDefaultWeightKg();
}

void AARShopCarryItemBase::SetWeightKg(const float NewWeightKg)
{
	if (!HasAuthority())
	{
		return;
	}

	const float SanitizedWeight = FMath::Max(0.0f, NewWeightKg);
	if (FMath::IsNearlyEqual(WeightKg, SanitizedWeight))
	{
		return;
	}

	WeightKg = SanitizedWeight;
	ApplyWeightToPrimitiveComponents();
	ForceNetUpdate();
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

void AARShopCarryItemBase::OnRep_WeightKg()
{
	ApplyWeightToPrimitiveComponents();
}

void AARShopCarryItemBase::ApplyWeightToPrimitiveComponents() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		if (WeightKg > 0.0f)
		{
			Primitive->SetMassOverrideInKg(NAME_None, WeightKg, true);
		}
		else
		{
			Primitive->SetMassOverrideInKg(NAME_None, 0.0f, false);
		}
	}
}

float AARShopCarryItemBase::ResolveDefaultWeightKg() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (const UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		const float PrimitiveMass = Primitive->GetMass();
		if (PrimitiveMass > KINDA_SMALL_NUMBER)
		{
			return PrimitiveMass;
		}
	}

	return 0.0f;
}

void AARShopCarryItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARShopCarryItemBase, WeightKg);
}
