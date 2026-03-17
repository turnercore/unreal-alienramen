#include "ARShopDispenserActor.h"

#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARSaveTypes.h"
#include "ARRamenMeatActor.h"
#include "ARShopCarryComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

AARShopDispenserActor::AARShopDispenserActor()
{
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnAnchor"));
	SpawnAnchor->SetupAttachment(SceneRoot);
}

bool AARShopDispenserActor::TryDispenseToController(AARPlayerController* RequestingController, FGameplayTag RequestedItemTag)
{
	if (!HasAuthority())
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Dispenser] TryDispenseToController rejected on '%s': no authority (controller='%s', item='%s')."),
			*GetNameSafe(this),
			*GetNameSafe(RequestingController),
			*RequestedItemTag.ToString());
		return false;
	}

	const FARShopDispenseDefinition* ResolvedDefinition = ResolveDispenseDefinition(RequestedItemTag);
	if (!ResolvedDefinition)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Dispenser] TryDispenseToController rejected on '%s': no definition for item '%s'."),
			*GetNameSafe(this),
			*RequestedItemTag.ToString());
		return false;
	}

	const FARShopDispenseDefinition Definition = *ResolvedDefinition;
	int32 DispenseAmount = FMath::Max(1, Definition.AmountPerDispense);

	FARMeatState PreConsumeMeatState;
	bool bUsedMeatReserve = false;
	if (!ConsumeSource(Definition, DispenseAmount, bUsedMeatReserve, PreConsumeMeatState))
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Dispenser] TryDispenseToController rejected on '%s': source consume failed (item='%s' amount=%d sourceType=%d sourceColor=%d)."),
			*GetNameSafe(this),
			*Definition.ItemTag.ToString(),
			DispenseAmount,
			static_cast<int32>(Definition.SourceType),
			static_cast<int32>(Definition.SourceColor));
		return false;
	}

	TSubclassOf<AActor> SpawnClass = Definition.SpawnActorClass;
	const FGameplayTag SharedItemRootTag = FGameplayTag::RequestGameplayTag(TEXT("Item"), false);
	const FGameplayTag MeatItemRootTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Meat"), false);
	const bool bCanResolveSharedItemClass =
		Definition.ItemTag.IsValid()
		&& Definition.bResolveSpawnActorClassFromItemDefinition
		&& SharedItemRootTag.IsValid()
		&& Definition.ItemTag.MatchesTag(SharedItemRootTag)
		&& (!MeatItemRootTag.IsValid() || !Definition.ItemTag.MatchesTag(MeatItemRootTag));
	if (!SpawnClass && bCanResolveSharedItemClass)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UARItemDefinitionSubsystem* ItemDefinitionSubsystem = GameInstance->GetSubsystem<UARItemDefinitionSubsystem>())
			{
				TSubclassOf<AActor> ResolvedItemClass;
				if (ItemDefinitionSubsystem->ResolveItemActorClass(Definition.ItemTag, ResolvedItemClass) && ResolvedItemClass)
				{
					SpawnClass = ResolvedItemClass;
				}
			}
		}
	}

	if (!SpawnClass && Definition.SourceType == EARShopDispenserSourceType::GameStateMeatReserve)
	{
		SpawnClass = AARRamenMeatActor::StaticClass();
	}

	if (!SpawnClass)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Shop|Dispenser] TryDispenseToController failed on '%s': no spawn class for item '%s'. Rolling back source."),
			*GetNameSafe(this),
			*Definition.ItemTag.ToString());
		RollbackSource(Definition, DispenseAmount, bUsedMeatReserve, PreConsumeMeatState);
		return false;
	}

	const FVector SpawnLocation = SpawnAnchor ? SpawnAnchor->GetComponentLocation() : GetActorLocation();
	const FRotator SpawnRotation = SpawnAnchor ? SpawnAnchor->GetComponentRotation() : GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(SpawnClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!SpawnedActor)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[Shop|Dispenser] TryDispenseToController failed on '%s': spawn failed for class '%s'. Rolling back source."),
			*GetNameSafe(this),
			*GetNameSafe(SpawnClass.Get()));
		RollbackSource(Definition, DispenseAmount, bUsedMeatReserve, PreConsumeMeatState);
		return false;
	}

	InitializeSpawnedActorFromDefinition(SpawnedActor, Definition, DispenseAmount);
	if (bCanResolveSharedItemClass)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UARItemDefinitionSubsystem* ItemDefinitionSubsystem = GameInstance->GetSubsystem<UARItemDefinitionSubsystem>())
			{
				ItemDefinitionSubsystem->ApplyItemPhysicsProperties(SpawnedActor, Definition.ItemTag);
			}
		}
	}
	BP_OnDispensedActorSpawned(SpawnedActor, Definition, DispenseAmount, RequestingController);

	if (Definition.bAutoPlaceIntoCarry)
	{
		if (UARShopCarryComponent* CarryComponent = ResolveCarryComponentFromController(RequestingController))
		{
			if (!CarryComponent->HasHeldActor())
			{
				CarryComponent->TrySetHeldActor(SpawnedActor);
			}
			else
			{
				UE_LOG(
					ARLog,
					Verbose,
					TEXT("[Shop|Dispenser] Auto-place skipped on '%s': controller '%s' already holding '%s'."),
					*GetNameSafe(this),
					*GetNameSafe(RequestingController),
					*GetNameSafe(CarryComponent->GetHeldActor()));
			}
		}
		else
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[Shop|Dispenser] Auto-place skipped on '%s': no carry component for controller '%s'."),
				*GetNameSafe(this),
				*GetNameSafe(RequestingController));
		}
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Shop|Dispenser] Dispense success on '%s': actor='%s' controller='%s' item='%s' amount=%d."),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedActor),
		*GetNameSafe(RequestingController),
		*Definition.ItemTag.ToString(),
		DispenseAmount);
	return true;
}

