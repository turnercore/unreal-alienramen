#include "ARMeatStorageBoxActor.h"

#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARRamenMeatActor.h"
#include "ARSaveTypes.h"
#include "ARShopCarryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
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

	static EARAffinityColor SanitizeColor(const EARAffinityColor InColor)
	{
		return InColor == EARAffinityColor::Unknown ? EARAffinityColor::None : InColor;
	}
}

AARMeatStorageBoxActor::AARMeatStorageBoxActor()
{
	StorageRootMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StorageRootMesh"));
	SetRootComponent(StorageRootMesh);

	if (SceneRoot)
	{
		SceneRoot->SetupAttachment(StorageRootMesh);
	}

	if (SpawnAnchor)
	{
		SpawnAnchor->SetupAttachment(StorageRootMesh);
	}
}

void AARMeatStorageBoxActor::BeginPlay()
{
	Super::BeginPlay();
	SyncLegacyDefinition();
}

bool AARMeatStorageBoxActor::TryDispenseMeat(AARPlayerController* RequestingController)
{
	return TryDispenseRandomMeatByContainerColor(RequestingController);
}

bool AARMeatStorageBoxActor::TryDispenseSpecificMeat(AARPlayerController* RequestingController, const FGameplayTag MeatTag)
{
	if (!HasAuthority() || !RequestingController || !MeatTag.IsValid())
	{
		return false;
	}

	UGameInstance* GI = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	FARMeatDefinitionRow MeatDefinition;
	if (!ItemDefinitions || !ItemDefinitions->ResolveMeatDefinition(MeatTag, MeatDefinition))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseSpecificMeat failed storage='%s': unresolved meat tag '%s'."),
			*GetNameSafe(this),
			*MeatTag.ToString());
		return false;
	}

	AARGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	const FARMeatState MeatState = GameState->GetMeat();
	const int32 DispenseAmount = FMath::Max(1, MeatAmountPerDispense);
	FARMeatTypeAmount SelectedEntry;
	for (const FARMeatTypeAmount& Entry : MeatState.AdditionalAmountsByType)
	{
		if (!Entry.MeatType.MatchesTagExact(MeatTag) || Entry.Amount < DispenseAmount)
		{
			continue;
		}

		SelectedEntry = Entry;
		break;
	}

	if (!SelectedEntry.MeatType.IsValid())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseSpecificMeat failed storage='%s': no eligible tuple stock for '%s'."),
			*GetNameSafe(this),
			*MeatTag.ToString());
		return false;
	}

	return TryDispenseResolvedMeat(RequestingController, MeatDefinition, SelectedEntry.MeatColor, SelectedEntry.MeatQualityTier);
}

bool AARMeatStorageBoxActor::TryDispenseRandomMeatByContainerColor(AARPlayerController* RequestingController)
{
	if (!HasAuthority() || !RequestingController)
	{
		return false;
	}

	AARGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	UGameInstance* GI = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!ItemDefinitions)
	{
		return false;
	}

	const FARMeatState CandidateState = GameState->GetMeat();
	FARMeatTypeAmount RandomMeatEntry;
	if (!SelectRandomEligibleMeatTupleFromTypedStock(CandidateState, RandomMeatEntry))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseRandomMeatByContainerColor failed storage='%s': no eligible meat tuples for color=%d."),
			*GetNameSafe(this),
			static_cast<int32>(SanitizeColor(MeatColor)));
		return false;
	}

	FARMeatDefinitionRow MeatDefinition;
	if (!ItemDefinitions->ResolveMeatDefinition(RandomMeatEntry.MeatType, MeatDefinition))
	{
		return false;
	}

	return TryDispenseResolvedMeat(RequestingController, MeatDefinition, RandomMeatEntry.MeatColor, RandomMeatEntry.MeatQualityTier);
}

