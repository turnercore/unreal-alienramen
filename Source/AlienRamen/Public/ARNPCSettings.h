/**
 * @file ARNPCSettings.h
 * @brief NPC system settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARNPCSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen NPC"))
class ALIENRAMEN_API UARNPCSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// Root used for content lookup routing of NPC rows.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FGameplayTag NpcDefinitionRootTag;
};
