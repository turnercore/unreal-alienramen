#include "ARShopGameMode.h"

#include "AREconomySettings.h"
#include "AREnergyDrinkCarryItem.h"
#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "ARPlayerStateBase.h"
#include "ARRamenMeatActor.h"
#include "ARRamenBowlActor.h"
#include "ARRunBuffSubsystem.h"
#include "ARScrapyardTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARCarryItemBase.h"
#include "ARShopCarryComponent.h"
#include "ARShopGameState.h"
#include "ARTaggedPlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	static FARMeatState MergeMeatStates(const FARMeatState& A, const FARMeatState& B)
	{
		FARMeatState Out = A;

		TMap<FString, FARMeatTypeAmount> AdditionalByTuple;
		for (const FARMeatTypeAmount& Entry : Out.AdditionalAmountsByType)
		{
			if (Entry.MeatType.IsValid() && Entry.Amount > 0)
			{
				const FString Key = FString::Printf(
					TEXT("%s|%d|%d"),
					*Entry.MeatType.ToString(),
					static_cast<int32>(Entry.MeatColor),
					static_cast<int32>(Entry.MeatQualityTier));
				FARMeatTypeAmount& Aggregated = AdditionalByTuple.FindOrAdd(Key);
				if (!Aggregated.MeatType.IsValid())
				{
					Aggregated.MeatType = Entry.MeatType;
					Aggregated.MeatColor = Entry.MeatColor;
					Aggregated.MeatQualityTier = Entry.MeatQualityTier;
				}
				Aggregated.Amount += Entry.Amount;
			}
		}
		for (const FARMeatTypeAmount& Entry : B.AdditionalAmountsByType)
		{
			if (Entry.MeatType.IsValid() && Entry.Amount > 0)
			{
				const FString Key = FString::Printf(
					TEXT("%s|%d|%d"),
					*Entry.MeatType.ToString(),
					static_cast<int32>(Entry.MeatColor),
					static_cast<int32>(Entry.MeatQualityTier));
				FARMeatTypeAmount& Aggregated = AdditionalByTuple.FindOrAdd(Key);
				if (!Aggregated.MeatType.IsValid())
				{
					Aggregated.MeatType = Entry.MeatType;
					Aggregated.MeatColor = Entry.MeatColor;
					Aggregated.MeatQualityTier = Entry.MeatQualityTier;
				}
				Aggregated.Amount += Entry.Amount;
			}
		}

		Out.AdditionalAmountsByType.Reset();
		for (const TPair<FString, FARMeatTypeAmount>& Pair : AdditionalByTuple)
		{
			FARMeatTypeAmount& Added = Out.AdditionalAmountsByType.AddDefaulted_GetRef();
			Added = Pair.Value;
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

bool AARShopGameMode::GetTipRangeForReaction(const EARRamenTasteReaction Reaction, FARShopReactionTipRange& OutRange) const
{
	switch (Reaction)
	{
	case EARRamenTasteReaction::Hate:
		OutRange = HateTipMultiplierRange;
		return true;
	case EARRamenTasteReaction::Ok:
		OutRange = OkTipMultiplierRange;
		return true;
	case EARRamenTasteReaction::Like:
		OutRange = LikeTipMultiplierRange;
		return true;
	case EARRamenTasteReaction::Love:
		OutRange = LoveTipMultiplierRange;
		return true;
	default:
		OutRange = FARShopReactionTipRange();
		return false;
	}
}

int32 AARShopGameMode::CalculateServePayout(
	const FARRamenBowlSpec& ServedBowl,
	const EARRamenTasteReaction Reaction,
	float& OutAppliedTipMultiplier,
	int32& OutCombinedMeatValue,
	int32& OutBasePayout,
	int32& OutTipPayout)
{
	OutAppliedTipMultiplier = 0.0f;
	OutCombinedMeatValue = ResolveCombinedMeatValue(ServedBowl);
	OutTipPayout = 0;

	AARShopGameState* ShopGameState = GetGameState<AARShopGameState>();
	OutBasePayout = ShopGameState ? ShopGameState->GetBaseBowlPayout() : FMath::Max(0, BaseBowlPayout);

	FARShopReactionTipRange TipRange;
	if (!GetTipRangeForReaction(Reaction, TipRange))
	{
		return FMath::Max(0, OutBasePayout);
	}

	const float RangeMin = FMath::Max(0.0f, FMath::Min(TipRange.MinMultiplier, TipRange.MaxMultiplier));
	const float RangeMax = FMath::Max(0.0f, FMath::Max(TipRange.MinMultiplier, TipRange.MaxMultiplier));
	const uint32 Seed = HashCombineFast(
		HashCombineFast(
			HashCombineFast(GetTypeHash(++ServeTipRollCounter), GetTypeHash(Reaction)),
			HashCombineFast(GetTypeHash(ServedBowl.Noodles.MeatTag), GetTypeHash(ServedBowl.Broth.MeatTag))),
		GetTypeHash(ServedBowl.Toppings.MeatTag));
	FRandomStream TipRandom(static_cast<int32>(Seed));
	OutAppliedTipMultiplier = TipRandom.FRandRange(RangeMin, RangeMax);
	OutTipPayout = FMath::Max(0, FMath::RoundToInt(OutCombinedMeatValue * OutAppliedTipMultiplier));
	return FMath::Max(0, OutBasePayout + OutTipPayout);
}

float AARShopGameMode::GetVendingQualityMultiplier(const EARVendingQualityTier QualityTier) const
{
	switch (QualityTier)
	{
	case EARVendingQualityTier::Low:
		return FMath::Max(0.0f, VendingLowQualityMultiplier);
	case EARVendingQualityTier::Standard:
		return FMath::Max(0.0f, VendingStandardQualityMultiplier);
	case EARVendingQualityTier::High:
		return FMath::Max(0.0f, VendingHighQualityMultiplier);
	case EARVendingQualityTier::Premium:
		return FMath::Max(0.0f, VendingPremiumQualityMultiplier);
	default:
		return FMath::Max(0.0f, VendingStandardQualityMultiplier);
	}
}

float AARShopGameMode::GetItemQualityMultiplier(const EARVendingQualityTier QualityTier) const
{
	switch (QualityTier)
	{
	case EARVendingQualityTier::Low:
		return FMath::Max(0.0f, ItemQualityLowMultiplier);
	case EARVendingQualityTier::Standard:
		return FMath::Max(0.0f, ItemQualityStandardMultiplier);
	case EARVendingQualityTier::High:
		return FMath::Max(0.0f, ItemQualityHighMultiplier);
	case EARVendingQualityTier::Premium:
		return FMath::Max(0.0f, ItemQualityPremiumMultiplier);
	default:
		return FMath::Max(0.0f, ItemQualityStandardMultiplier);
	}
}

bool AARShopGameMode::QueueVendingStockedBowl(const FARVendingStockedBowlEntry& Entry)
{
	if (!HasAuthority())
	{
		return false;
	}

	UGameInstance* GI = GetGameInstance();
	UARSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<UARSaveSubsystem>() : nullptr;
	UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	if (!SaveSubsystem || !SaveGame)
	{
		return false;
	}

	FARVendingStockedBowlEntry SanitizedEntry = Entry;
	auto SanitizeColor = [](const EARAffinityColor InColor)
	{
		return InColor == EARAffinityColor::Unknown ? EARAffinityColor::None : InColor;
	};
	auto SanitizeQuality = [](const EARVendingQualityTier InTier)
	{
		return StaticEnum<EARVendingQualityTier>()->IsValidEnumValue(static_cast<int64>(InTier))
			? InTier
			: EARVendingQualityTier::Standard;
	};
	SanitizedEntry.BowlSpec.Noodles.SlotType = EARRamenStationType::Noodles;
	SanitizedEntry.BowlSpec.Broth.SlotType = EARRamenStationType::Broth;
	SanitizedEntry.BowlSpec.Toppings.SlotType = EARRamenStationType::Toppings;
	SanitizedEntry.BowlSpec.Noodles.Color = SanitizeColor(SanitizedEntry.BowlSpec.Noodles.Color);
	SanitizedEntry.BowlSpec.Broth.Color = SanitizeColor(SanitizedEntry.BowlSpec.Broth.Color);
	SanitizedEntry.BowlSpec.Toppings.Color = SanitizeColor(SanitizedEntry.BowlSpec.Toppings.Color);
	SanitizedEntry.QualityTier = SanitizeQuality(SanitizedEntry.QualityTier);
	SanitizedEntry.BowlSpec.Noodles.QualityTier = SanitizeQuality(SanitizedEntry.BowlSpec.Noodles.QualityTier);
	SanitizedEntry.BowlSpec.Broth.QualityTier = SanitizeQuality(SanitizedEntry.BowlSpec.Broth.QualityTier);
	SanitizedEntry.BowlSpec.Toppings.QualityTier = SanitizeQuality(SanitizedEntry.BowlSpec.Toppings.QualityTier);
	SaveGame->PendingVendingStockedBowls.Add(SanitizedEntry);
	SaveSubsystem->MarkSaveDirty();
	return true;
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
	AARShopGameState* ShopGameState = Cast<AARShopGameState>(SharedGameState);
	if (ShopGameState)
	{
		ShopGameState->SetBaseBowlPayout(BaseBowlPayout);
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
		// Fresh runtime with no active save object: still materialize canonical unpossessed character pawns.
		ReconcileInitialControlledShopPawns(nullptr);
		TryRestoreMissingCharacterPawns(nullptr);
		return;
	}

	FinalizePendingVendingPayout(SaveGame, SaveSubsystem, ShopGameState);

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
	ReconcileInitialControlledShopPawns(SaveGame);
	if (TryRestoreMissingCharacterPawns(SaveGame))
	{
		SaveSubsystem->MarkSaveDirty();
	}
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
	if (TryRestoreMissingCharacterPawns(SaveGame))
	{
		if (SaveSubsystem && SaveGame)
		{
			SaveSubsystem->MarkSaveDirty();
		}
	}
}

bool AARShopGameMode::TryRestoreMissingCharacterPawns(UARSaveGame* SaveGame) const
{
	if (!HasAuthority())
	{
		return false;
	}

	TSet<FGameplayTag> CandidateCharacterTags;
	CandidateCharacterTags.Reserve(4);
	if (SaveGame)
	{
		for (const FARCharacterSaveData& ExistingCharacterState : SaveGame->CharacterStates)
		{
			const FGameplayTag ExistingTag = ARPlayer::NormalizeCharacterTag(ExistingCharacterState.CharacterTag);
			if (ExistingTag.IsValid())
			{
				CandidateCharacterTags.Add(ExistingTag);
			}
		}
	}

	for (const FGameplayTag& OrderedTag : PlayableCharacterSwitchOrder)
	{
		const FGameplayTag CanonicalTag = ARPlayer::NormalizeCharacterTag(OrderedTag);
		if (CanonicalTag.IsValid())
		{
			CandidateCharacterTags.Add(CanonicalTag);
		}
	}

	// Keep explicit Brother/Sister seeds for maps/save data that have not yet authored switch-order tags.
	{
		const FGameplayTag BrotherTag = ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Brother);
		const FGameplayTag SisterTag = ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Sister);
		if (BrotherTag.IsValid())
		{
			CandidateCharacterTags.Add(BrotherTag);
		}
		if (SisterTag.IsValid())
		{
			CandidateCharacterTags.Add(SisterTag);
		}
	}

	AARGameStateBase* ShopGameState = GetGameState<AARGameStateBase>();
	bool bMutatedAny = false;
	for (const FGameplayTag& CharacterTag : CandidateCharacterTags)
	{
		if (SaveGame)
		{
			FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(CharacterTag);
			const bool bIsControlledCharacter = ShopGameState && ShopGameState->GetPlayerStateByCharacterTag(CharacterTag);
			if (!bIsControlledCharacter && !CharacterState.ShopSnapshot.bHasCharacterTransform)
			{
				AARPlayerStateBase* OwnerPlayerState = ResolveShopCharacterOwnerForTag(CharacterTag);
				FTransform DefaultSpawnTransform = FTransform::Identity;
				if (OwnerPlayerState && ResolveDefaultShopCharacterSpawnTransform(CharacterTag, OwnerPlayerState, DefaultSpawnTransform))
				{
					CharacterState.ShopSnapshot.bHasCharacterTransform = true;
					CharacterState.ShopSnapshot.CharacterTransform = DefaultSpawnTransform;
					bMutatedAny = true;
				}
			}

			bMutatedAny = TryRestoreMissingCharacterPawn(CharacterState) || bMutatedAny;
		}
		else
		{
			FARCharacterSaveData TransientCharacterState;
			TransientCharacterState.CharacterTag = CharacterTag;
			bMutatedAny = TryRestoreMissingCharacterPawn(TransientCharacterState) || bMutatedAny;
		}
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[ShopGameMode] Missing-pawn materialization pass complete: SaveGamePresent=%d CandidateTags=%d MutatedAny=%d."),
		SaveGame ? 1 : 0,
		CandidateCharacterTags.Num(),
		bMutatedAny ? 1 : 0);

	return bMutatedAny;
}

