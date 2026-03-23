#include "ARScrapyardGameMode.h"

#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "AREconomySettings.h"
#include "ARGameStateBase.h"
#include "ARLoadoutSettings.h"
#include "ARLoadoutTypes.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffSubsystem.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARScrapyardGameState.h"
#include "ARScrapyardItemSpawner.h"
#include "ARScrapyardSpawnRules.h"
#include "ARTaggedPlayerStart.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "StructUtils/InstancedStruct.h"
#include "TagKeySubsystem.h"

namespace
{
	static FGameplayTag ResolveScrapyardCharacterTagForController(const AController* Controller)
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

	static void ApplyActiveRunBuffsForController(AARScrapyardGameMode* GameMode, AController* Controller)
	{
		if (!GameMode || !Controller || !GameMode->HasAuthority())
		{
			return;
		}

		if (UGameInstance* GameInstance = GameMode->GetGameInstance())
		{
			if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
			{
				if (AARPlayerStateBase* PlayerState = Controller->GetPlayerState<AARPlayerStateBase>())
				{
					RunBuffSubsystem->ApplyActiveRunBuffsToPlayerState(PlayerState);
				}
			}
		}
	}
}

AARScrapyardGameMode::AARScrapyardGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Scrapyard"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[ScrapyardGameMode] Required gameplay tag 'Mode.Scrapyard' is missing."));
	bAllowManualSaveInMode = false;
	bShareLocalPauseAcrossControllersInMode = true;
	bRouteModeTravelThroughTransitionMap = true;
	TransitionTravelMapURL = TEXT("/Game/Maps/Lvl_Loading");
	TransitionSourceMode = EARTransitionSourceMode::Scrapyard;
	TransitionReason = EARTransitionReason::ScrapyardToShop;
}

void AARScrapyardGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
		{
			if (AARGameStateBase* SharedGameState = World->GetGameState<AARGameStateBase>())
			{
				for (AARPlayerStateBase* PlayerState : SharedGameState->GetPlayerStates())
				{
					RunBuffSubsystem->ApplyActiveRunBuffsToPlayerState(PlayerState);
				}
			}
		}

		InitializeScrapyardSpawns();
	}

	ReconcileInitialControlledCharacterPawns();
	TryRestoreMissingCharacterPawns();
}

void AARScrapyardGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	ApplyActiveRunBuffsForController(this, NewPlayer);
}

void AARScrapyardGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	ApplyActiveRunBuffsForController(this, NewPlayer);
	TryRestoreMissingCharacterPawns();
}

