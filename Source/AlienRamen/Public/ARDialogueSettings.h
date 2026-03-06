/**
 * @file ARDialogueSettings.h
 * @brief Dialogue system settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARDialogueSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Dialogue"))
class ALIENRAMEN_API UARDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// Root used for content lookup routing of dialogue rows.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag DialogueNodeRootTag;

	// Modes that share one global dialogue session across players.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTagContainer SharedDialogueModeTags;

	// Modes that use per-player dialogue sessions.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTagContainer PerPlayerDialogueModeTags;

	// Modes where active shared dialogue should pause the world.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FGameplayTagContainer PauseOnDialogueModeTags;
};