bool AARShopGameMode::ReconcileInitialControlledShopPawns(UARSaveGame* SaveGame) const
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UARCharacterSubsystem* CharacterSubsystem = World ? World->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	if (!World || !CharacterSubsystem)
	{
		return false;
	}

	bool bMutatedAny = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ResolveCharacterTagForController(Controller));
		if (!Controller || !PlayerState || !CharacterTag.IsValid())
		{
			continue;
		}

		bool bCreatedRuntime = false;
		AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(PlayerState, CharacterTag, bCreatedRuntime);
		if (!Runtime)
		{
			continue;
		}

		PlayerState->SetCurrentCharacterRuntime(Runtime);

		APawn* DesiredPawn = Runtime->GetCurrentPawn();
		APawn* ControlledPawn = Controller->GetPawn();
		if (!DesiredPawn)
		{
			DesiredPawn = ControlledPawn;
		}

		if (!DesiredPawn)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ShopGameMode] Controlled-pawn reconciliation skipped for '%s': no pawn exists yet for character '%s'."),
				*GetNameSafe(PlayerState),
				*CharacterTag.ToString());
			continue;
		}

		if (DesiredPawn != ControlledPawn)
		{
			if (DesiredPawn->GetController() && DesiredPawn->GetController() != Controller)
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[ShopGameMode] Controlled-pawn reconciliation skipped for '%s': pawn '%s' for '%s' is controlled by '%s'."),
					*GetNameSafe(PlayerState),
					*GetNameSafe(DesiredPawn),
					*CharacterTag.ToString(),
					*GetNameSafe(DesiredPawn->GetController()));
				continue;
			}

			if (ControlledPawn)
			{
				Controller->UnPossess();
			}

			Controller->Possess(DesiredPawn);
			bMutatedAny = true;

			if (ControlledPawn && ControlledPawn != DesiredPawn && !ControlledPawn->GetController())
			{
				ControlledPawn->Destroy();
			}
		}

		FTransform DesiredTransform = FTransform::Identity;
		bool bHasDesiredTransform = false;
		FARCharacterSaveData* CharacterState = nullptr;
		int32 CharacterStateIndex = INDEX_NONE;
		if (SaveGame)
		{
			CharacterState = SaveGame->FindCharacterStateDataMutable(CharacterTag, CharacterStateIndex);
			if (CharacterState && CharacterState->ShopSnapshot.bHasCharacterTransform)
			{
				DesiredTransform = CharacterState->ShopSnapshot.CharacterTransform;
				bHasDesiredTransform = true;
			}
		}

		if (!bHasDesiredTransform)
		{
			bHasDesiredTransform = ResolveDefaultShopCharacterSpawnTransform(CharacterTag, PlayerState, DesiredTransform);
		}

		if (bHasDesiredTransform)
		{
			DesiredPawn->SetActorTransform(DesiredTransform, false, nullptr, ETeleportType::TeleportPhysics);
			bMutatedAny = true;
		}

		CharacterSubsystem->BindRuntimePawn(Runtime, DesiredPawn);
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[ShopGameMode] Controlled-pawn reconciliation complete: SaveGamePresent=%d MutatedAny=%d."),
		SaveGame ? 1 : 0,
		bMutatedAny ? 1 : 0);

	return bMutatedAny;
}