UClass* AARScrapyardGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (InController)
	{
		const AARPlayerStateBase* PlayerState = InController->GetPlayerState<AARPlayerStateBase>();
		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ResolveScrapyardCharacterTagForController(InController));
		TSubclassOf<APawn> ResolvedPawnClass;
		if (PlayerState && ResolveScrapyardPawnClassForCharacterTag(CharacterTag, PlayerState, ResolvedPawnClass) && ResolvedPawnClass)
		{
			return ResolvedPawnClass.Get();
		}
	}

	if (FallbackScrapyardPawnClass)
	{
		return FallbackScrapyardPawnClass.Get();
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

bool AARScrapyardGameMode::ResolveScrapyardPawnClassForCharacterTag(
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	TSubclassOf<APawn>& OutPawnClass) const
{
	OutPawnClass = nullptr;

	FGameplayTagContainer LoadoutTags;
	if (ResolveCharacterOwnedLoadout(CharacterTag, OwnerPlayerState, LoadoutTags))
	{
		const FGameplayTag ShipRootTag = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Ship"), false);
		FGameplayTag ShipTag;
		if (ShipRootTag.IsValid()
			&& FindFirstTagUnderRoot(LoadoutTags, ShipRootTag, ShipTag)
			&& ResolveScrapyardPawnClassFromShipTag(ShipTag, OutPawnClass)
			&& OutPawnClass)
		{
			return true;
		}
	}

	if (FallbackScrapyardPawnClass)
	{
		OutPawnClass = FallbackScrapyardPawnClass;
		return true;
	}

	return false;
}

bool AARScrapyardGameMode::ResolveCharacterOwnedLoadout(
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	FGameplayTagContainer& OutLoadoutTags) const
{
	OutLoadoutTags.Reset();

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid() || !OwnerPlayerState)
	{
		return false;
	}

	if (const UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->FindCharacterRuntimeForPlayer(OwnerPlayerState, NormalizedCharacterTag))
		{
			OutLoadoutTags = Runtime->GetLoadoutTags();
			if (!OutLoadoutTags.IsEmpty())
			{
				return true;
			}
		}
	}

	if (const UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
	{
		if (const UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
		{
			FARCharacterSaveData CharacterState;
			int32 CharacterIndex = INDEX_NONE;
			if (SaveGame->FindCharacterStateDataByTag(NormalizedCharacterTag, CharacterState, CharacterIndex) && !CharacterState.LoadoutTags.IsEmpty())
			{
				OutLoadoutTags = CharacterState.LoadoutTags;
				return true;
			}
		}
	}

	const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
	OutLoadoutTags = LoadoutSettings ? LoadoutSettings->DefaultPlayerLoadoutTags : FGameplayTagContainer();
	return !OutLoadoutTags.IsEmpty();
}

AARPlayerStateBase* AARScrapyardGameMode::ResolveCharacterOwnerForTag(const FGameplayTag CharacterTag) const
{
	AARGameStateBase* SharedGameState = GetGameState<AARGameStateBase>();
	if (!SharedGameState)
	{
		return nullptr;
	}

	if (AARPlayerStateBase* TaggedPlayerState = SharedGameState->GetPlayerStateByCharacterTag(CharacterTag))
	{
		const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
		if (OwningController && OwningController->PlayerState == TaggedPlayerState)
		{
			return TaggedPlayerState;
		}
	}

	const TArray<AARPlayerStateBase*> Players = SharedGameState->GetPlayerStates();
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

bool AARScrapyardGameMode::ResolveCharacterSpawnTransform(
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
			return true;
		}
	}

	if (const APawn* OwnerPawn = OwnerPlayerState ? OwnerPlayerState->GetPawn() : nullptr)
	{
		OutTransform = OwnerPawn->GetActorTransform();
		FVector Location = OutTransform.GetLocation();
		Location += OutTransform.GetUnitAxis(EAxis::Y) * 150.0f;
		OutTransform.SetLocation(Location);
		return true;
	}

	return false;
}

bool AARScrapyardGameMode::ReconcileInitialControlledCharacterPawns() const
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
		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ResolveScrapyardCharacterTagForController(Controller));
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
			continue;
		}

		if (DesiredPawn != ControlledPawn)
		{
			if (DesiredPawn->GetController() && DesiredPawn->GetController() != Controller)
			{
				continue;
			}

			CharacterSubsystem->BindRuntimePawn(Runtime, DesiredPawn);

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
		if (ResolveCharacterSpawnTransform(CharacterTag, PlayerState, DesiredTransform))
		{
			DesiredPawn->SetActorTransform(DesiredTransform, false, nullptr, ETeleportType::TeleportPhysics);
			bMutatedAny = true;
		}

		CharacterSubsystem->BindRuntimePawn(Runtime, DesiredPawn);
	}

	return bMutatedAny;
}

bool AARScrapyardGameMode::TryRestoreMissingCharacterPawns() const
{
	if (!HasAuthority())
	{
		return false;
	}

	TSet<FGameplayTag> CandidateCharacterTags;
	for (const FGameplayTag& OrderedTag : PlayableCharacterSwitchOrder)
	{
		const FGameplayTag CanonicalTag = ARPlayer::NormalizeCharacterTag(OrderedTag);
		if (CanonicalTag.IsValid())
		{
			CandidateCharacterTags.Add(CanonicalTag);
		}
	}

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

	bool bMutatedAny = false;
	for (const FGameplayTag& CharacterTag : CandidateCharacterTags)
	{
		bMutatedAny = TryRestoreMissingCharacterPawn(CharacterTag) || bMutatedAny;
	}

	return bMutatedAny;
}

