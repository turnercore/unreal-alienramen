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

	return TryDispenseResolvedMeat(RequestingController, MeatDefinition);
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

	FARMeatState CandidateState = GameState->GetMeat();
	PromoteLegacyBucketsToTyped(CandidateState);
	RebuildLegacyColorBucketsFromTyped(CandidateState);

	FGameplayTag RandomMeatTag;
	if (!SelectRandomEligibleMeatTagFromTypedStock(CandidateState, RandomMeatTag))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseRandomMeatByContainerColor failed storage='%s': no eligible meat tags for color=%d."),
			*GetNameSafe(this),
			static_cast<int32>(SanitizeColor(MeatColor)));
		return false;
	}

	return TryDispenseSpecificMeat(RequestingController, RandomMeatTag);
}

bool AARMeatStorageBoxActor::TryDispenseResolvedMeat(AARPlayerController* RequestingController, const FARMeatDefinitionRow& MeatDefinition)
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
	PromoteLegacyBucketsToTyped(NewMeatState);
	if (!ConsumeTypedMeatFromState(NewMeatState, MeatDefinition.MeatTag, DispenseAmount))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] TryDispenseResolvedMeat failed storage='%s': insufficient stock for '%s' amount=%d."),
			*GetNameSafe(this),
			*MeatDefinition.MeatTag.ToString(),
			DispenseAmount);
		return false;
	}
	RebuildLegacyColorBucketsFromTyped(NewMeatState);

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

	// Meat type and color are decoupled at runtime; storage color drives dispensed actor color.
	const EARAffinityColor SpawnColor = SanitizeColor(MeatColor);
	SpawnedMeat->SetMeatDataByTag(MeatDefinition.MeatTag, SpawnColor, DispenseAmount);
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
			if (MeatDefinition.ItemTag.IsValid())
			{
				ItemDefinitions->ApplyItemPhysicsProperties(SpawnedMeat, MeatDefinition.ItemTag);
			}
		}
	}

	GameState->SetMeatFromSave(NewMeatState);
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Storage] Dispensed meat storage='%s' controller='%s' meatTag='%s' color=%d amount=%d."),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		*MeatDefinition.MeatTag.ToString(),
		static_cast<int32>(SpawnColor),
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

		if (!ItemDefinitions->ResolveFirstMeatTagForColor(DepositColor, DepositMeatTag))
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
	FARMeatState NewMeatState = GameState->GetMeat();
	PromoteLegacyBucketsToTyped(NewMeatState);
	AddTypedMeatToState(NewMeatState, DepositMeatTag, HeldAmount);
	RebuildLegacyColorBucketsFromTyped(NewMeatState);
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
		TEXT("[Shop|Storage] Stored meat storage='%s' controller='%s' meat='%s' meatTag='%s' color=%d amount=%d worldReturn=%d."),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		*GetNameSafe(MeatActor),
		*DepositMeatTag.ToString(),
		static_cast<int32>(DepositColor),
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

bool AARMeatStorageBoxActor::ConsumeTypedMeatFromState(FARMeatState& InOutMeatState, const FGameplayTag MeatTag, const int32 AmountToConsume) const
{
	if (!MeatTag.IsValid() || AmountToConsume <= 0)
	{
		return false;
	}

	for (FARMeatTypeAmount& Entry : InOutMeatState.AdditionalAmountsByType)
	{
		if (!Entry.MeatType.MatchesTagExact(MeatTag))
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

void AARMeatStorageBoxActor::AddTypedMeatToState(FARMeatState& InOutMeatState, const FGameplayTag MeatTag, const int32 AmountToAdd) const
{
	if (!MeatTag.IsValid() || AmountToAdd <= 0)
	{
		return;
	}

	for (FARMeatTypeAmount& Entry : InOutMeatState.AdditionalAmountsByType)
	{
		if (Entry.MeatType.MatchesTagExact(MeatTag))
		{
			Entry.Amount = FMath::Max(0, Entry.Amount) + AmountToAdd;
			InOutMeatState.NormalizeAdditionalAmounts();
			return;
		}
	}

	FARMeatTypeAmount& Added = InOutMeatState.AdditionalAmountsByType.AddDefaulted_GetRef();
	Added.MeatType = MeatTag;
	Added.Amount = AmountToAdd;
	InOutMeatState.NormalizeAdditionalAmounts();
}

void AARMeatStorageBoxActor::PromoteLegacyBucketsToTyped(FARMeatState& InOutMeatState) const
{
	UGameInstance* GI = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!ItemDefinitions)
	{
		return;
	}

	auto PromoteBucket = [this, &InOutMeatState, ItemDefinitions](const EARAffinityColor BucketColor, int32& BucketAmount)
	{
		const int32 SanitizedAmount = FMath::Max(0, BucketAmount);
		if (SanitizedAmount <= 0)
		{
			BucketAmount = 0;
			return;
		}

		FGameplayTag ResolvedMeatTag;
		if (!ItemDefinitions->ResolveFirstMeatTagForColor(BucketColor, ResolvedMeatTag))
		{
			return;
		}

		AddTypedMeatToState(InOutMeatState, ResolvedMeatTag, SanitizedAmount);
		BucketAmount = 0;
	};

	PromoteBucket(EARAffinityColor::Red, InOutMeatState.RedAmount);
	PromoteBucket(EARAffinityColor::Blue, InOutMeatState.BlueAmount);
	PromoteBucket(EARAffinityColor::White, InOutMeatState.WhiteAmount);
	PromoteBucket(EARAffinityColor::None, InOutMeatState.UnspecifiedAmount);
	InOutMeatState.NormalizeAdditionalAmounts();
}

void AARMeatStorageBoxActor::RebuildLegacyColorBucketsFromTyped(FARMeatState& InOutMeatState) const
{
	InOutMeatState.RedAmount = 0;
	InOutMeatState.BlueAmount = 0;
	InOutMeatState.WhiteAmount = 0;
	InOutMeatState.UnspecifiedAmount = 0;

	for (const FARMeatTypeAmount& Entry : InOutMeatState.AdditionalAmountsByType)
	{
		const int32 Amount = FMath::Max(0, Entry.Amount);
		if (!Entry.MeatType.IsValid() || Amount <= 0)
		{
			continue;
		}

		// Typed inventory no longer implies a canonical color. Keep legacy mirrors conservative.
		InOutMeatState.UnspecifiedAmount += Amount;
	}
}

bool AARMeatStorageBoxActor::SelectRandomEligibleMeatTagFromTypedStock(const FARMeatState& MeatState, FGameplayTag& OutMeatTag) const
{
	OutMeatTag = FGameplayTag();

	TArray<FGameplayTag> EligibleTags;
	const int32 DispenseAmount = FMath::Max(1, MeatAmountPerDispense);
	for (const FARMeatTypeAmount& Entry : MeatState.AdditionalAmountsByType)
	{
		if (!Entry.MeatType.IsValid())
		{
			continue;
		}

		if (Entry.Amount >= DispenseAmount)
		{
			EligibleTags.Add(Entry.MeatType);
		}
	}

	if (EligibleTags.IsEmpty())
	{
		return false;
	}

	const int32 RandomIndex = FMath::RandRange(0, EligibleTags.Num() - 1);
	OutMeatTag = EligibleTags[RandomIndex];
	return OutMeatTag.IsValid();
}
