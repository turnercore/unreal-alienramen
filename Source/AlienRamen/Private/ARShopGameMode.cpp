#include "ARShopGameMode.h"

#include "AREconomySettings.h"
#include "AREnergyDrinkCarryItem.h"
#include "ARGameStateBase.h"
#include "ARFactionSubsystem.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARRamenMeatActor.h"
#include "ARRunBuffSubsystem.h"
#include "ARScrapyardTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARShopCarryItemBase.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	static FARMeatState MergeMeatStates(const FARMeatState& A, const FARMeatState& B)
	{
		FARMeatState Out = A;
		Out.RedAmount += FMath::Max(0, B.RedAmount);
		Out.BlueAmount += FMath::Max(0, B.BlueAmount);
		Out.WhiteAmount += FMath::Max(0, B.WhiteAmount);
		Out.UnspecifiedAmount += FMath::Max(0, B.UnspecifiedAmount);

		TMap<FGameplayTag, int32> AdditionalByType;
		for (const FARMeatTypeAmount& Entry : Out.AdditionalAmountsByType)
		{
			if (Entry.MeatType.IsValid() && Entry.Amount > 0)
			{
				AdditionalByType.FindOrAdd(Entry.MeatType) += Entry.Amount;
			}
		}
		for (const FARMeatTypeAmount& Entry : B.AdditionalAmountsByType)
		{
			if (Entry.MeatType.IsValid() && Entry.Amount > 0)
			{
				AdditionalByType.FindOrAdd(Entry.MeatType) += Entry.Amount;
			}
		}

		Out.AdditionalAmountsByType.Reset();
		for (const TPair<FGameplayTag, int32>& Pair : AdditionalByType)
		{
			FARMeatTypeAmount& Added = Out.AdditionalAmountsByType.AddDefaulted_GetRef();
			Added.MeatType = Pair.Key;
			Added.Amount = Pair.Value;
		}
		Out.NormalizeAdditionalAmounts();
		return Out;
	}

	static void ClampMeatStateTotal(FARMeatState& InOutMeat, const int32 MaxTotal)
	{
		if (MaxTotal < 0)
		{
			return;
		}

		int32 Excess = FMath::Max(0, InOutMeat.GetTotalAmount() - MaxTotal);
		if (Excess <= 0)
		{
			return;
		}

		auto TrimBucket = [&Excess](int32& Bucket)
		{
			if (Excess <= 0 || Bucket <= 0)
			{
				return;
			}

			const int32 Removed = FMath::Min(Excess, Bucket);
			Bucket -= Removed;
			Excess -= Removed;
		};

		TrimBucket(InOutMeat.UnspecifiedAmount);
		TrimBucket(InOutMeat.RedAmount);
		TrimBucket(InOutMeat.BlueAmount);
		TrimBucket(InOutMeat.WhiteAmount);
		for (FARMeatTypeAmount& Entry : InOutMeat.AdditionalAmountsByType)
		{
			TrimBucket(Entry.Amount);
			if (Excess <= 0)
			{
				break;
			}
		}

		InOutMeat.NormalizeAdditionalAmounts();
	}
}

AARShopGameMode::AARShopGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[ShopGameMode] Required gameplay tag 'Mode.Shop' is missing."));
}

void AARShopGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AARGameStateBase* SharedGameState = GetGameState<AARGameStateBase>();
	if (!SharedGameState)
	{
		return;
	}

	const UAREconomySettings* EconomySettings = GetDefault<UAREconomySettings>();
	const int32 MaxScrapStorage = EconomySettings ? FMath::Max(0, EconomySettings->MaxScrapStorage) : 0;
	const int32 MaxMeatStorage = EconomySettings ? FMath::Max(0, EconomySettings->MaxMeatStorage) : 0;

	const int32 DepositedScrap = SharedGameState->GetScrap() + SharedGameState->GetRunLedgerScrap();
	SharedGameState->SetScrapFromSave(FMath::Clamp(DepositedScrap, 0, MaxScrapStorage));

	FARMeatState DepositedMeat = MergeMeatStates(SharedGameState->GetMeat(), SharedGameState->GetRunLedgerMeat());
	ClampMeatStateTotal(DepositedMeat, MaxMeatStorage);
	SharedGameState->SetMeatFromSave(DepositedMeat);
	SharedGameState->ClearRunLedger();

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
	{
		RunBuffSubsystem->ClearRunBuffsForShopEntry();
	}

	UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>();
	UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	if (!SaveSubsystem || !SaveGame)
	{
		return;
	}

	if (SaveGame->bClearShopTransientCarryablesOnNextShopLoad)
	{
		SaveGame->ShopTransientCarryables.Reset();
		SaveGame->bClearShopTransientCarryablesOnNextShopLoad = false;
		SaveSubsystem->MarkSaveDirty();
	}

	const int32 TransientCountBeforeRestore = SaveGame->ShopTransientCarryables.Num();
	RestoreTransientShopCarryables(SaveGame);
	if (SaveGame->ShopTransientCarryables.Num() != TransientCountBeforeRestore)
	{
		SaveSubsystem->MarkSaveDirty();
	}
	// Always evaluate anchor spawning so shared stored inventory can still materialize
	// even when non-drink transient carryables were restored.
	SpawnStoredEnergyDrinksAtAnchors(SaveGame, SaveSubsystem);
}

