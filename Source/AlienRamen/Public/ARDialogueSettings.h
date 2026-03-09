/**
 * @file ARDialogueSettings.h
 * @brief Dialogue runtime/content settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARDialogueSettings.generated.h"

class UARDialogueConversationAsset;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Dialogue"))
class ALIENRAMEN_API UARDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// ContentLookup root tag used to resolve FDialogueSpeakerRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content")
	FGameplayTag SpeakerDefinitionRootTag;

	// Optional canonical root for conversation identity tags (used for validation/logging).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content")
	FGameplayTag ConversationDefinitionRootTag;

	// Explicit conversation asset registry consumed at runtime for offers/lookups.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content")
	TArray<TSoftObjectPtr<UARDialogueConversationAsset>> ConversationAssets;

	// Modes that use one shared session for all slotted players.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Mode")
	FGameplayTagContainer SharedDialogueModeTags;

	// Modes that use one session per active player.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Mode")
	FGameplayTagContainer PerPlayerDialogueModeTags;

	// Modes where shared dialogue should assert pause through AARGameStateBase external reasons.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Mode")
	FGameplayTagContainer PauseOnDialogueModeTags;

	// Safety guard for runtime node traversal loops.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Runtime", meta=(ClampMin="16", UIMin="16"))
	int32 MaxExecutionStepsPerAdvance = 1024;
};
