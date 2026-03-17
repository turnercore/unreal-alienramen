#include "ARShopGameMode.h"

#include "AREconomySettings.h"
#include "AREnergyDrinkCarryItem.h"
#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRamenMeatActor.h"
#include "ARRamenBowlActor.h"
#include "ARRunBuffSubsystem.h"
#include "ARScrapyardTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARShopCarryComponent.h"
#include "ARShopCarryItemBase.h"
#include "Kismet/GameplayStatics.h"
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

	static bool TryParseSaveLoadEntryTransitionOptions(const FString& OptionsString, bool& bOutHasTransitionOptions)
	{
		bOutHasTransitionOptions = OptionsString.Contains(ARTransition::OptionSourceMode)
			|| OptionsString.Contains(ARTransition::OptionReason)
			|| OptionsString.Contains(ARTransition::OptionFreshLoad);
		if (!bOutHasTransitionOptions)
		{
			return false;
		}

		FARTransitionContext TransitionContext;
		ARTransition::ApplyTransitionContextFromTravelOptions(OptionsString, TransitionContext);
		return TransitionContext.SourceMode == EARTransitionSourceMode::SaveLoad
			&& TransitionContext.Reason == EARTransitionReason::SaveLoadEntry
			&& TransitionContext.bFreshLoadEntry;
	}

	static FGameplayTag ResolveCharacterTagForController(const AController* Controller)
	{
		const AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		if (!PlayerState)
		{
			return FGameplayTag();
		}

		const FGameplayTag CanonicalCharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
		const FGameplayTag ChoiceCharacterTag = ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());
		if (ChoiceCharacterTag.IsValid())
		{
			if (!CanonicalCharacterTag.IsValid() || !CanonicalCharacterTag.MatchesTagExact(ChoiceCharacterTag))
			{
				return ChoiceCharacterTag;
			}
		}

		if (CanonicalCharacterTag.IsValid())
		{
			return CanonicalCharacterTag;
		}

		return ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());
	}
}

AARShopGameMode::AARShopGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[ShopGameMode] Required gameplay tag 'Mode.Shop' is missing."));
	bRouteModeTravelThroughTransitionMap = true;
	TransitionTravelMapURL = TEXT("/Game/Maps/Lvl_Loading");
	TransitionSourceMode = EARTransitionSourceMode::Shop;
	TransitionReason = EARTransitionReason::ShopToInvader;
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
	TryRestoreFreshLoadCharacterStates(SaveGame, SaveSubsystem);
	PersistCanonicalShopEntryIfNeeded(SaveSubsystem, SaveGame);
}

void AARShopGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

	UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr;
	UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	TryRestoreFreshLoadCharacterStates(SaveGame, SaveSubsystem);
}

