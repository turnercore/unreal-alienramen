/**
 * @file ARGameStateModeStructs.h
 * @brief ARGameStateModeStructs header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARSaveTypes.h"
#include "GameplayTagContainer.h"
#include "ARGameStateModeStructs.generated.h"

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderGameStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Scrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTag ActiveFactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer ActiveFactionEffectTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARScrapyardGameStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Scrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTag ActiveFactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer ActiveFactionEffectTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopGameStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Scrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTag ActiveFactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer ActiveFactionEffectTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARLobbyGameStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Scrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTag ActiveFactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer ActiveFactionEffectTags;
};
