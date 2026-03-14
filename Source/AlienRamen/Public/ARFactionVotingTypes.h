/**
 * @file ARFactionVotingTypes.h
 * @brief Game-owned faction voting definitions and runtime view structs.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ARFactionVotingTypes.generated.h"

/**
 * Game-owned vote-layer row that maps a Parley faction to election-specific metadata.
 * Rows are required for AR voting candidate generation.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionVotingDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Faction identity tag owned by Parley (for example Parley.Factions.DebugFaction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction|Voting", meta = (ToolTip = "Faction identity tag owned by Parley that this voting row targets."))
	FGameplayTag FactionTag;

	/** Election-specific effect tags to apply when this faction wins. Empty falls back to Parley definition effect tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction|Voting", meta = (ToolTip = "Election-specific effect tags applied when this faction wins. If empty, subsystem falls back to Parley faction definition effect tags."))
	FGameplayTagContainer ElectedEffectTags;

	/** Higher priority breaks ties after vote count and popularity comparisons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction|Voting", meta = (ToolTip = "Tie-break priority used after vote count and popularity. Higher values win.", ClampMin = "0", UIMin = "0"))
	int32 CandidatePriority = 0;

	/** Minimum clout required for this candidate to enter the election pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction|Voting", meta = (ToolTip = "Minimum faction clout required before this faction can appear as a voting candidate.", ClampMin = "0", UIMin = "0"))
	int32 MinRequiredClout = 0;

	/** Maximum clout allowed for this candidate (-1 means no maximum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction|Voting", meta = (ToolTip = "Maximum faction clout where this faction remains eligible. Use -1 for no maximum.", ClampMin = "-1", UIMin = "-1"))
	int32 MaxAllowedClout = -1;

	/** Enables/disables this candidate row without deleting authored data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction|Voting", meta = (ToolTip = "When false, this row is ignored by the faction voting subsystem."))
	bool bEnabled = true;
};

/** Runtime candidate snapshot produced by voting subsystem candidate resolution. */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionVotingCandidate
{
	GENERATED_BODY()

	/** Candidate faction identity tag. */
	UPROPERTY(BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Candidate faction identity tag."))
	FGameplayTag FactionTag;

	/** Effect tags that would become active if this candidate wins. */
	UPROPERTY(BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Faction effect tags that will be applied if this candidate wins the election."))
	FGameplayTagContainer ElectedEffectTags;

	/** Effective popularity used in ranking/tie-break flows. */
	UPROPERTY(BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Current effective popularity used by voting tie-break and candidate ranking logic."))
	float EffectivePopularity = 0.0f;

	/** Candidate priority from optional voting definition row (0 when not authored). */
	UPROPERTY(BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Tie-break priority from voting row metadata. Higher values are preferred."))
	int32 CandidatePriority = 0;
};

/** Current vote map entry keyed by player slot tag. */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARFactionVoteEntry
{
	GENERATED_BODY()

	/** Canonical player slot tag for this vote (for example Player.Slot.P1). */
	UPROPERTY(BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Canonical player slot tag that submitted this vote."))
	FGameplayTag PlayerSlotTag;

	/** Candidate faction currently selected by this player slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Faction tag this player slot is currently voting for."))
	FGameplayTag VotedFactionTag;
};