bool AARShopGameMode::PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks)
{
	if (!Super::PreStartTravel(URL, Options, bSkipReadyChecks))
	{
		return false;
	}

	UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr;
	ClearShopTransientCarryablesForRunStart(SaveSubsystem);

	UARFactionSubsystem* FactionSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARFactionSubsystem>() : nullptr;
	if (!FactionSubsystem)
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] PreStartTravel failed: FactionSubsystem missing."));
		return false;
	}

	FGameplayTag WinnerFactionTag;
	EARFactionWinnerReason Reason = EARFactionWinnerReason::NoValidFactions;
	const bool bFinalized = FactionSubsystem->FinalizeElectionForTravel(WinnerFactionTag, Reason);
	if (!bFinalized)
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] PreStartTravel blocked: faction election finalize failed."));
		return false;
	}

	UE_LOG(
		ARLog,
		Log,
		TEXT("[ShopGameMode] Faction election finalized. Winner='%s' Reason=%d"),
		*WinnerFactionTag.ToString(),
		static_cast<int32>(Reason));
	return true;
}

bool AARShopGameMode::RestoreTransientShopCarryables(UARSaveGame* SaveGame) const
{
	if (!HasAuthority() || !SaveGame || SaveGame->ShopTransientCarryables.IsEmpty())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!World)
	{
		return false;
	}

	bool bRestoredAny = false;
	TArray<FARShopTransientCarryableSnapshot> SanitizedSnapshots;
	SanitizedSnapshots.Reserve(SaveGame->ShopTransientCarryables.Num());

	for (const FARShopTransientCarryableSnapshot& Snapshot : SaveGame->ShopTransientCarryables)
	{
		UClass* SpawnClass = Snapshot.ActorClass.LoadSynchronous();
		if (!SpawnClass || !SpawnClass->IsChildOf(AARShopCarryItemBase::StaticClass()))
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ShopGameMode] Skipping invalid transient carryable class '%s'."),
				*Snapshot.ActorClass.ToSoftObjectPath().ToString());
			continue;
		}

		const bool bEnergyDrinkClass = SpawnClass->IsChildOf(AAREnergyDrinkCarryItem::StaticClass());
		if (bEnergyDrinkClass && !Snapshot.EnergyDrinkItemTag.IsValid())
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ShopGameMode] Skipping transient energy drink '%s': missing item tag."),
				*Snapshot.ActorClass.ToSoftObjectPath().ToString());
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AARShopCarryItemBase* Spawned = World->SpawnActor<AARShopCarryItemBase>(SpawnClass, Snapshot.WorldTransform, SpawnParams);
		if (!Spawned)
		{
			continue;
		}

		if (AAREnergyDrinkCarryItem* EnergyDrink = Cast<AAREnergyDrinkCarryItem>(Spawned))
		{
			EnergyDrink->SetEnergyDrinkItemTag(Snapshot.EnergyDrinkItemTag);
			if (ItemDefinitions)
			{
				ItemDefinitions->ApplyItemPhysicsProperties(EnergyDrink, Snapshot.EnergyDrinkItemTag);
			}
		}
		else if (AARRamenMeatActor* MeatActor = Cast<AARRamenMeatActor>(Spawned))
		{
			MeatActor->SetMeatData(Snapshot.MeatColor, FMath::Max(1, Snapshot.MeatAmount));
		}

		SanitizedSnapshots.Add(Snapshot);
		bRestoredAny = true;
	}

	if (SanitizedSnapshots.Num() != SaveGame->ShopTransientCarryables.Num())
	{
		SaveGame->ShopTransientCarryables = MoveTemp(SanitizedSnapshots);
	}

	return bRestoredAny;
}