UClass* AARShopGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const FGameplayTag PendingCharacterTag = ARPlayer::NormalizeCharacterTag(GetPendingSpawnCharacterTagForController(InController));
	const FGameplayTag RuntimeCharacterTag = ARPlayer::NormalizeCharacterTag(ResolveCharacterTagForController(InController));
	const FGameplayTag CharacterTag = PendingCharacterTag.IsValid() ? PendingCharacterTag : RuntimeCharacterTag;
	const TCHAR* CharacterTagSource = PendingCharacterTag.IsValid() ? TEXT("PendingChoosePlayerStart") : TEXT("PlayerState");
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[ShopGameMode] Resolve pawn class for controller='%s' playerSlotId=%d choice=%d characterTag=%s source=%s."),
		*GetNameSafe(InController),
		InController && InController->GetPlayerState<AARPlayerStateBase>() ? InController->GetPlayerState<AARPlayerStateBase>()->GetPlayerSlotId() : 0,
		InController && InController->GetPlayerState<AARPlayerStateBase>() ? static_cast<int32>(InController->GetPlayerState<AARPlayerStateBase>()->GetCharacterPicked()) : static_cast<int32>(EARCharacterChoice::None),
		*CharacterTag.ToString(),
		CharacterTagSource);
	if (CharacterTag.IsValid())
	{
		if (const TSubclassOf<APawn>* PawnClassByTag = ShopPawnClassByCharacterTag.Find(CharacterTag))
		{
			if (*PawnClassByTag)
			{
				UE_LOG(ARLog, Verbose, TEXT("[ShopGameMode] Pawn class exact-tag hit: QueryTag=%s Class=%s."), *CharacterTag.ToString(), *GetNameSafe(PawnClassByTag->Get()));
				return PawnClassByTag->Get();
			}
		}

		// Legacy compatibility: support Parley/Customer keyed entries by canonicalizing map keys at runtime.
		for (const TPair<FGameplayTag, TSubclassOf<APawn>>& Entry : ShopPawnClassByCharacterTag)
		{
			if (!Entry.Value)
			{
				continue;
			}

			const FGameplayTag CanonicalEntryTag = ARPlayer::NormalizeCharacterTag(Entry.Key);
			if (!CanonicalEntryTag.IsValid() || !CanonicalEntryTag.MatchesTagExact(CharacterTag))
			{
				continue;
			}

			if (!Entry.Key.MatchesTagExact(CanonicalEntryTag))
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[ShopGameMode] Pawn class map uses legacy character tag '%s'; please migrate this key to canonical '%s'."),
					*Entry.Key.ToString(),
					*CanonicalEntryTag.ToString());
			}

			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[ShopGameMode] Pawn class canonicalized exact hit: QueryTag=%s EntryTag=%s Class=%s."),
				*CharacterTag.ToString(),
				*Entry.Key.ToString(),
				*GetNameSafe(Entry.Value.Get()));
			return Entry.Value.Get();
		}

		// Allow broader parent-tag mappings while still canonicalizing legacy key roots.
		for (const TPair<FGameplayTag, TSubclassOf<APawn>>& Entry : ShopPawnClassByCharacterTag)
		{
			if (!Entry.Value)
			{
				continue;
			}

			const FGameplayTag CanonicalEntryTag = ARPlayer::NormalizeCharacterTag(Entry.Key);
			if (CanonicalEntryTag.IsValid() && CharacterTag.MatchesTag(CanonicalEntryTag))
			{
				if (!Entry.Key.MatchesTagExact(CanonicalEntryTag))
				{
					UE_LOG(
						ARLog,
						Warning,
						TEXT("[ShopGameMode] Pawn class map uses legacy parent tag '%s'; migrate to canonical '%s'."),
						*Entry.Key.ToString(),
						*CanonicalEntryTag.ToString());
				}

				UE_LOG(
					ARLog,
					Verbose,
					TEXT("[ShopGameMode] Pawn class canonicalized parent-tag hit: QueryTag=%s EntryTag=%s Class=%s."),
					*CharacterTag.ToString(),
					*Entry.Key.ToString(),
					*GetNameSafe(Entry.Value.Get()));
				return Entry.Value.Get();
			}
		}
	}

	if (FallbackShopPawnClass)
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] Pawn class falling back to FallbackShopPawnClass '%s' for controller '%s' (CharacterTag=%s)."),
			*GetNameSafe(FallbackShopPawnClass.Get()), *GetNameSafe(InController), *CharacterTag.ToString());
		return FallbackShopPawnClass.Get();
	}

	UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] Pawn class falling back to Super for controller '%s' (CharacterTag=%s)."), *GetNameSafe(InController), *CharacterTag.ToString());
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

bool AARShopGameMode::PreStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks)
{
	if (!Super::PreStartTravel(URL, Options, bSkipReadyChecks))
	{
		return false;
	}

	UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr;
	ClearShopTransientCarryablesForRunStart(SaveSubsystem);
	UE_LOG(ARLog, Verbose, TEXT("[ShopGameMode] Faction election finalization is handled in transition map flow."));
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

bool AARShopGameMode::ShouldApplyFreshLoadCharacterRestore(const UARSaveSubsystem* SaveSubsystem, const UARSaveGame* SaveGame) const
{
	if (!HasAuthority() || !SaveSubsystem || !SaveGame || !SaveSubsystem->HasPendingFreshLoadEntry())
	{
		return false;
	}

	const FGameplayTag ShopModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Shop"), false);
	if (!ShopModeTag.IsValid() || !SaveGame->LastSavedModeTag.MatchesTagExact(ShopModeTag))
	{
		return false;
	}

	bool bHasTransitionOptions = false;
	if (!TryParseSaveLoadEntryTransitionOptions(OptionsString, bHasTransitionOptions) && bHasTransitionOptions)
	{
		return false;
	}

	return true;
}

bool AARShopGameMode::TryRestoreFreshLoadCharacterStates(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem) const
{
	if (!ShouldApplyFreshLoadCharacterRestore(SaveSubsystem, SaveGame))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AController*> ControllersToRestore;
	bool bHasDeferredRestore = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
		if (!PlayerState)
		{
			continue;
		}

		if (!Pawn || !CarryComponent)
		{
			bHasDeferredRestore = true;
			continue;
		}

		ControllersToRestore.Add(Controller);
	}

	if (ControllersToRestore.IsEmpty())
	{
		return false;
	}

	bool bRestoredAny = false;
	for (AController* Controller : ControllersToRestore)
	{
		bRestoredAny = TryRestoreCharacterShopStateForController(Controller, SaveGame) || bRestoredAny;
	}

	if (!bHasDeferredRestore)
	{
		SaveSubsystem->ClearPendingFreshLoadEntry();
	}

	return bRestoredAny;
}