bool AARScrapyardGameMode::TryRestoreMissingCharacterPawn(const FGameplayTag CharacterTag) const
{
	if (!HasAuthority())
	{
		return false;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return false;
	}

	AARGameStateBase* SharedGameState = GetGameState<AARGameStateBase>();
	if (SharedGameState)
	{
		if (AARPlayerStateBase* TaggedPlayerState = SharedGameState->GetPlayerStateByCharacterTag(NormalizedCharacterTag))
		{
			const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
			if (OwningController && OwningController->PlayerState == TaggedPlayerState)
			{
				return false;
			}
		}
	}

	AARPlayerStateBase* OwnerPlayerState = ResolveCharacterOwnerForTag(NormalizedCharacterTag);
	UWorld* World = GetWorld();
	UARCharacterSubsystem* CharacterSubsystem = World ? World->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	if (!OwnerPlayerState || !World || !CharacterSubsystem)
	{
		return false;
	}

	bool bCreatedRuntime = false;
	AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(OwnerPlayerState, NormalizedCharacterTag, bCreatedRuntime);
	if (!Runtime)
	{
		return false;
	}

	APawn* ExistingPawn = Runtime->GetCurrentPawn();
	if (ExistingPawn && ExistingPawn->GetController())
	{
		return false;
	}

	TSubclassOf<APawn> PawnClass;
	if (!ResolveScrapyardPawnClassForCharacterTag(NormalizedCharacterTag, OwnerPlayerState, PawnClass) || !PawnClass)
	{
		return false;
	}

	FTransform SpawnTransform = FTransform::Identity;
	if (!ResolveCharacterSpawnTransform(NormalizedCharacterTag, OwnerPlayerState, SpawnTransform))
	{
		return false;
	}

	APawn* PawnToBind = ExistingPawn;
	if (!PawnToBind)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		PawnToBind = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams);
		if (!PawnToBind)
		{
			return false;
		}
	}
	else
	{
		PawnToBind->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	CharacterSubsystem->BindRuntimePawn(Runtime, PawnToBind);
	return true;
}

