#include "ARMeatStorageBoxActor.h"

#include "ARLog.h"
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
	const bool bDispensed = TryDispenseToController(RequestingController, MeatItemTag);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Storage] TryDispenseMeat storage='%s' controller='%s' color=%d amountPerDispense=%d item='%s' success=%d."),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		static_cast<int32>(MeatColor),
		FMath::Max(1, MeatAmountPerDispense),
		*MeatItemTag.ToString(),
		bDispensed ? 1 : 0);
	return bDispensed;
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