UClass* AARShopGameMode::ResolveShopPawnClassForCharacterTag(const FGameplayTag CharacterTag, const TCHAR* CharacterTagSource, AController* InController) const
{
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

		// Allow broader parent-tag mappings for canonical keys.
		for (const TPair<FGameplayTag, TSubclassOf<APawn>>& Entry : ShopPawnClassByCharacterTag)
		{
			if (!Entry.Value)
			{
				continue;
			}

			if (Entry.Key.IsValid() && CharacterTag.MatchesTag(Entry.Key))
			{
				UE_LOG(
					ARLog,
					Verbose,
					TEXT("[ShopGameMode] Pawn class parent-tag hit: QueryTag=%s EntryTag=%s Class=%s."),
					*CharacterTag.ToString(),
					*Entry.Key.ToString(),
					*GetNameSafe(Entry.Value.Get()));
				return Entry.Value.Get();
			}
		}
	}

	if (FallbackShopPawnClass)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ShopGameMode] Pawn class falling back to FallbackShopPawnClass '%s' for character source '%s' (CharacterTag=%s)."),
			*GetNameSafe(FallbackShopPawnClass.Get()),
			CharacterTagSource ? CharacterTagSource : TEXT("Unknown"),
			*CharacterTag.ToString());
		return FallbackShopPawnClass.Get();
	}

	UE_LOG(
		ARLog,
		Warning,
		TEXT("[ShopGameMode] Pawn class falling back to Super for character source '%s' controller='%s' (CharacterTag=%s)."),
		CharacterTagSource ? CharacterTagSource : TEXT("Unknown"),
		*GetNameSafe(InController),
		*CharacterTag.ToString());
	return const_cast<AARShopGameMode*>(this)->Super::GetDefaultPawnClassForController_Implementation(InController);
}

