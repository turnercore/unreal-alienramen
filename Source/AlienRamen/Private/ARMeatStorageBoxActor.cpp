#include "ARMeatStorageBoxActor.h"

#include "ARGameStateBase.h"
#include "ARPlayerController.h"
#include "ARRamenMeatActor.h"
#include "ARSaveTypes.h"
#include "ARShopCarryComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	static int32* ResolveMeatBucket(FARMeatState& MeatState, const EARAffinityColor Color)
	{
		switch (Color)
		{
		case EARAffinityColor::Red:
			return &MeatState.RedAmount;
		case EARAffinityColor::Blue:
			return &MeatState.BlueAmount;
		case EARAffinityColor::White:
			return &MeatState.WhiteAmount;
		default:
			return &MeatState.UnspecifiedAmount;
		}
	}
}

AARMeatStorageBoxActor::AARMeatStorageBoxActor()
{
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnAnchor"));
	SpawnAnchor->SetupAttachment(SceneRoot);
}

bool AARMeatStorageBoxActor::TryDispenseMeat(AARPlayerController* RequestingController)
{
	if (!HasAuthority())
	{
		return false;
	}

	AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!ARGameState)
	{
		return false;
	}

	FARMeatState MeatState = ARGameState->GetMeat();
	int32* Bucket = ResolveMeatBucket(MeatState, MeatColor);
	const int32 AmountToDispense = FMath::Max(1, MeatAmountPerDispense);
	if (!Bucket || *Bucket < AmountToDispense)
	{
		return false;
	}

	*Bucket -= AmountToDispense;
	ARGameState->SetMeatFromSave(MeatState);

	TSubclassOf<AARRamenMeatActor> SpawnClass = MeatActorClass;
	if (!SpawnClass)
	{
		SpawnClass = AARRamenMeatActor::StaticClass();
	}

	const FVector SpawnLocation = SpawnAnchor ? SpawnAnchor->GetComponentLocation() : GetActorLocation();
	const FRotator SpawnRotation = SpawnAnchor ? SpawnAnchor->GetComponentRotation() : GetActorRotation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AARRamenMeatActor* SpawnedMeat = GetWorld()->SpawnActor<AARRamenMeatActor>(SpawnClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!SpawnedMeat)
	{
		*Bucket += AmountToDispense;
		ARGameState->SetMeatFromSave(MeatState);
		return false;
	}

	SpawnedMeat->SetMeatData(MeatColor, AmountToDispense);

	APawn* RequestingPawn = RequestingController ? RequestingController->GetPawn() : nullptr;
	UARShopCarryComponent* CarryComponent = RequestingPawn ? RequestingPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	if (CarryComponent && !CarryComponent->HasHeldActor())
	{
		CarryComponent->TrySetHeldActor(SpawnedMeat);
	}

	return true;
}