bool AARMeatStorageBoxActor::TryDispenseResolvedMeat(
	AARPlayerController* RequestingController,
	const FARMeatDefinitionRow& MeatDefinition,
	const EARAffinityColor MeatColorToDispense,
	const EARVendingQualityTier MeatQualityToDispense)
{
	if (!HasAuthority() || !RequestingController || !MeatDefinition.MeatTag.IsValid())
	{
		return false;
	}

	APawn* ControlledPawn = RequestingController->GetPawn();
	UARShopCarryComponent* CarryComponent = ControlledPawn ? ControlledPawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	if (!CarryComponent || CarryComponent->HasHeldActor())
	{
		return false;
	}

	AARGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	const int32 DispenseAmount = FMath::Max(1, MeatAmountPerDispense);
	FARMeatState NewMeatState = GameState->GetMeat();
	if (!ConsumeTypedMeatFromState(NewMeatState, MeatDefinition.MeatTag, MeatColorToDispense, MeatQualityToDispense, DispenseAmount))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseResolvedMeat failed storage='%s': insufficient stock for '%s' color=%d quality=%d amount=%d."),
			*GetNameSafe(this),
			*MeatDefinition.MeatTag.ToString(),
			static_cast<int32>(MeatColorToDispense),
			static_cast<int32>(MeatQualityToDispense),
			DispenseAmount);
		return false;
	}

	const FVector SpawnLocation = SpawnAnchor ? SpawnAnchor->GetComponentLocation() : GetActorLocation();
	const FRotator SpawnRotation = SpawnAnchor ? SpawnAnchor->GetComponentRotation() : GetActorRotation();
	UClass* SpawnClass = MeatActorClass ? MeatActorClass.Get() : AARRamenMeatActor::StaticClass();
	if (!SpawnClass || !SpawnClass->IsChildOf(AARRamenMeatActor::StaticClass()))
	{
		SpawnClass = AARRamenMeatActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AARRamenMeatActor* SpawnedMeat = GetWorld()->SpawnActor<AARRamenMeatActor>(SpawnClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!SpawnedMeat)
	{
		return false;
	}

	const EARAffinityColor SpawnColor = SanitizeColor(MeatColorToDispense);
	SpawnedMeat->SetMeatDataByTag(MeatDefinition.MeatTag, SpawnColor, DispenseAmount, MeatQualityToDispense);
	if (!CarryComponent->TrySetHeldActor(SpawnedMeat))
	{
		SpawnedMeat->ReleaseCarryItem();
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseResolvedMeat failed storage='%s': could not place spawned meat '%s' into carry slot."),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedMeat));
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARItemDefinitionSubsystem* ItemDefinitions = GI->GetSubsystem<UARItemDefinitionSubsystem>())
		{
			if (MeatDefinition.MeatTag.IsValid())
			{
				ItemDefinitions->ApplyItemPhysicsProperties(SpawnedMeat, MeatDefinition.MeatTag);
			}
		}
	}

	GameState->SetMeatFromSave(NewMeatState);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Storage] Dispensed meat storage='%s' controller='%s' meatTag='%s' color=%d quality=%d amount=%d."),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		*MeatDefinition.MeatTag.ToString(),
		static_cast<int32>(SpawnColor),
		static_cast<int32>(MeatQualityToDispense),
		DispenseAmount);
	return true;
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

	UGameInstance* GI = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!ItemDefinitions)
	{
		return false;
	}

	FGameplayTag DepositMeatTag = MeatActor->GetMeatTag();
	EARAffinityColor DepositColor = SanitizeColor(MeatActor->GetMeatColor());
	FARMeatDefinitionRow DepositDefinition;
	if (DepositMeatTag.IsValid() && ItemDefinitions->ResolveMeatDefinition(DepositMeatTag, DepositDefinition))
	{
		if (DepositDefinition.MeatTag.IsValid())
		{
			DepositMeatTag = DepositDefinition.MeatTag;
		}
	}
	else
	{
		if (DepositColor == EARAffinityColor::None)
		{
			DepositColor = SanitizeColor(MeatColor);
		}

		if (!ItemDefinitions->ResolveFirstMeatTag(DepositMeatTag))
		{
			return false;
		}

		ItemDefinitions->ResolveMeatDefinition(DepositMeatTag, DepositDefinition);
	}

	const EARAffinityColor StorageColor = SanitizeColor(MeatColor);
	if (StorageColor != EARAffinityColor::None && DepositColor != StorageColor)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] Store rejected storage='%s': deposit color (%d) does not match storage color (%d)."),
			*GetNameSafe(this),
			static_cast<int32>(DepositColor),
			static_cast<int32>(StorageColor));
		return false;
	}

	AARGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	const int32 HeldAmount = FMath::Max(1, MeatActor->GetMeatAmount());
	const EARVendingQualityTier HeldQualityTier = MeatActor->GetMeatQualityTier();
	FARMeatState NewMeatState = GameState->GetMeat();
	AddTypedMeatToState(NewMeatState, DepositMeatTag, DepositColor, HeldQualityTier, HeldAmount);
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
		TEXT("[Shop|Storage] Stored meat storage='%s' controller='%s' meat='%s' meatTag='%s' color=%d quality=%d amount=%d worldReturn=%d."),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		*GetNameSafe(MeatActor),
		*DepositMeatTag.ToString(),
		static_cast<int32>(DepositColor),
		static_cast<int32>(HeldQualityTier),
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
	Definition.SourceMeatQualityTier = EARVendingQualityTier::Standard;
	Definition.bAutoPlaceIntoCarry = true;

	if (DispenseDefinitions.Num() <= 0)
	{
		DispenseDefinitions.Add(Definition);
		return;
	}

	DispenseDefinitions.SetNum(1, EAllowShrinking::No);
	DispenseDefinitions[0] = Definition;
}