void AARScrapyardGameMode::InitializeScrapyardSpawns()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	AARScrapyardGameState* ScrapyardGameState = World ? World->GetGameState<AARScrapyardGameState>() : nullptr;
	if (!World || !ScrapyardGameState)
	{
		return;
	}

	UARScrapyardSpawnRuleSet* RuleAsset = SpawnRuleSet.IsNull() ? nullptr : SpawnRuleSet.LoadSynchronous();
	if (!RuleAsset)
	{
		UE_LOG(ARLog, Error, TEXT("[ScrapyardGameMode] SpawnRuleSet is unset; managed scrapyard spawns cannot run for this map."));
		return;
	}

	const FARScrapyardSpawnRules& Rules = RuleAsset->Rules;
	const UAREconomySettings* EconomySettings = GetDefault<UAREconomySettings>();
	const int32 RunSeed = ScrapyardGameState->GetScrapyardRunSeed();
	FRandomStream GlobalRng(static_cast<int32>(HashCombine(GetTypeHash(RunSeed), GetTypeHash(EconomySettings ? EconomySettings->ScrapyardSpawnSeedSalt : 1337))));
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[ScrapyardGameMode] Managed spawns begin (RuleSet=%s, RunSeed=%d, MaxTotal=%d)."),
		*GetNameSafe(RuleAsset),
		RunSeed,
		Rules.MaxTotalSpawns);

	TArray<AActor*> FoundSpawners;
	UGameplayStatics::GetAllActorsOfClass(World, AARScrapyardItemSpawner::StaticClass(), FoundSpawners);
	if (FoundSpawners.IsEmpty())
	{
		UE_LOG(ARLog, Error, TEXT("[ScrapyardGameMode] No AARScrapyardItemSpawner actors found in level; nothing to spawn."));
		return;
	}

	struct FSpawnerContext
	{
		TWeakObjectPtr<AARScrapyardItemSpawner> Spawner;
		TArray<FARScrapyardSpawnCandidate> Candidates;
		float NoiseScore = 0.0f;
		bool bAlways = false;
	};

	TArray<FSpawnerContext> SpawnerContexts;
	SpawnerContexts.Reserve(FoundSpawners.Num());

	for (AActor* Actor : FoundSpawners)
	{
		AARScrapyardItemSpawner* Spawner = Cast<AARScrapyardItemSpawner>(Actor);
		if (!Spawner || Spawner->HasSpawned())
		{
			continue;
		}

		FSpawnerContext Context;
		Context.Spawner = Spawner;
		Context.bAlways = Spawner->ShouldAlwaysSpawn();
		Spawner->BuildEligibleItems(ScrapyardGameState, GetGameInstance(), Context.Candidates);
		if (Context.Candidates.IsEmpty())
		{
			continue;
		}

		const FVector Location = Spawner->GetActorLocation() / FMath::Max(1.0f, Rules.NoiseScale);
		const float RawNoise = FMath::PerlinNoise3D(Location);
		const float Noise01 = FMath::Clamp((RawNoise + 1.0f) * 0.5f, 0.0f, 1.0f);
		const float Jitter = GlobalRng.FRandRange(-Rules.NoiseJitter, Rules.NoiseJitter);
		Context.NoiseScore = FMath::Clamp(Noise01 + Jitter, 0.0f, 1.0f) * FMath::Max(0.01f, Spawner->GetSpawnerWeight());
		SpawnerContexts.Add(MoveTemp(Context));
	}

	if (SpawnerContexts.IsEmpty())
	{
		UE_LOG(ARLog, Error, TEXT("[ScrapyardGameMode] No eligible scrapyard spawners with candidates were found; managed spawns aborted."));
		return;
	}

	auto BuildWeightedCandidates = [&](const TOptional<EARScrapyardItemRarity> RarityFilter, const bool bApplyNoiseThreshold)
	{
		TArray<FARScrapyardSpawnCandidate> Result;
		for (const FSpawnerContext& Context : SpawnerContexts)
		{
			AARScrapyardItemSpawner* Spawner = Context.Spawner.Get();
			if (!Spawner || Spawner->HasSpawned())
			{
				continue;
			}

			if (bApplyNoiseThreshold && Context.NoiseScore < Rules.NoiseThreshold && !Context.bAlways)
			{
				continue;
			}

			for (const FARScrapyardSpawnCandidate& Candidate : Context.Candidates)
			{
				if (RarityFilter.IsSet() && Candidate.Rarity != RarityFilter.GetValue())
				{
					continue;
				}

				FARScrapyardSpawnCandidate Weighted = Candidate;
				Weighted.SelectionWeight = Candidate.ItemWeight * (Context.bAlways ? 1.0f : Context.NoiseScore) * FMath::Max(0.01f, Spawner->GetSpawnerWeight());
				Result.Add(Weighted);
			}
		}
		return Result;
	};

	auto PickAndSpawn = [&](const TArray<FARScrapyardSpawnCandidate>& Pool, FRandomStream& Stream) -> TOptional<EARScrapyardItemRarity>
	{
		if (Pool.IsEmpty())
		{
			UE_LOG(ARLog, VeryVerbose, TEXT("[ScrapyardGameMode] PickAndSpawn skipped: empty pool."));
			return TOptional<EARScrapyardItemRarity>();
		}

		float TotalWeight = 0.0f;
		for (const FARScrapyardSpawnCandidate& Candidate : Pool)
		{
			TotalWeight += FMath::Max(0.0f, Candidate.SelectionWeight);
		}
		if (TotalWeight <= 0.0f)
		{
			UE_LOG(ARLog, VeryVerbose, TEXT("[ScrapyardGameMode] PickAndSpawn skipped: zero total weight."));
			return TOptional<EARScrapyardItemRarity>();
		}

		const float PickValue = Stream.FRandRange(0.0f, TotalWeight);
		float Running = 0.0f;
		const FARScrapyardSpawnCandidate* Selected = nullptr;
		for (const FARScrapyardSpawnCandidate& Candidate : Pool)
		{
			Running += FMath::Max(0.0f, Candidate.SelectionWeight);
			if (PickValue <= Running)
			{
				Selected = &Candidate;
				break;
			}
		}
		if (!Selected)
		{
			Selected = &Pool.Last();
		}

		AARScrapyardItemSpawner* Spawner = Selected->Spawner.Get();
		if (!Spawner)
		{
			UE_LOG(ARLog, VeryVerbose, TEXT("[ScrapyardGameMode] PickAndSpawn skipped: spawner destroyed/invalid."));
			return TOptional<EARScrapyardItemRarity>();
		}

		const int32 SpawnSeed = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(RunSeed), GetTypeHash(Spawner->GetUniqueID())), GetTypeHash(Stream.RandHelper(INT32_MAX))));
		AARCarryItemBase* Spawned = Spawner->SpawnItemByDefinition(Selected->ItemDef, Selected->ItemTag, SpawnSeed);
		return Spawned ? TOptional<EARScrapyardItemRarity>(Selected->Rarity) : TOptional<EARScrapyardItemRarity>();
	};

	TMap<EARScrapyardItemRarity, int32> SpawnedByRarity;
	int32 TotalSpawned = 0;

	// Phase 0: Always-spawn spawners fire first.
	{
		TArray<FARScrapyardSpawnCandidate> AlwaysPool = BuildWeightedCandidates(TOptional<EARScrapyardItemRarity>(), false);
		for (const FARScrapyardSpawnCandidate& Candidate : AlwaysPool)
		{
			if (!Candidate.Spawner.IsValid() || !Candidate.Spawner->ShouldAlwaysSpawn())
			{
				continue;
			}
			if (Candidate.Spawner->HasSpawned())
			{
				continue;
			}

			const int32 SpawnSeed = GlobalRng.RandHelper(INT32_MAX);
			AARCarryItemBase* Spawned = Candidate.Spawner->SpawnItemByDefinition(Candidate.ItemDef, Candidate.ItemTag, SpawnSeed);
			if (Spawned)
			{
				SpawnedByRarity.FindOrAdd(Candidate.Rarity)++;
				++TotalSpawned;
				UE_LOG(
					ARLog,
					VeryVerbose,
					TEXT("[ScrapyardGameMode] AlwaysSpawn fired: %s (Rarity=%d)"),
					*Candidate.ItemTag.ToString(),
					static_cast<int32>(Candidate.Rarity));
			}
		}
	}

	// Phase 1: satisfy rarity minimums.
	for (uint8 RarityIdx = 0; RarityIdx <= static_cast<uint8>(EARScrapyardItemRarity::Legendary); ++RarityIdx)
	{
		const EARScrapyardItemRarity Rarity = static_cast<EARScrapyardItemRarity>(RarityIdx);
		const FARScrapyardRarityBudget Budget = Rules.GetBudgetForRarity(Rarity);
		int32& Current = SpawnedByRarity.FindOrAdd(Rarity);

		while (Current < Budget.MinCount && TotalSpawned < Rules.MaxTotalSpawns)
		{
			TArray<FARScrapyardSpawnCandidate> Pool = BuildWeightedCandidates(Rarity, true);
			TOptional<EARScrapyardItemRarity> SpawnedRarity = PickAndSpawn(Pool, GlobalRng);
			if (!SpawnedRarity.IsSet())
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[ScrapyardGameMode] Could not satisfy minimum for rarity %d (have %d, need %d)."),
					static_cast<int32>(Rarity),
					Current,
					Budget.MinCount);
				break;
			}
			++Current;
			++TotalSpawned;
		}
	}

	// Phase 2: fill within budgets.
	while (TotalSpawned < Rules.MaxTotalSpawns)
	{
		TArray<FARScrapyardSpawnCandidate> Pool;
		for (uint8 RarityIdx = 0; RarityIdx <= static_cast<uint8>(EARScrapyardItemRarity::Legendary); ++RarityIdx)
		{
			const EARScrapyardItemRarity Rarity = static_cast<EARScrapyardItemRarity>(RarityIdx);
			const FARScrapyardRarityBudget Budget = Rules.GetBudgetForRarity(Rarity);
			const int32 Current = SpawnedByRarity.FindOrAdd(Rarity);
			if (Budget.MaxCount > 0 && Current >= Budget.MaxCount)
			{
				continue;
			}

			TArray<FARScrapyardSpawnCandidate> RarityPool = BuildWeightedCandidates(Rarity, true);
			Pool.Append(RarityPool);
		}

		if (Pool.IsEmpty())
		{
			UE_LOG(ARLog, VeryVerbose, TEXT("[ScrapyardGameMode] Fill phase stopped: no eligible candidates within budgets."));
			break;
		}

		TOptional<EARScrapyardItemRarity> SpawnedRarity = PickAndSpawn(Pool, GlobalRng);
		if (!SpawnedRarity.IsSet())
		{
			break;
		}

		SpawnedByRarity.FindOrAdd(SpawnedRarity.GetValue())++;
		++TotalSpawned;

		if (TotalSpawned >= Rules.MinTotalSpawns && TotalSpawned >= Rules.MaxTotalSpawns)
		{
			break;
		}
	}

	FString RarityCountsStr;
	for (uint8 RarityIdx = 0; RarityIdx <= static_cast<uint8>(EARScrapyardItemRarity::Legendary); ++RarityIdx)
	{
		const EARScrapyardItemRarity Rarity = static_cast<EARScrapyardItemRarity>(RarityIdx);
		const int32 Count = SpawnedByRarity.FindOrAdd(Rarity);
		RarityCountsStr += FString::Printf(TEXT(" [%d:%d]"), RarityIdx, Count);
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[ScrapyardGameMode] Managed spawns finished. Total=%d%s"),
		TotalSpawned,
		*RarityCountsStr);
}

