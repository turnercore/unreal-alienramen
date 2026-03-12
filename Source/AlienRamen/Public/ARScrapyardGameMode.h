/**
 * @file ARScrapyardGameMode.h
 * @brief ARScrapyardGameMode header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameModeBase.h"
#include "ARScrapyardSpawnRules.h"
#include "ARScrapyardGameMode.generated.h"

class AController;
class APawn;
class APlayerController;
class FProperty;
class UScriptStruct;
class UClass;

UCLASS()
class ALIENRAMEN_API AARScrapyardGameMode : public AARGameModeBase
{
	GENERATED_BODY()

public:
	AARScrapyardGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard")
	TSubclassOf<APawn> FallbackScrapyardPawnClass;

	// Optional per-map spawn rule asset. When set, GameMode orchestrates scrapyard spawns.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Scrapyard|Spawns")
	TSoftObjectPtr<UARScrapyardSpawnRuleSet> SpawnRuleSet;

private:
	static FProperty* FindPropertyByNamePrefix(const UScriptStruct* StructType, const FString& Prefix);
	bool ResolveScrapyardPawnClassFromShipTag(FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const;
	static bool FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag);
	void InitializeScrapyardSpawns();
};