bool AARShopGameMode::TryRestoreCharacterShopStateForController(AController* Controller, UARSaveGame* SaveGame) const
{
	if (!Controller || !SaveGame)
	{
		return false;
	}

	AARPlayerStateBase* PlayerState = Controller->GetPlayerState<AARPlayerStateBase>();
	APawn* Pawn = Controller->GetPawn();
	UARShopCarryComponent* CarryComponent = Pawn ? Pawn->FindComponentByClass<UARShopCarryComponent>() : nullptr;
	if (!PlayerState || !Pawn || !CarryComponent)
	{
		return false;
	}

	const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
	int32 CharacterIndex = INDEX_NONE;
	FARCharacterSaveData* CharacterState = SaveGame->FindCharacterStateDataMutable(CharacterTag, CharacterIndex);
	if (!CharacterState)
	{
		return false;
	}

	const FARCharacterShopSnapshot& Snapshot = CharacterState->ShopSnapshot;
	bool bAppliedAny = false;

	if (Snapshot.bHasCharacterTransform)
	{
		Pawn->SetActorTransform(Snapshot.CharacterTransform, false, nullptr, ETeleportType::TeleportPhysics);
		bAppliedAny = true;
	}

	if (AActor* ExistingHeldActor = CarryComponent->ClearHeldActor(false))
	{
		ExistingHeldActor->Destroy();
	}

	if (Snapshot.bHasHeldItem)
	{
		bAppliedAny = RestoreHeldShopItemSnapshot(CarryComponent, Snapshot.HeldItem) || bAppliedAny;
	}

	if (bAppliedAny)
	{
		CharacterState->ShopSnapshot = FARCharacterShopSnapshot();
	}

	return bAppliedAny;
}

bool AARShopGameMode::RestoreHeldShopItemSnapshot(UARShopCarryComponent* CarryComponent, const FARCharacterHeldShopItemSnapshot& Snapshot) const
{
	if (!CarryComponent)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	AActor* OwnerActor = CarryComponent->GetOwner();
	if (!World || !OwnerActor)
	{
		return false;
	}

	UClass* SpawnClass = Snapshot.ActorClass.LoadSynchronous();
	if (!SpawnClass || !SpawnClass->IsChildOf(AARShopCarryItemBase::StaticClass()))
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] Skipping invalid held item restore class '%s'."),
			*Snapshot.ActorClass.ToSoftObjectPath().ToString());
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AARShopCarryItemBase* SpawnedItem = World->SpawnActor<AARShopCarryItemBase>(SpawnClass, OwnerActor->GetActorTransform(), SpawnParams);
	if (!SpawnedItem)
	{
		return false;
	}

	bool bConfigured = true;
	if (AAREnergyDrinkCarryItem* EnergyDrink = Cast<AAREnergyDrinkCarryItem>(SpawnedItem))
	{
		if (!Snapshot.EnergyDrinkItemTag.IsValid())
		{
			bConfigured = false;
		}
		else
		{
			EnergyDrink->SetEnergyDrinkItemTag(Snapshot.EnergyDrinkItemTag);
			if (ItemDefinitions)
			{
				ItemDefinitions->ApplyItemPhysicsProperties(EnergyDrink, Snapshot.EnergyDrinkItemTag);
			}
		}
	}
	else if (AARRamenMeatActor* MeatActor = Cast<AARRamenMeatActor>(SpawnedItem))
	{
		MeatActor->SetMeatData(Snapshot.MeatColor, FMath::Max(1, Snapshot.MeatAmount));
	}
	else if (AARRamenBowlActor* BowlActor = Cast<AARRamenBowlActor>(SpawnedItem))
	{
		bConfigured = RestoreBowlSnapshot(BowlActor, Snapshot);
	}

	if (!bConfigured || !CarryComponent->TrySetHeldActor(SpawnedItem))
	{
		SpawnedItem->Destroy();
		return false;
	}

	return true;
}

