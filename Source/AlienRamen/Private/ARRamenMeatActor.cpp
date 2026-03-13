#include "ARRamenMeatActor.h"

#include "ARMeatStorageBoxActor.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

AARRamenMeatActor::AARRamenMeatActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AARRamenMeatActor::SetMeatData(const EARAffinityColor NewColor, const int32 NewAmount)
{
	if (!HasAuthority())
	{
		return;
	}

	MeatColor = NewColor;
	MeatAmount = FMath::Max(1, NewAmount);
	ForceNetUpdate();
}

bool AARRamenMeatActor::HasMovedAwayForStorageReturn(const float RequiredDistance) const
{
	if (bStorageReturnArmedByPickup)
	{
		return true;
	}

	if (RequiredDistance <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	return MaxDistanceFromSpawnSq >= FMath::Square(RequiredDistance);
}

void AARRamenMeatActor::ArmStorageReturn()
{
	if (!HasAuthority())
	{
		return;
	}

	bStorageReturnArmedByPickup = true;
}

void AARRamenMeatActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnLocationForStorageReturn = GetActorLocation();
	MaxDistanceFromSpawnSq = 0.0f;
	bStorageReturnArmedByPickup = false;
}

void AARRamenMeatActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	const float DistanceSq = FVector::DistSquared(GetActorLocation(), SpawnLocationForStorageReturn);
	MaxDistanceFromSpawnSq = FMath::Max(MaxDistanceFromSpawnSq, DistanceSq);
}

void AARRamenMeatActor::NotifyHit(
	UPrimitiveComponent* MyComp,
	AActor* Other,
	UPrimitiveComponent* OtherComp,
	const bool bSelfMoved,
	FVector HitLocation,
	FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	TryAutoStoreWithActor(Other);
}

void AARRamenMeatActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	TryAutoStoreWithActor(OtherActor);
}

void AARRamenMeatActor::TryAutoStoreWithActor(AActor* OtherActor)
{
	if (!HasAuthority() || !IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	const USceneComponent* ActorRootComponent = GetRootComponent();
	if (ActorRootComponent && ActorRootComponent->GetAttachParent())
	{
		return;
	}

	AARMeatStorageBoxActor* StorageActor = Cast<AARMeatStorageBoxActor>(OtherActor);
	if (!StorageActor)
	{
		return;
	}

	StorageActor->TryStoreWorldMeat(this);
}

void AARRamenMeatActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARRamenMeatActor, MeatColor);
	DOREPLIFETIME(AARRamenMeatActor, MeatAmount);
}