bool AARMeatStorageBoxActor::ConsumeTypedMeatFromState(
	FARMeatState& InOutMeatState,
	const FGameplayTag MeatTag,
	const EARAffinityColor MeatColorToConsume,
	const EARVendingQualityTier MeatQualityToConsume,
	const int32 AmountToConsume) const
{
	if (!MeatTag.IsValid() || AmountToConsume <= 0)
	{
		return false;
	}

	for (FARMeatTypeAmount& Entry : InOutMeatState.AdditionalAmountsByType)
	{
		if (!Entry.MeatType.MatchesTagExact(MeatTag)
			|| Entry.MeatColor != (MeatColorToConsume == EARAffinityColor::Unknown ? EARAffinityColor::None : MeatColorToConsume)
			|| Entry.MeatQualityTier != MeatQualityToConsume)
		{
			continue;
		}

		if (Entry.Amount < AmountToConsume)
		{
			return false;
		}

		Entry.Amount -= AmountToConsume;
		InOutMeatState.NormalizeAdditionalAmounts();
		return true;
	}

	return false;
}

void AARMeatStorageBoxActor::AddTypedMeatToState(
	FARMeatState& InOutMeatState,
	const FGameplayTag MeatTag,
	const EARAffinityColor MeatColorValue,
	const EARVendingQualityTier MeatQualityTier,
	const int32 AmountToAdd) const
{
	if (!MeatTag.IsValid() || AmountToAdd <= 0)
	{
		return;
	}

	for (FARMeatTypeAmount& Entry : InOutMeatState.AdditionalAmountsByType)
	{
		if (Entry.MeatType.MatchesTagExact(MeatTag)
			&& Entry.MeatColor == (MeatColorValue == EARAffinityColor::Unknown ? EARAffinityColor::None : MeatColorValue)
			&& Entry.MeatQualityTier == MeatQualityTier)
		{
			Entry.Amount = FMath::Max(0, Entry.Amount) + AmountToAdd;
			InOutMeatState.NormalizeAdditionalAmounts();
			return;
		}
	}

	FARMeatTypeAmount& Added = InOutMeatState.AdditionalAmountsByType.AddDefaulted_GetRef();
	Added.MeatType = MeatTag;
	Added.MeatColor = MeatColorValue == EARAffinityColor::Unknown ? EARAffinityColor::None : MeatColorValue;
	Added.MeatQualityTier = MeatQualityTier;
	Added.Amount = AmountToAdd;
	InOutMeatState.NormalizeAdditionalAmounts();
}

bool AARMeatStorageBoxActor::SelectRandomEligibleMeatTupleFromTypedStock(const FARMeatState& MeatState, FARMeatTypeAmount& OutMeatEntry) const
{
	OutMeatEntry = FARMeatTypeAmount();

	TArray<FARMeatTypeAmount> EligibleEntries;
	const int32 DispenseAmount = FMath::Max(1, MeatAmountPerDispense);
	for (const FARMeatTypeAmount& Entry : MeatState.AdditionalAmountsByType)
	{
		if (!Entry.MeatType.IsValid())
		{
			continue;
		}

		const EARAffinityColor EntryColor = Entry.MeatColor == EARAffinityColor::Unknown ? EARAffinityColor::None : Entry.MeatColor;
		const EARAffinityColor StorageColor = SanitizeColor(MeatColor);
		if (StorageColor != EARAffinityColor::None && EntryColor != StorageColor)
		{
			continue;
		}

		if (Entry.Amount >= DispenseAmount)
		{
			EligibleEntries.Add(Entry);
		}
	}

	if (EligibleEntries.IsEmpty())
	{
		return false;
	}

	const int32 RandomIndex = FMath::RandRange(0, EligibleEntries.Num() - 1);
	OutMeatEntry = EligibleEntries[RandomIndex];
	return OutMeatEntry.MeatType.IsValid();
}
