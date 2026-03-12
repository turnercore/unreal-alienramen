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

UCLASS(Blueprintable)
class ALIENRAMEN_API AARScrapyardItemSpawner : public AActor
{
	GENERATED_BODY()

public:
	AARScrapyardItemSpawner();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Scrapyard|Spawner", meta = (BlueprintAuthorityOnly))
	AARScrapyardCarryItemBase* TrySpawnItem(int32 OverrideSeed = 0);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	bool bSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	FGameplayTagContainer AllowedItemTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	FGameplayTagContainer RequiredRuntimeTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	EARScrapyardItemRarity MaxRarity = EARScrapyardItemRarity::Legendary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawner")
	TSubclassOf<AARScrapyardCarryItemBase> FallbackCarryItemClass;
};

