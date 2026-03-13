/**
 * @file ARScrapyardItemSpawner.h
 * @brief Deterministic scrapyard item spawn point with tag/rarity/rule filtering.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "ARScrapyardTypes.h"
#include "ARScrapyardItemSpawner.generated.h"

class AARScrapyardCarryItemBase;
class AARGameStateBase;
class UGameInstance;
struct FARScrapyardSpawnCandidate;

UCLASS(Blueprintable)
class ALIENRAMEN_API AARScrapyardItemSpawner : public AActor
{
	GENERATED_BODY()

public:
	AARScrapyardItemSpawner();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Spawner", meta = (BlueprintAuthorityOnly))
	AARScrapyardCarryItemBase* TrySpawnItem(int32 OverrideSeed = 0);

	// GameMode helper: build eligible items for this spawner (authority only).
	bool BuildEligibleItems(const class AARGameStateBase* GameState, class UGameInstance* GameInstance, TArray<FARScrapyardSpawnCandidate>& OutCandidates) const;

	// GameMode helper: spawn a specific item definition deterministically.
	AARScrapyardCarryItemBase* SpawnItemByDefinition(const FARScrapyardItemDefRow& ItemDef, const FGameplayTag& ItemTag, int32 OverrideSeed = 0);

	bool HasSpawned() const { return bHasSpawned; }
	void MarkSpawned() { bHasSpawned = true; }
	bool ShouldAlwaysSpawn() const { return bAlwaysSpawn; }
	float GetSpawnerWeight() const { return SpawnerWeight; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	bool bSpawnOnBeginPlay = true;

	/** Allowed item tags for this spawner (empty = no tag-based restriction; other constraints such as MaxRarity and RequiredRuntimeTags still apply). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	FGameplayTagContainer AllowedItemTags;

	/** Runtime tags required to allow spawning (e.g., progression/phase). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	FGameplayTagContainer RequiredRuntimeTags;

	/** Chance for this spawner to attempt a spawn when selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	EARScrapyardItemRarity MaxRarity = EARScrapyardItemRarity::Legendary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	TSubclassOf<AARScrapyardCarryItemBase> FallbackCarryItemClass;

	// When true, this spawner always attempts to spawn regardless of GameMode quotas/noise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	bool bAlwaysSpawn = false;

	// Designer-tunable weight used by GameMode selection; clamped to minimum > 0.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SpawnerWeight = 1.0f;

private:
	mutable bool bHasSpawned = false;
};

// Candidate built by spawner for GameMode budgeting/selection.
USTRUCT()
struct FARScrapyardSpawnCandidate
{
	GENERATED_BODY()

	// Owning spawner (non-owning pointer; validity checked before use).
	UPROPERTY()
	TWeakObjectPtr<AARScrapyardItemSpawner> Spawner;

	UPROPERTY()
	FGameplayTag ItemTag;

	UPROPERTY()
	FARScrapyardItemDefRow ItemDef;

	UPROPERTY()
	float ItemWeight = 1.0f;

	UPROPERTY()
	EARScrapyardItemRarity Rarity = EARScrapyardItemRarity::Common;

	// Computed by GameMode: spawner weight * noise score (0..inf).
	UPROPERTY()
	float SelectionWeight = 1.0f;
};