AARPlayerStateBase* AARShopGameMode::ResolveShopCharacterOwnerForTag(const FGameplayTag CharacterTag) const
{
	AARGameStateBase* ShopGameState = GetGameState<AARGameStateBase>();
	if (!ShopGameState)
	{
		return nullptr;
	}

	if (AARPlayerStateBase* TaggedPlayerState = ShopGameState->GetPlayerStateByCharacterTag(CharacterTag))
	{
		const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
		if (OwningController && OwningController->PlayerState == TaggedPlayerState)
		{
			return TaggedPlayerState;
		}
	}

	const TArray<AARPlayerStateBase*> Players = ShopGameState->GetPlayerStates();
	for (AARPlayerStateBase* PlayerState : Players)
	{
		const AController* OwningController = PlayerState ? Cast<AController>(PlayerState->GetOwner()) : nullptr;
		if (OwningController && OwningController->PlayerState == PlayerState)
		{
			return PlayerState;
		}
	}

	return nullptr;
}

bool AARShopGameMode::ResolveDefaultShopCharacterSpawnTransform(
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AARTaggedPlayerStart* BestTaggedStart = nullptr;
	for (TActorIterator<AARTaggedPlayerStart> It(World); It; ++It)
	{
		AARTaggedPlayerStart* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->MatchesSpawnIdentityTag(CharacterTag, true))
		{
			BestTaggedStart = Candidate;
			break;
		}

		if (!BestTaggedStart && Candidate->MatchesSpawnIdentityTag(CharacterTag, false))
		{
			BestTaggedStart = Candidate;
		}
	}

	if (BestTaggedStart)
	{
		OutTransform = BestTaggedStart->GetActorTransform();
		return true;
	}

	if (TActorIterator<APlayerStart> StartIt(World); StartIt)
	{
		if (APlayerStart* DefaultStart = *StartIt)
		{
			OutTransform = DefaultStart->GetActorTransform();
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ShopGameMode] Missing tagged start for '%s'. Falling back to first PlayerStart '%s'."),
				*CharacterTag.ToString(),
				*GetNameSafe(DefaultStart));
			return true;
		}
	}

	if (const APawn* OwnerPawn = OwnerPlayerState ? OwnerPlayerState->GetPawn() : nullptr)
	{
		OutTransform = OwnerPawn->GetActorTransform();
		FVector Location = OutTransform.GetLocation();
		Location += OutTransform.GetUnitAxis(EAxis::Y) * 150.0f;
		OutTransform.SetLocation(Location);
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ShopGameMode] Missing tagged/default PlayerStart for '%s'. Falling back near owner pawn '%s'."),
			*CharacterTag.ToString(),
			*GetNameSafe(OwnerPawn));
		return true;
	}

	UE_LOG(
		ARLog,
		Warning,
		TEXT("[ShopGameMode] Missing all spawn transform sources for '%s'; using identity transform."),
		*CharacterTag.ToString());
	return true;
}

