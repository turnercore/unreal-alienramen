#include "ARMeatStorageBoxActor.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARRamenMeatActor.h"
#include "ARSaveTypes.h"
#include "ARShopCarryComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	static AARPlayerController* ResolveControllerFromUsingActor(AActor* UsingActor)
	{
		AARPlayerController* UsingController = Cast<AARPlayerController>(UsingActor);
		if (!UsingController)
		{
			const APawn* UsingPawn = Cast<APawn>(UsingActor);
			UsingController = UsingPawn ? Cast<AARPlayerController>(UsingPawn->GetController()) : nullptr;
		}

		return UsingController;
	}

	static int32* ResolveMeatBucketForColor(FARMeatState& MeatState, const EARAffinityColor SourceColor)
	{
		switch (SourceColor)
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

bool AARMeatStorageBoxActor::TryStoreHeldMeat(AARPlayerController* RequestingController)
{
	if (!HasAuthority() || !RequestingController)
	{
		return false;
	}

	APawn* ControlledPawn = RequestingController->GetPawn();
	UARShopCarryComponent* CarryComponent = ControlledPawn ? ControlledPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	AARRamenMeatActor* HeldMeat = CarryComponent ? CarryComponent->GetHeldMeatActor() : nullptr;
	if (!CarryComponent || !HeldMeat)
	{
		return false;
	}

	return TryStoreMeatActorInternal(HeldMeat, RequestingController, false);
}

bool AARMeatStorageBoxActor::TryStoreWorldMeat(AARRamenMeatActor* MeatActor)
{
	return TryStoreMeatActorInternal(MeatActor, nullptr, true);
}

bool AARMeatStorageBoxActor::TryStoreMeatActorInternal(
	AARRamenMeatActor* MeatActor,
	AARPlayerController* RequestingController,
	const bool bRequireWorldReturnArmed)
{
	if (!HasAuthority() || !MeatActor)
	{
		return false;
	}

	const EARAffinityColor HeldColor = MeatActor->GetMeatColor();
	const int32 HeldAmount = FMath::Max(1, MeatActor->GetMeatAmount());
	if (MeatColor != EARAffinityColor::None && HeldColor != MeatColor)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryStoreHeldMeat rejected storage='%s': held color (%d) does not match storage color (%d)."),
			*GetNameSafe(this),
			static_cast<int32>(HeldColor),
			static_cast<int32>(MeatColor));
		return false;
	}

	if (bRequireWorldReturnArmed && !MeatActor->HasMovedAwayForStorageReturn(MinWorldAutoStoreTravelDistance))
	{
		UE_LOG(
			ARLog,
			VeryVerbose,
			TEXT("[Shop|Storage] World meat return gated storage='%s': meat '%s' has not moved away enough (required=%.1f)."),
			*GetNameSafe(this),
			*GetNameSafe(MeatActor),
			MinWorldAutoStoreTravelDistance);
		return false;
	}

	AARGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	FARMeatState NewMeatState = GameState->GetMeat();
	int32* Bucket = ResolveMeatBucketForColor(NewMeatState, HeldColor);
	if (!Bucket)
	{
		return false;
	}

	*Bucket = FMath::Max(0, *Bucket) + HeldAmount;
	GameState->SetMeatFromSave(NewMeatState);

	if (RequestingController)
	{
		APawn* ControlledPawn = RequestingController->GetPawn();
		UARShopCarryComponent* CarryComponent = ControlledPawn ? ControlledPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
		if (CarryComponent && CarryComponent->GetHeldActor() == MeatActor)
		{
			CarryComponent->ReleaseHeldActorForTransfer();
		}
	}
	MeatActor->ReleaseCarryItem();

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Storage] Stored meat storage='%s' controller='%s' meat='%s' color=%d amount=%d worldReturn=%d."),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		*GetNameSafe(MeatActor),
		static_cast<int32>(HeldColor),
		HeldAmount,
		bRequireWorldReturnArmed ? 1 : 0);
	return true;
}

void AARMeatStorageBoxActor::ForwardUseToController(AActor* UsingActor)
{
	AARPlayerController* Controller = ResolveControllerFromUsingActor(UsingActor);
	if (!Controller)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] ForwardUse ignored on '%s': unable to resolve controller from '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(UsingActor));
		return;
	}

	TryHandleStorageInteraction(Controller);
}

bool AARMeatStorageBoxActor::TryHandleStorageInteraction(AARPlayerController* RequestingController)
{
	if (!HasAuthority() || !RequestingController)
	{
		return false;
	}

	APawn* ControlledPawn = RequestingController->GetPawn();
	UARShopCarryComponent* CarryComponent = ControlledPawn ? ControlledPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	if (CarryComponent && CarryComponent->GetHeldMeatActor())
	{
		return TryStoreHeldMeat(RequestingController);
	}

	return TryDispenseMeat(RequestingController);
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
