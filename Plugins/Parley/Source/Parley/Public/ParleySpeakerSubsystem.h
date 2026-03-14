/**
 * @file ParleySpeakerSubsystem.h
 * @brief Server-authoritative speaker talkable-state runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ParleySpeakerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FParleyOnSpeakerTalkableChanged, FGameplayTag, SpeakerTag, bool, bNewTalkable);

UCLASS()
class PARLEY_API UParleySpeakerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True when the speaker is currently talkable (resolved from dialogue runtime). */
	UFUNCTION(BlueprintPure, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Returns speaker runtime state without mutation."))
	bool IsSpeakerTalkable(FGameplayTag SpeakerTag) const;

	/** Recompute talkable state for a single speaker tag (call after unlocks/state changes). */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Executes a speaker component or subsystem operation."))
	bool RefreshSpeakerTalkableState(FGameplayTag SpeakerTag);

	/** Recompute talkable state for all registered speakers. */
	UFUNCTION(BlueprintCallable, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Executes a speaker component or subsystem operation."))
	void RefreshAllSpeakerTalkableStates();

	UPROPERTY(BlueprintAssignable, Category = "Parley|Dialogue|Speaker", meta = (ToolTip = "Broadcast when cached talkable state changes for a speaker tag."))
	FParleyOnSpeakerTalkableChanged OnSpeakerTalkableChanged;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, bool> SpeakerTalkableCache;
};
