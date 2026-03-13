/**
 * @file ARFactionTypes.h
 * @brief Shared faction election types for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ARPlayerTypes.h"
#include "ARFactionTypes.generated.h"

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionPopularityModifierRule
{
	GENERATED_BODY()

	// Progression condition tag to check (for example Progression.Faction.CorpFriendly).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FGameplayTag ConditionTag;

	// Additive popularity delta applied when ConditionTag is present.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float Delta = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// Canonical faction identity tag. Expected format: Faction.Definition.<Leaf>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float BasePopularity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float MinPopularity = -100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float MaxPopularity = 100.0f;

	// Per-election popularity drift range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float DriftPerCycleMin = -2.0f;

	// Per-election popularity drift range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float DriftPerCycleMax = 2.0f;

	// Gameplay tags representing elected effects this faction applies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FGameplayTagContainer EffectTags;

	// Additive popularity rules evaluated against SaveSubsystem ProgressionTags.
	// Example: ConditionTag=Progression.Faction.CorpFriendly, Delta=+5 raises this faction when player unlocked that tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	TArray<FARFactionPopularityModifierRule> PopularityModifierRules;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionRuntimeState
{
	GENERATED_BODY()

	/** Faction identity tag matching FactionDefinition row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FGameplayTag FactionTag;

	/** Current popularity score (clamped to Min/Max from definition). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	float Popularity = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionVoteSelection
{
	GENERATED_BODY()

	/** Player slot casting this vote (P1/P2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown;

	/** Faction tag chosen by the player (must be in CurrentCandidates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FGameplayTag SelectedFactionTag;

	/** True once the player made a selection; false = abstain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	bool bHasSelection = false;
};

UENUM(BlueprintType)
enum class EARFactionWinnerReason : uint8
{
	SamePick = 0,
	DivergedRandom,
	SinglePick,
	NoVotesTopPopularity,
	DisabledByClout,
	NoValidFactions
};
