/**
 * @file ARDialogueTypes.h
 * @brief Shared dialogue and NPC runtime types for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ARPlayerTypes.h"
#include "ARSaveTypes.h"
#include "ARDialogueTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EARDialogueChoiceParticipation : uint8
{
	InitiatorOnly = 0,
	GroupChoice
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueChoiceDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag NextNodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer GrantProgressionTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueNodeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag NpcTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag NodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText LineText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSoftObjectPtr<UTexture2D> SpeakerPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bAllowRepeatAfterSeen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 MinLoveRating = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bRequiresWantSatisfied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer RequiredProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer BlockedProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer RequiredUnlockTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer BlockedUnlockTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer GrantProgressionTagsOnEnter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag NextNodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EARDialogueChoiceParticipation ChoiceParticipation = EARDialogueChoiceParticipation::InitiatorOnly;

	// Forces partner view subscription in Shop and blocks choice commits until both players are viewing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bForceEavesdropForImportantDecision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FARDialogueChoiceDef> Choices;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARNpcDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FGameplayTag NpcTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	int32 StartingLoveRating = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FGameplayTag InitialWantTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	int32 LoveIncreaseOnWantDelivery = 1;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARNpcRelationshipState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FGameplayTag NpcTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	int32 LoveRating = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FGameplayTag CurrentWantTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	bool bCurrentWantSatisfied = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARPlayerDialogueHistoryState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FARPlayerIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer SeenNodeTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueCanonicalChoiceState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag NodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceTag;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag ChoiceTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bCanChoose = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueClientView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag NpcTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag CurrentNodeTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag SpeakerTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText LineText;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TSoftObjectPtr<UTexture2D> SpeakerPortrait;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FARDialogueChoiceView> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bWaitingForChoice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bIsSharedSession = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bIsEavesdropping = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EARPlayerSlot InitiatorSlot = EARPlayerSlot::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EARPlayerSlot OwnerSlot = EARPlayerSlot::Unknown;
};
