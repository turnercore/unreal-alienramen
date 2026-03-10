/**
 * @file ARCustomerSettings.h
 * @brief Shop customer-serving and station defaults.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARCustomerSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Customer"))
class ALIENRAMEN_API UARCustomerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen|NPC"); }
	virtual FName GetSectionName() const override { return TEXT("Customer"); }

	// Root tag used by TagContentResolver for FARCustomerDefinitionRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Content")
	FGameplayTag CustomerDefinitionRootTag;

	// Root tag used by TagContentResolver for FARShopStationConfigRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Content")
	FGameplayTag StationDefinitionRootTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Scoring")
	int32 HateRelationshipDeltaPoints = 0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Scoring")
	int32 OkRelationshipDeltaPoints = 1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Scoring")
	int32 LikeRelationshipDeltaPoints = 3;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Scoring")
	int32 LoveRelationshipDeltaPoints = 5;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Emotion", meta = (Categories = "Dialogue"))
	FGameplayTag HateEmotionTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Emotion", meta = (Categories = "Dialogue"))
	FGameplayTag OkEmotionTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Emotion", meta = (Categories = "Dialogue"))
	FGameplayTag LikeEmotionTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Emotion", meta = (Categories = "Dialogue"))
	FGameplayTag LoveEmotionTag;

	// Fallback station processing duration when no row/config override is found.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Station|Defaults", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float DefaultStationProcessingDurationSeconds = 1.5f;

	// Fallback station max stock when no row/config override is found.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Station|Defaults", meta = (ClampMin = "1", UIMin = "1"))
	int32 DefaultStationMaxStock = 5;

	// If no authored customer orders are available, generate a procedural fallback request.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Customer|Fallback")
	bool bAllowProceduralFallbackOrders = true;
};
