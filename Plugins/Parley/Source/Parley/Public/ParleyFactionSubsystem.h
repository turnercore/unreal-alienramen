/**
 * @file ParleyFactionSubsystem.h
 * @brief Server-authoritative generic faction runtime for Parley.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ParleyFactionTypes.h"
#include "ParleyFactionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FParleyOnFactionPopularityChanged, FGameplayTag, FactionTag, float, Delta, float, NewTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FParleyOnFactionSpeakerReputationChanged, FGameplayTag, FactionTag, FGameplayTag, SpeakerTag, float, Delta, float, NewTotal);

UCLASS()
class PARLEY_API UParleyFactionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Resolves faction definition data for a faction tag via TagKey. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Faction", meta = (ToolTip = "Runs this faction subsystem operation on authoritative runtime state."))
	bool GetFactionDefinition(FGameplayTag FactionTag, FParleyFactionDefinitionRow& OutDefinition) const;

	/** Gets all configured faction tags resolved under FactionDefinitionRootTag. */
	UFUNCTION(BlueprintPure, Category = "Parley|Faction", meta = (ToolTip = "Returns current faction runtime state without mutating subsystem data."))
	void GetAllFactionTags(TArray<FGameplayTag>& OutFactionTags) const;

	/** Gets raw persisted popularity state for all factions known to the subsystem. */
	UFUNCTION(BlueprintPure, Category = "Parley|Faction", meta = (ToolTip = "Returns current faction runtime state without mutating subsystem data."))
	void GetFactionPopularityStates(TArray<FParleyFactionState>& OutStates) const;

	/** Gets raw persisted faction-speaker reputation state entries. */
	UFUNCTION(BlueprintPure, Category = "Parley|Faction", meta = (ToolTip = "Returns current faction runtime state without mutating subsystem data."))
	void GetFactionSpeakerReputationStates(TArray<FParleyFactionSpeakerReputationState>& OutStates) const;

	/** Returns current persisted popularity (or definition base popularity when no state was injected). */
	UFUNCTION(BlueprintPure, Category = "Parley|Faction", meta = (ToolTip = "Returns current faction runtime state without mutating subsystem data."))
	float GetFactionPopularity(FGameplayTag FactionTag) const;

	/** Returns current effective popularity (persisted popularity plus progression-driven modifier rules). */
	UFUNCTION(BlueprintPure, Category = "Parley|Faction", meta = (ToolTip = "Returns current faction runtime state without mutating subsystem data."))
	float GetEffectiveFactionPopularity(FGameplayTag FactionTag) const;

	/** Returns current persisted faction reputation for a speaker (0 when no state exists). */
	UFUNCTION(BlueprintPure, Category = "Parley|Faction", meta = (ToolTip = "Returns current faction runtime state without mutating subsystem data."))
	float GetFactionSpeakerReputation(FGameplayTag FactionTag, FGameplayTag SpeakerTag) const;

	/** Applies an immediate popularity delta to faction state. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Faction", meta = (ToolTip = "Runs this faction subsystem operation on authoritative runtime state."))
	bool ModifyFactionPopularity(FGameplayTag FactionTag, float DeltaPopularity);

	/** Applies an immediate faction reputation delta for a specific speaker tag. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Faction", meta = (ToolTip = "Runs this faction subsystem operation on authoritative runtime state."))
	bool ModifyFactionSpeakerReputation(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float DeltaReputation);

	/** Injects persisted faction popularity states from an external save bridge. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Faction", meta = (ToolTip = "Runs this faction subsystem operation on authoritative runtime state."))
	void SetFactionPopularityStates(const TArray<FParleyFactionState>& States);

	/** Injects persisted faction-speaker reputation states from an external save bridge. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Faction", meta = (ToolTip = "Runs this faction subsystem operation on authoritative runtime state."))
	void SetFactionSpeakerReputationStates(const TArray<FParleyFactionSpeakerReputationState>& States);

	/** Injects game-owned progression tags used for faction modifier rules (optional). */
	UFUNCTION(BlueprintCallable, Category = "Parley|Faction", meta = (ToolTip = "Runs this faction subsystem operation on authoritative runtime state."))
	void SetProgressionTags(const FGameplayTagContainer& Tags);

	UPROPERTY(BlueprintAssignable, Category = "Parley|Faction", meta = (ToolTip = "Broadcast when faction popularity is mutated by dialogue or scripted runtime. Save bridges should persist and mark dirty."))
	FParleyOnFactionPopularityChanged OnFactionPopularityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parley|Faction", meta = (ToolTip = "Broadcast when faction reputation for a speaker is mutated. Save bridges should persist and mark dirty."))
	FParleyOnFactionSpeakerReputationChanged OnFactionSpeakerReputationChanged;

private:
	FParleyFactionState* FindFactionPopularityStateMutable(FGameplayTag FactionTag);
	const FParleyFactionState* FindFactionPopularityState(FGameplayTag FactionTag) const;
	FParleyFactionSpeakerReputationState* FindSpeakerReputationStateMutable(FGameplayTag FactionTag, FGameplayTag SpeakerTag);
	const FParleyFactionSpeakerReputationState* FindSpeakerReputationState(FGameplayTag FactionTag, FGameplayTag SpeakerTag) const;
	bool BuildFactionTagList(TArray<FGameplayTag>& OutFactionTags, FString& OutError) const;
	bool ResolveFactionDefinition(const FGameplayTag& FactionTag, FParleyFactionDefinitionRow& OutRow, FString& OutError) const;
	float ComputeModifierDelta(const FParleyFactionDefinitionRow& Row, const FGameplayTagContainer& ProgressionTags) const;
	static float ClampPopularity(const FParleyFactionDefinitionRow& Row, float Value);
	static FGameplayTag BuildFactionTagFromRootAndLeaf(const FGameplayTag& RootTag, FName LeafRowName);

	UPROPERTY(Transient)
	TArray<FParleyFactionState> PersistedFactionPopularityStates;

	UPROPERTY(Transient)
	TArray<FParleyFactionSpeakerReputationState> PersistedFactionSpeakerReputationStates;

	UPROPERTY(Transient)
	FGameplayTagContainer GameProgressionTags;
};
