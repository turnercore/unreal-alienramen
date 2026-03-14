/**
 * @file ARFactionVotingSettings.h
 * @brief Game-owned faction voting configuration layered on top of Parley faction runtime.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ARFactionVotingSettings.generated.h"

class UDataTable;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Faction Voting"))
class ALIENRAMEN_API UARFactionVotingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Faction Voting"); }

	/** DataTable that maps Parley faction tags to election-layer metadata and candidate policy. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Voting DataTable that layers election metadata on top of Parley faction definitions. This table is required for candidate generation."))
	TSoftObjectPtr<UDataTable> VotingDefinitionDataTable;

	/** Base number of candidates available at zero clout before clout expansion applies. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Base number of faction candidates exposed at zero clout.", ClampMin = "1", UIMin = "1"))
	int32 MinCandidateCount = 2;

	/** Hard cap for candidate count after clout expansion. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Maximum number of faction candidates that can appear in one election.", ClampMin = "1", UIMin = "1"))
	int32 MaxCandidateCount = 3;

	/** Clout amount required to unlock one additional candidate slot. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "Faction clout required per additional candidate slot above MinCandidateCount.", ClampMin = "1", UIMin = "1"))
	int32 CloutPerAdditionalCandidate = 10;

	/** Clears slot votes after a successful finalization. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Faction|Voting", meta = (ToolTip = "When true, submitted votes are cleared after election finalization."))
	bool bClearVotesAfterElection = true;
};