bool AARShopGameMode::TryRestoreMissingCharacterPawn(FARCharacterSaveData& CharacterState) const
{
	if (!HasAuthority())
	{
		return false;
	}

	const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(CharacterState.CharacterTag);
	if (!CharacterTag.IsValid())
	{
		return false;
	}

	UWorld* World = GetWorld();
	AARGameStateBase* ShopGameState = GetGameState<AARGameStateBase>();
	UARCharacterSubsystem* CharacterSubsystem = World ? World->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	if (!CharacterSubsystem)
	{
		return false;
	}

	if (ShopGameState)
	{
		if (AARPlayerStateBase* TaggedPlayerState = ShopGameState->GetPlayerStateByCharacterTag(CharacterTag))
		{
			const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
			const bool bHasActiveController = OwningController && OwningController->PlayerState == TaggedPlayerState;
			if (bHasActiveController)
			{
				// Controlled characters are handled by the controller restore path.
				UE_LOG(ARLog, VeryVerbose, TEXT("[ShopGameMode] Skip missing-pawn restore for '%s': character is currently controlled by '%s'."),
					*CharacterTag.ToString(),
					*GetNameSafe(OwningController));
				return false;
			}
		}
	}

	AARPlayerStateBase* OwnerPlayerState = ResolveShopCharacterOwnerForTag(CharacterTag);
	if (!OwnerPlayerState)
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ShopGameMode] Skipping missing-pawn materialization for '%s': no active player-state owner was available."),
			*CharacterTag.ToString());
		return false;
	}

	bool bCreatedRuntime = false;
	AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(OwnerPlayerState, CharacterTag, bCreatedRuntime);
	if (!Runtime)
	{
		return false;
	}

	APawn* ExistingPawn = Runtime->GetCurrentPawn();
	if (ExistingPawn && ExistingPawn->GetController())
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[ShopGameMode] Skip missing-pawn restore for '%s': runtime pawn '%s' is already possessed by '%s'."),
			*CharacterTag.ToString(),
			*GetNameSafe(ExistingPawn),
			*GetNameSafe(ExistingPawn->GetController()));
		return false;
	}

	UClass* PawnClass = ResolveShopPawnClassForCharacterTag(CharacterTag, TEXT("ShopSnapshot"));
	if (!PawnClass || !PawnClass->IsChildOf(APawn::StaticClass()))
	{
		return false;
	}

	FTransform SpawnTransform = FTransform::Identity;
	if (CharacterState.ShopSnapshot.bHasCharacterTransform)
	{
		SpawnTransform = CharacterState.ShopSnapshot.CharacterTransform;
	}
	else
	{
		if (!ResolveDefaultShopCharacterSpawnTransform(CharacterTag, OwnerPlayerState, SpawnTransform))
		{
			return false;
		}
	}

	APawn* PawnToRestore = ExistingPawn;
	if (!PawnToRestore)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		PawnToRestore = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams);
		if (!PawnToRestore)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[ShopGameMode] Failed to spawn restored pawn for character '%s' using class '%s'."),
				*CharacterTag.ToString(),
				*GetNameSafe(PawnClass));
			return false;
		}

		UE_LOG(
			ARLog,
			Log,
			TEXT("[ShopGameMode] Spawned missing unpossessed pawn '%s' for character '%s' (OwnerPlayerState='%s')."),
			*GetNameSafe(PawnToRestore),
			*CharacterTag.ToString(),
			*GetNameSafe(OwnerPlayerState));
	}
	else
	{
		PawnToRestore->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[ShopGameMode] Reused existing unpossessed pawn '%s' for character '%s'."),
			*GetNameSafe(PawnToRestore),
			*CharacterTag.ToString());
	}

	CharacterSubsystem->BindRuntimePawn(Runtime, PawnToRestore);

	if (UARShopCarryComponent* CarryComponent = PawnToRestore->FindComponentByClass<UARShopCarryComponent>())
	{
		if (CharacterState.ShopSnapshot.bHasHeldItem)
		{
			if (!RestoreHeldShopItemSnapshot(CarryComponent, CharacterState.ShopSnapshot.HeldItem))
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[ShopGameMode] Restored pawn for '%s' but failed to restore held item snapshot."),
					*CharacterTag.ToString());
			}
		}
		else
		{
			AActor* ExistingHeldActor = CarryComponent->ClearHeldActor(false);
			if (ExistingHeldActor)
			{
				ExistingHeldActor->Destroy();
			}
		}
	}

	if (CharacterState.ShopSnapshot.bHasCharacterTransform || CharacterState.ShopSnapshot.bHasHeldItem)
	{
		CharacterState.ShopSnapshot = FARCharacterShopSnapshot();
	}
	return true;
}

