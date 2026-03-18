#include "ARScrapyardItemSpawner.h"

#include "AREconomySettings.h"
#include "ARGameStateBase.h"
#include "ARItemDefinitionSubsystem.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARScrapyardCarryItemBase.h"
#include "ARScrapyardGameState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	static bool HasAnyRuntimeTagSource(const AARGameStateBase* GameState, const FGameplayTag Tag)
	{
		if (!GameState || !Tag.IsValid())
		{
			return false;
		}

		if (GameState->GetUnlocks().HasTag(Tag))
		{
			return true;
		}

		for (AARPlayerStateBase* PlayerState : GameState->GetPlayerStates())
		{
			if (!PlayerState)
			{
				continue;
			}

			if (PlayerState->GetCurrentCharacterLoadoutTags().HasTag(Tag))
			{
				return true;
			}
		}

		return false;
	}
}

AARScrapyardItemSpawner::AARScrapyardItemSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AARScrapyardItemSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		TrySpawnItem(0);
	}
}

AARScrapyardCarryItemBase* AARScrapyardItemSpawner::TrySpawnItem(const int32 OverrideSeed)
{
	if (!HasAuthority() || SpawnChance <= 0.0f || bHasSpawned)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance ? GameInstance->GetSubsystem<UARItemDefinitionSubsystem>() : nullptr;
	AARGameStateBase* GameState = World ? World->GetGameState<AARGameStateBase>() : nullptr;
	if (!World || !GameInstance || !GameState || !ItemDefinitions)
	{
		return nullptr;
	}

	if (!RequiredRuntimeTags.IsEmpty())
	{
		for (const FGameplayTag RequiredTag : RequiredRuntimeTags)
		{
			if (!HasAnyRuntimeTagSource(GameState, RequiredTag))
			{
				return nullptr;
			}
		}
	}

	TArray<FARScrapyardSpawnCandidate> Candidates;
	if (!BuildEligibleItems(GameState, GameInstance, Candidates))
	{
		return nullptr;
	}

	const int32 BaseSeed = OverrideSeed != 0
		? OverrideSeed
		: (World->GetGameState<AARScrapyardGameState>() ? World->GetGameState<AARScrapyardGameState>()->GetScrapyardRunSeed() : FMath::Rand());
	const UAREconomySettings* EconomySettings = GetDefault<UAREconomySettings>();
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(BaseSeed), GetTypeHash(EconomySettings ? EconomySettings->ScrapyardSpawnSeedSalt : 1337))));

	if (Rng.FRand() > FMath::Clamp(SpawnChance, 0.0f, 1.0f))
	{
		return nullptr;
	}

	float TotalWeight = 0.0f;
	for (const FARScrapyardSpawnCandidate& Entry : Candidates)
	{
		TotalWeight += Entry.ItemWeight;
	}
	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	const float PickValue = Rng.FRandRange(0.0f, TotalWeight);
	float RunningWeight = 0.0f;
	const FARScrapyardSpawnCandidate* Selected = nullptr;
	for (const FARScrapyardSpawnCandidate& Entry : Candidates)
	{
		RunningWeight += Entry.ItemWeight;
		if (PickValue <= RunningWeight)
		{
			Selected = &Entry;
			break;
		}
	}
	if (!Selected)
	{
		Selected = &Candidates.Last();
	}

	AARScrapyardCarryItemBase* SpawnedItem = SpawnItemByDefinition(Selected->ItemDef, Selected->ItemTag, Rng.RandHelper(INT32_MAX));
	return SpawnedItem;
}

bool AARScrapyardItemSpawner::BuildEligibleItems(const AARGameStateBase* GameState, UGameInstance* GameInstance, TArray<FARScrapyardSpawnCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	if (bHasSpawned || !GameInstance)
	{
		return false;
	}

	UARItemDefinitionSubsystem* ItemDefinitions = GameInstance->GetSubsystem<UARItemDefinitionSubsystem>();
	if (!ItemDefinitions)
	{
		return false;
	}

	if (!RequiredRuntimeTags.IsEmpty())
	{
		for (const FGameplayTag RequiredTag : RequiredRuntimeTags)
		{
			if (!HasAnyRuntimeTagSource(GameState, RequiredTag))
			{
				return false;
			}
		}
	}

	for (const FGameplayTag AllowedTag : AllowedItemTags)
	{
		FARScrapyardItemDefRow Row;
		if (!ItemDefinitions->ResolveItemDefinition(AllowedTag, Row))
		{
			continue;
		}

		if (static_cast<uint8>(Row.Rarity) > static_cast<uint8>(MaxRarity))
		{
			continue;
		}

		bool bRequirementsMet = true;
		for (const FGameplayTag SpawnTag : Row.SpawnConditionTags)
		{
			if (!HasAnyRuntimeTagSource(GameState, SpawnTag))
			{
				bRequirementsMet = false;
				break;
			}
		}
		if (!bRequirementsMet)
		{
			continue;
		}

		FARScrapyardSpawnCandidate& Entry = OutCandidates.AddDefaulted_GetRef();
		Entry.Spawner = const_cast<AARScrapyardItemSpawner*>(this);
		Entry.ItemTag = AllowedTag;
		Entry.ItemDef = Row;
		Entry.ItemWeight = FMath::Max(0.01f, Row.Weight);
		Entry.Rarity = Row.Rarity;
	}

	return !OutCandidates.IsEmpty();
}

AARScrapyardCarryItemBase* AARScrapyardItemSpawner::SpawnItemByDefinition(const FARScrapyardItemDefRow& ItemDef, const FGameplayTag& ItemTag, const int32 OverrideSeed)
{
	if (!HasAuthority() || bHasSpawned)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	(void)OverrideSeed;

	UClass* SpawnClass = FallbackCarryItemClass.Get();
	if (!ItemDef.ItemModelClass.IsNull())
	{
		UClass* AuthoredClass = ItemDef.ItemModelClass.LoadSynchronous();
		if (AuthoredClass && AuthoredClass->IsChildOf(AARScrapyardCarryItemBase::StaticClass()))
		{
			SpawnClass = AuthoredClass;
		}
	}

	if (!SpawnClass)
	{
		UE_LOG(ARLog, Warning, TEXT("[Scrapyard|Spawner] No valid spawn class configured for '%s'."), *GetNameSafe(this));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AARScrapyardCarryItemBase* SpawnedItem = World->SpawnActor<AARScrapyardCarryItemBase>(
		SpawnClass,
		GetActorTransform(),
		SpawnParams);
	if (!SpawnedItem)
	{
		return nullptr;
	}

	SpawnedItem->SetScrapyardItemTag(ItemTag);
	SpawnedItem->SetFallbackScrapCost(FMath::Max(0, ItemDef.ScrapCost));
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARItemDefinitionSubsystem* ItemDefinitions = GameInstance->GetSubsystem<UARItemDefinitionSubsystem>())
		{
			ItemDefinitions->ApplyItemPhysicsProperties(SpawnedItem, ItemTag);
		}
	}
	bHasSpawned = true;
	return SpawnedItem;
}
