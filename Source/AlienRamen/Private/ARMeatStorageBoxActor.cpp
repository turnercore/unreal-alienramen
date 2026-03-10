#include "ARMeatStorageBoxActor.h"

#include "ARPlayerController.h"
#include "ARRamenMeatActor.h"

AARMeatStorageBoxActor::AARMeatStorageBoxActor()
{
}

void AARMeatStorageBoxActor::BeginPlay()
{
	Super::BeginPlay();
	SyncLegacyDefinition();
}

bool AARMeatStorageBoxActor::TryDispenseMeat(AARPlayerController* RequestingController)
{
	SyncLegacyDefinition();
	return TryDispenseToController(RequestingController, MeatItemTag);
}

void AARMeatStorageBoxActor::SyncLegacyDefinition()
{
	FARShopDispenseDefinition Definition;
	Definition.ItemTag = MeatItemTag;
	Definition.SpawnActorClass = MeatActorClass;
	Definition.AmountPerDispense = FMath::Max(1, MeatAmountPerDispense);
	Definition.SourceType = EARShopDispenserSourceType::GameStateMeatReserve;
	Definition.SourceColor = MeatColor;
	Definition.bAutoPlaceIntoCarry = true;

	if (DispenseDefinitions.Num() <= 0)
	{
		DispenseDefinitions.Add(Definition);
		return;
	}

	DispenseDefinitions.SetNum(1, EAllowShrinking::No);
	DispenseDefinitions[0] = Definition;
}