UClass* AARShopGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const AARPlayerStateBase* PlayerState = InController ? InController->GetPlayerState<AARPlayerStateBase>() : nullptr;
	const FGameplayTag PendingCharacterTag = ARPlayer::NormalizeCharacterTag(GetPendingSpawnCharacterTagForController(InController));
	const FGameplayTag RuntimeCharacterTag = PlayerState ? ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag()) : FGameplayTag();
	const FGameplayTag ChoiceCharacterTag = PlayerState ? ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked()) : FGameplayTag();
	const FGameplayTag CharacterTag = PendingCharacterTag.IsValid() ? PendingCharacterTag : (RuntimeCharacterTag.IsValid() ? RuntimeCharacterTag : ChoiceCharacterTag);
	const TCHAR* CharacterTagSource = PendingCharacterTag.IsValid() ? TEXT("PendingChoosePlayerStart") : TEXT("PlayerState");
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[ShopGameMode] Resolve pawn class for controller='%s' playerSlotId=%d characterTag=%s source=%s."),
		*GetNameSafe(InController),
		PlayerState ? PlayerState->GetPlayerSlotId() : 0,
		*CharacterTag.ToString(),
		CharacterTagSource);
	return ResolveShopPawnClassForCharacterTag(CharacterTag, CharacterTagSource, InController);
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
		if (!SpawnClass || !SpawnClass->IsChildOf(AARCarryItemBase::StaticClass()))
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
		AARCarryItemBase* Spawned = World->SpawnActor<AARCarryItemBase>(SpawnClass, Snapshot.WorldTransform, SpawnParams);
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
			MeatActor->SetMeatDataByTag(Snapshot.MeatTag, Snapshot.MeatColor, FMath::Max(1, Snapshot.MeatAmount), Snapshot.MeatQualityTier);
		}
		else if (AARRamenBowlActor* BowlActor = Cast<AARRamenBowlActor>(Spawned))
		{
			BowlActor->ClearBowl();
			const int32 FillStep = FMath::Clamp(Snapshot.BowlFillStep, 0, 3);
			if (FillStep >= 1 && !BowlActor->TryApplyFillFromStation(
				EARRamenStationType::Noodles,
				Snapshot.BowlSpec.Noodles.Color,
				Snapshot.BowlSpec.Noodles.MeatTag,
				Snapshot.BowlSpec.Noodles.QualityTier))
			{
				Spawned->Destroy();
				continue;
			}

			if (FillStep >= 2 && !BowlActor->TryApplyFillFromStation(
				EARRamenStationType::Broth,
				Snapshot.BowlSpec.Broth.Color,
				Snapshot.BowlSpec.Broth.MeatTag,
				Snapshot.BowlSpec.Broth.QualityTier))
			{
				Spawned->Destroy();
				continue;
			}

			if (FillStep >= 3 && !BowlActor->TryApplyFillFromStation(
				EARRamenStationType::Toppings,
				Snapshot.BowlSpec.Toppings.Color,
				Snapshot.BowlSpec.Toppings.MeatTag,
				Snapshot.BowlSpec.Toppings.QualityTier))
			{
				Spawned->Destroy();
				continue;
			}
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

	const AARCharacterStateRuntime* Runtime = PlayerState->GetCurrentCharacterRuntime();
	const FGameplayTag CharacterTag = Runtime
		? ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag())
		: ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
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
	if (!SpawnClass || !SpawnClass->IsChildOf(AARCarryItemBase::StaticClass()))
	{
		UE_LOG(ARLog, Warning, TEXT("[ShopGameMode] Skipping invalid held item restore class '%s'."),
			*Snapshot.ActorClass.ToSoftObjectPath().ToString());
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AARCarryItemBase* SpawnedItem = World->SpawnActor<AARCarryItemBase>(SpawnClass, OwnerActor->GetActorTransform(), SpawnParams);
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
		MeatActor->SetMeatDataByTag(Snapshot.MeatTag, Snapshot.MeatColor, FMath::Max(1, Snapshot.MeatAmount), Snapshot.MeatQualityTier);
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
	if (FillStep >= 1 && !BowlActor->TryApplyFillFromStation(
		EARRamenStationType::Noodles,
		Snapshot.BowlSpec.Noodles.Color,
		Snapshot.BowlSpec.Noodles.MeatTag,
		Snapshot.BowlSpec.Noodles.QualityTier))
	{
		return false;
	}

	if (FillStep >= 2 && !BowlActor->TryApplyFillFromStation(
		EARRamenStationType::Broth,
		Snapshot.BowlSpec.Broth.Color,
		Snapshot.BowlSpec.Broth.MeatTag,
		Snapshot.BowlSpec.Broth.QualityTier))
	{
		return false;
	}

	if (FillStep >= 3 && !BowlActor->TryApplyFillFromStation(
		EARRamenStationType::Toppings,
		Snapshot.BowlSpec.Toppings.Color,
		Snapshot.BowlSpec.Toppings.MeatTag,
		Snapshot.BowlSpec.Toppings.QualityTier))
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

void AARShopGameMode::FinalizePendingVendingPayout(UARSaveGame* SaveGame, UARSaveSubsystem* SaveSubsystem, AARShopGameState* ShopGameState) const
{
	if (!HasAuthority() || !SaveGame || !SaveSubsystem || !ShopGameState || SaveGame->PendingVendingStockedBowls.IsEmpty())
	{
		return;
	}

	int32 TotalPayout = 0;
	for (const FARVendingStockedBowlEntry& Entry : SaveGame->PendingVendingStockedBowls)
	{
		TotalPayout += ResolveVendingBowlPayout(Entry);
	}

	SaveGame->PendingVendingStockedBowls.Reset();
	if (TotalPayout > 0)
	{
		ShopGameState->SetMoneyFromSave(ShopGameState->GetMoney() + TotalPayout);
	}

	SaveSubsystem->MarkSaveDirty();
}

int32 AARShopGameMode::ResolveCombinedMeatValue(const FARRamenBowlSpec& BowlSpec) const
{
	UGameInstance* GI = GetGameInstance();
	const UARItemDefinitionSubsystem* ItemDefinitions = GI ? GI->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	if (!ItemDefinitions)
	{
		return 0;
	}

	const auto ResolveSlotValue = [this, ItemDefinitions](const FARRamenBowlSlotSpec& SlotSpec)
	{
		const int32 SlotBaseValue = FMath::Max(0, ItemDefinitions->ResolveBowlSlotItemValue(SlotSpec));
		const float SlotQualityMultiplier = GetItemQualityMultiplier(SlotSpec.QualityTier);
		return FMath::Max(0, FMath::RoundToInt(static_cast<float>(SlotBaseValue) * SlotQualityMultiplier));
	};

	return ResolveSlotValue(BowlSpec.Noodles)
		+ ResolveSlotValue(BowlSpec.Broth)
		+ ResolveSlotValue(BowlSpec.Toppings);
}

int32 AARShopGameMode::ResolveVendingBowlPayout(const FARVendingStockedBowlEntry& Entry) const
{
	const int32 CombinedMeatValue = ResolveCombinedMeatValue(Entry.BowlSpec);
	const float Multiplier = GetVendingQualityMultiplier(Entry.QualityTier);
	return FMath::Max(0, FMath::RoundToInt(1.0f + (CombinedMeatValue * Multiplier)));
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
