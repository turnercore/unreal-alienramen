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
	struct FEligibleScrapyardSpawn
	{
		FGameplayTag ItemTag;
		FARScrapyardItemDefRow ItemDef;
		float Weight = 1.0f;
	};

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

			if (PlayerState->LoadoutTags.HasTag(Tag))
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
	if (!HasAuthority() || SpawnChance <= 0.0f)
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

	const int32 BaseSeed = OverrideSeed != 0
		? OverrideSeed
		: (World->GetGameState<AARScrapyardGameState>() ? World->GetGameState<AARScrapyardGameState>()->GetScrapyardRunSeed() : FMath::Rand());
	const UAREconomySettings* EconomySettings = GetDefault<UAREconomySettings>();
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(BaseSeed), GetTypeHash(EconomySettings ? EconomySettings->ScrapyardSpawnSeedSalt : 1337))));

	if (Rng.FRand() > FMath::Clamp(SpawnChance, 0.0f, 1.0f))
	{
		return nullptr;
	}

	TArray<FEligibleScrapyardSpawn> Eligible;
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

		FEligibleScrapyardSpawn& Entry = Eligible.AddDefaulted_GetRef();
		Entry.ItemTag = AllowedTag;
		Entry.ItemDef = Row;
		Entry.Weight = FMath::Max(0.01f, Row.Weight);
	}

	if (Eligible.IsEmpty())
	{
		return nullptr;
	}

	float TotalWeight = 0.0f;
	for (const FEligibleScrapyardSpawn& Entry : Eligible)
	{
		TotalWeight += Entry.Weight;
	}
	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	const float PickValue = Rng.FRandRange(0.0f, TotalWeight);
	float RunningWeight = 0.0f;
	const FEligibleScrapyardSpawn* Selected = nullptr;
	for (const FEligibleScrapyardSpawn& Entry : Eligible)
	{
		RunningWeight += Entry.Weight;
		if (PickValue <= RunningWeight)
		{
			Selected = &Entry;
			break;
		}
	}
	if (!Selected)
	{
		Selected = &Eligible.Last();
	}

	UClass* SpawnClass = FallbackCarryItemClass.Get();
	if (Selected->ItemDef.ItemModelClass.IsValid())
	{
		UClass* AuthoredClass = Selected->ItemDef.ItemModelClass.LoadSynchronous();
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

	SpawnedItem->SetScrapyardItemTag(Selected->ItemTag);
	SpawnedItem->SetFallbackScrapCost(FMath::Max(0, Selected->ItemDef.ScrapCost));
	ItemDefinitions->ApplyItemPhysicsProperties(SpawnedItem, Selected->ItemTag);
	return SpawnedItem;
}
