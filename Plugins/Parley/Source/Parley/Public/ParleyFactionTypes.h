/**
 * @file ParleyFactionTypes.h
 * @brief Shared faction types for generic Parley faction runtime.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ParleyFactionTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct PARLEY_API FParleyFactionPopularityModifierRule
{
	GENERATED_BODY()

	/** Progression condition tag to check (for example Progression.Faction.CorpFriendly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Progression tag condition to check before applying Delta."))
	FGameplayTag ConditionTag;

	/** Additive popularity delta applied when ConditionTag is present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Signed popularity delta applied when ConditionTag is present."))
	float Delta = 0.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleyFactionDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Canonical faction identity tag. Expected format: Parley.Factions.<Leaf>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Canonical faction identity gameplay tag."))
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Display name for this faction in UI surfaces."))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Designer-authored description for this faction used by UI and codex surfaces."))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Default starting popularity for this faction when no persisted state exists."))
	float BasePopularity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Minimum allowed popularity value for this faction."))
	float MinPopularity = -100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Maximum allowed popularity value for this faction."))
	float MaxPopularity = 100.0f;

	/** Optional per-cycle popularity drift minimum. Game-owned voting layers can consume this if needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Optional drift minimum used by game-specific voting layers."))
	float DriftPerCycleMin = -2.0f;

	/** Optional per-cycle popularity drift maximum. Game-owned voting layers can consume this if needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Optional drift maximum used by game-specific voting layers."))
	float DriftPerCycleMax = 2.0f;

	/** Optional effect tags. Game-owned voting layers can interpret and apply these. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Optional effect tags a game-owned voting layer can apply when this faction is elected."))
	FGameplayTagContainer EffectTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Primary icon for this faction in compact UI surfaces."))
	TSoftObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Poster/hero texture for this faction in large UI surfaces."))
	TSoftObjectPtr<UTexture2D> PosterTexture;

	/** Additive popularity rules evaluated against game-provided progression tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Additive popularity modifiers evaluated against injected progression tags."))
	TArray<FParleyFactionPopularityModifierRule> PopularityModifierRules;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleyFactionRuntimeState
{
	GENERATED_BODY()

	/** Faction identity tag matching FactionDefinition row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Faction identity tag for this runtime popularity entry."))
	FGameplayTag FactionTag;

	/** Current popularity score (clamped to Min/Max from definition). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Current runtime popularity value for this faction."))
	float Popularity = 0.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleyFactionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Faction tag for this persisted popularity state entry."))
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Current persisted popularity value for the faction."))
	float Popularity = 0.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleyFactionSpeakerReputationState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Faction tag for this speaker reputation record."))
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (Categories = "Parley.Speaker", ToolTip = "Speaker tag for this reputation record (for example Parley.Speaker.Requester, Parley.Speaker.Owner, or a project-defined character speaker tag)."))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction", meta = (ToolTip = "Persisted reputation value for this faction-speaker pair."))
	float Reputation = 0.0f;
};