bool AARScrapyardGameMode::ResolveScrapyardPawnClassFromShipTag(const FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const
{
	OutPawnClass = nullptr;
	if (!ShipTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct ShipRow;
	FString ResolveError;
	if (!Resolver->TryResolveRowStructForTag(ShipTag, ShipRow, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[ScrapyardGameMode] Failed resolving ship row '%s': %s"),
			*ShipTag.ToString(),
			*ResolveError);
		return false;
	}

	const FARShipDefRow* ShipDef = ShipRow.GetPtr<FARShipDefRow>();
	if (!ShipDef)
	{
		const UScriptStruct* RowStruct = ShipRow.GetScriptStruct();
		UE_LOG(
			ARLog,
			Error,
			TEXT("[ScrapyardGameMode] Ship row '%s' resolved to unexpected struct '%s'; expected FARShipDefRow."),
			*ShipTag.ToString(),
			*GetNameSafe(RowStruct));
		return false;
	}

	if (ShipDef->ScrapyardPawnClass.IsNull())
	{
		return false;
	}

	if (UClass* PawnClass = ShipDef->ScrapyardPawnClass.LoadSynchronous())
	{
		OutPawnClass = PawnClass;
		return true;
	}

	UE_LOG(
		ARLog,
		Warning,
		TEXT("[ScrapyardGameMode] Ship row '%s' ScrapyardPawnClass failed to load (Path=%s)."),
		*ShipTag.ToString(),
		*ShipDef->ScrapyardPawnClass.ToString());
	return false;
}

bool AARScrapyardGameMode::FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();
	if (!RootTag.IsValid())
	{
		return false;
	}

	for (const FGameplayTag Tag : InTags)
	{
		if (Tag.IsValid() && Tag.MatchesTag(RootTag))
		{
			OutTag = Tag;
			return true;
		}
	}

	return false;
}
