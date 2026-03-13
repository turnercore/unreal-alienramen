/**
 * @file ARSpeakerSubsystem.h
 * @brief Server-authoritative speaker talkable-state runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARSpeakerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnSpeakerTalkableChanged, FGameplayTag, SpeakerTag, bool, bNewTalkable);

UCLASS()
class ALIENRAMEN_API UARSpeakerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True when the speaker is currently talkable (resolved from dialogue runtime). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsSpeakerTalkable(FGameplayTag SpeakerTag) const;

	/** Recompute talkable state for a single speaker tag (call after unlocks/state changes). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	bool RefreshSpeakerTalkableState(FGameplayTag SpeakerTag);

	/** Recompute talkable state for all registered speakers. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	void RefreshAllSpeakerTalkableStates();

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Speaker")
	FAROnSpeakerTalkableChanged OnSpeakerTalkableChanged;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, bool> SpeakerTalkableCache;
};