bool AARShopGameMode::RestoreBowlSnapshot(AARRamenBowlActor* BowlActor, const FARCharacterHeldShopItemSnapshot& Snapshot) const
{
	if (!BowlActor)
	{
		return false;
	}

	BowlActor->ClearBowl();
	const int32 FillStep = FMath::Clamp(Snapshot.BowlFillStep, 0, 3);
	if (FillStep >= 1 && !BowlActor->TryApplyFillFromStation(EARRamenStationType::Noodles, Snapshot.BowlSpec.NoodlesColor))
	{
		return false;
	}

	if (FillStep >= 2 && !BowlActor->TryApplyFillFromStation(EARRamenStationType::Broth, Snapshot.BowlSpec.BrothColor))
	{
		return false;
	}

	if (FillStep >= 3 && !BowlActor->TryApplyFillFromStation(EARRamenStationType::Toppings, Snapshot.BowlSpec.ToppingsColor))
	{
		return false;
	}

	return true;
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

	TMap<FGameplayTag, int32> ExistingWorldDrinkCounts;
	for (TActorIterator<AAREnergyDrinkCarryItem> It(World); It; ++It)
	{
		const AAREnergyDrinkCarryItem* ExistingDrink = *It;
		if (!ExistingDrink)
		{
			continue;
		}

		const FGameplayTag ExistingTag = ExistingDrink->GetEnergyDrinkItemTag();
		if (!ExistingTag.IsValid())
		{
			continue;
		}

		ExistingWorldDrinkCounts.FindOrAdd(ExistingTag) += 1;
	}

	bool bSpawnedAny = false;
	bool bMutatedInventory = false;
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

		if (int32* ExistingCountPtr = ExistingWorldDrinkCounts.Find(Stack.ItemTag))
		{
			const int32 CountAlreadyMaterialized = FMath::Max(0, *ExistingCountPtr);
			if (CountAlreadyMaterialized > 0)
			{
				const int32 ConsumedCount = FMath::Min(Stack.Count, CountAlreadyMaterialized);
				Stack.Count -= ConsumedCount;
				*ExistingCountPtr -= ConsumedCount;
				bMutatedInventory = bMutatedInventory || ConsumedCount > 0;
			}
		}

		if (Stack.Count <= 0)
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
			bMutatedInventory = true;
			++SpawnSequence;
			bSpawnedAny = true;
		}
	}

	SaveGame->StoredEnergyDrinkStacks.RemoveAll([](const FARRunBuffItemStack& Stack)
	{
		return !Stack.ItemTag.IsValid() || Stack.Count <= 0;
	});

	if (!bSpawnedAny && !bMutatedInventory)
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

bool AARShopGameMode::ShouldPersistCanonicalShopEntry(const UARSaveGame* SaveGame) const
{
	if (!HasAuthority() || !SaveGame)
	{
		return false;
	}

	const UPackage* WorldPackage = GetWorld() && GetWorld()->PersistentLevel ? GetWorld()->PersistentLevel->GetOutermost() : nullptr;
	const FString CurrentMapPath = WorldPackage ? WorldPackage->GetName() : FString();
	return SaveGame->LastSavedModeTag != ModeTag || (!CurrentMapPath.IsEmpty() && SaveGame->LastSavedMapPath != CurrentMapPath);
}

void AARShopGameMode::PersistCanonicalShopEntryIfNeeded(UARSaveSubsystem* SaveSubsystem, const UARSaveGame* SaveGame) const
{
	if (!SaveSubsystem || !ShouldPersistCanonicalShopEntry(SaveGame))
	{
		return;
	}

	FARSaveResult SaveResult;
	if (!SaveSubsystem->SaveCurrentGameUnthrottled(NAME_None, true, SaveResult))
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] Failed to persist canonical shop entry save: %s"), *SaveResult.Error);
	}
}