bool AARShopDispenserActor::HasDispenseDefinition(FGameplayTag RequestedItemTag) const
{
	return ResolveDispenseDefinition(RequestedItemTag) != nullptr;
}

bool AARShopDispenserActor::GetDispenseDefinition(FGameplayTag RequestedItemTag, FARShopDispenseDefinition& OutDefinition) const
{
	const FARShopDispenseDefinition* Definition = ResolveDispenseDefinition(RequestedItemTag);
	if (!Definition)
	{
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

bool AARShopDispenserActor::ConsumeCustomSource_Implementation(
	const FARShopDispenseDefinition& Definition,
	const int32 RequestedAmount,
	int32& OutGrantedAmount)
{
	(void)Definition;
	OutGrantedAmount = RequestedAmount;
	return false;
}

void AARShopDispenserActor::RollbackCustomSource_Implementation(const FARShopDispenseDefinition& Definition, const int32 GrantedAmount)
{
	(void)Definition;
	(void)GrantedAmount;
}

void AARShopDispenserActor::InitializeSpawnedActorFromDefinition(
	AActor* SpawnedActor,
	const FARShopDispenseDefinition& Definition,
	const int32 DispensedAmount)
{
	AARRamenMeatActor* MeatActor = Cast<AARRamenMeatActor>(SpawnedActor);
	if (!MeatActor)
	{
		return;
	}

	MeatActor->SetMeatData(Definition.SourceColor, DispensedAmount);
}

bool AARShopDispenserActor::ConsumeSource(
	const FARShopDispenseDefinition& Definition,
	int32& InOutDispenseAmount,
	bool& bOutUsedMeatReserve,
	FARMeatState& OutPreConsumeMeatState)
{
	bOutUsedMeatReserve = false;
	OutPreConsumeMeatState = FARMeatState();
	InOutDispenseAmount = FMath::Max(1, InOutDispenseAmount);

	switch (Definition.SourceType)
	{
	case EARShopDispenserSourceType::Unlimited:
		return true;

	case EARShopDispenserSourceType::GameStateMeatReserve:
		{
			AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
			if (!ARGameState)
			{
				return false;
			}

			FARMeatState NewMeatState = ARGameState->GetMeat();
			OutPreConsumeMeatState = NewMeatState;

			int32* Bucket = ResolveMeatBucket(NewMeatState, Definition.SourceColor);
			if (!Bucket || *Bucket < InOutDispenseAmount)
			{
				return false;
			}

			*Bucket -= InOutDispenseAmount;
			ARGameState->SetMeatFromSave(NewMeatState);
			bOutUsedMeatReserve = true;
			return true;
		}

	case EARShopDispenserSourceType::Custom:
		{
			int32 GrantedAmount = InOutDispenseAmount;
			if (!ConsumeCustomSource(Definition, InOutDispenseAmount, GrantedAmount))
			{
				return false;
			}

			if (GrantedAmount <= 0)
			{
				return false;
			}

			InOutDispenseAmount = GrantedAmount;
			return true;
		}

	default:
		return false;
	}
}

void AARShopDispenserActor::RollbackSource(
	const FARShopDispenseDefinition& Definition,
	const int32 DispenseAmount,
	const bool bUsedMeatReserve,
	const FARMeatState& PreConsumeMeatState)
{
	if (bUsedMeatReserve)
	{
		AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
		if (ARGameState)
		{
			ARGameState->SetMeatFromSave(PreConsumeMeatState);
		}
		return;
	}

	if (Definition.SourceType == EARShopDispenserSourceType::Custom)
	{
		RollbackCustomSource(Definition, DispenseAmount);
	}
}

const FARShopDispenseDefinition* AARShopDispenserActor::ResolveDispenseDefinition(FGameplayTag RequestedItemTag) const
{
	if (DispenseDefinitions.IsEmpty())
	{
		return nullptr;
	}

	if (!RequestedItemTag.IsValid())
	{
		return &DispenseDefinitions[0];
	}

	for (const FARShopDispenseDefinition& Definition : DispenseDefinitions)
	{
		if (Definition.ItemTag.IsValid() && Definition.ItemTag.MatchesTagExact(RequestedItemTag))
		{
			return &Definition;
		}
	}

	return nullptr;
}

UARShopCarryComponent* AARShopDispenserActor::ResolveCarryComponentFromController(AARPlayerController* Controller)
{
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
}

int32* AARShopDispenserActor::ResolveMeatBucket(FARMeatState& MeatState, const EARAffinityColor SourceColor)
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
