/**
 * @file ARDialogueSettings.h
 * @brief Dialogue runtime/content settings for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARDialogueSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Dialogue"))
class ALIENRAMEN_API UARDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	// TagContentResolver root tag used to resolve FARDialogueSpeakerRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content")
	FGameplayTag SpeakerDefinitionRootTag;

	// TagContentResolver root tag used to resolve FARDialogueConversationAssetRow records.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Content")
	FGameplayTag ConversationDefinitionRootTag;

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