bool AARShopGameMode::SpawnStoredEnergyDrinksAtAnchors(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem) const
{
	if (!HasAuthority() || !SaveGame || !SaveSubsystem)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!World || !GameInstance || !ItemDefinitions)
	{
		return false;
	}

	TArray<AActor*> AnchorActors;
	if (!EnergyDrinkSpawnAnchorActorTag.IsNone())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->ActorHasTag(EnergyDrinkSpawnAnchorActorTag))
			{
				AnchorActors.Add(Actor);
			}
		}
	}

	if (AnchorActors.IsEmpty())
	{
		return false;
	}

	AnchorActors.Sort([](const AActor& A, const AActor& B)
	{
		return A.GetName() < B.GetName();
	});

	bool bSpawnedAny = false;
	int32 SpawnSequence = 0;
	for (FARRunBuffItemStack& Stack : SaveGame->StoredEnergyDrinkStacks)
	{
		if (!Stack.ItemTag.IsValid() || Stack.Count <= 0 || Stack.CharacterTag.IsValid())
		{
			continue;
		}

		FARScrapyardItemDefRow ItemDef;
		if (!ItemDefinitions->ResolveItemDefinition(Stack.ItemTag, ItemDef))
		{
			continue;
		}
		if (ItemDef.ItemType != EARScrapyardItemType::EnergyDrink)
		{
			continue;
		}

		UClass* SpawnClass = FallbackEnergyDrinkCarryItemClass ? FallbackEnergyDrinkCarryItemClass.Get() : AAREnergyDrinkCarryItem::StaticClass();
		TSubclassOf<AActor> ResolvedItemClass;
		if (ItemDefinitions->ResolveItemActorClass(Stack.ItemTag, ResolvedItemClass) && ResolvedItemClass)
		{
			SpawnClass = ResolvedItemClass.Get();
		}

		if (!SpawnClass || !SpawnClass->IsChildOf(AAREnergyDrinkCarryItem::StaticClass()))
		{
			continue;
		}

		const int32 SpawnCount = FMath::Max(0, Stack.Count);
		for (int32 InstanceIndex = 0; InstanceIndex < SpawnCount; ++InstanceIndex)
		{
			if (AnchorActors.IsEmpty())
			{
				break;
			}

			const int32 AnchorIndex = SpawnSequence % AnchorActors.Num();
			const int32 StackLayer = SpawnSequence / AnchorActors.Num();
			const AActor* AnchorActor = AnchorActors[AnchorIndex];
			if (!AnchorActor)
			{
				++SpawnSequence;
				continue;
			}

			FTransform SpawnTransform = AnchorActor->GetActorTransform();
			SpawnTransform.AddToTranslation(FVector(0.0f, 0.0f, EnergyDrinkStackedSpawnZOffset * StackLayer));

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AAREnergyDrinkCarryItem* SpawnedDrink = World->SpawnActor<AAREnergyDrinkCarryItem>(SpawnClass, SpawnTransform, SpawnParams);
			if (!SpawnedDrink)
			{
				++SpawnSequence;
				continue;
			}

			SpawnedDrink->SetEnergyDrinkItemTag(Stack.ItemTag);
			ItemDefinitions->ApplyItemPhysicsProperties(SpawnedDrink, Stack.ItemTag);
			FARShopTransientCarryableSnapshot& Snapshot = SaveGame->ShopTransientCarryables.AddDefaulted_GetRef();
			Snapshot.ActorClass = SpawnClass;
			Snapshot.WorldTransform = SpawnTransform;
			Snapshot.EnergyDrinkItemTag = Stack.ItemTag;

			--Stack.Count;
			++SpawnSequence;
			bSpawnedAny = true;
		}
	}

	SaveGame->StoredEnergyDrinkStacks.RemoveAll([](const FARRunBuffItemStack& Stack)
	{
		return !Stack.ItemTag.IsValid() || Stack.Count <= 0;
	});

	if (!bSpawnedAny)
	{
		return false;
	}

	SaveSubsystem->MarkSaveDirty();
	return true;
}

void AARShopGameMode::ClearShopTransientCarryablesForRunStart(UARSaveSubsystem* SaveSubsystem) const
{
	if (!HasAuthority() || !SaveSubsystem)
	{
		return;
	}

	UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame();
	if (!SaveGame)
	{
		return;
	}

	if (SaveGame->ShopTransientCarryables.IsEmpty() && !SaveGame->bClearShopTransientCarryablesOnNextShopLoad)
	{
		return;
	}

	SaveGame->ShopTransientCarryables.Reset();
	SaveGame->bClearShopTransientCarryablesOnNextShopLoad = false;
	SaveSubsystem->MarkSaveDirty();
}
