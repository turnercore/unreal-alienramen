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

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsSpeakerTalkable(FGameplayTag SpeakerTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	bool RefreshSpeakerTalkableState(FGameplayTag SpeakerTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	void RefreshAllSpeakerTalkableStates();

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Speaker")
	FAROnSpeakerTalkableChanged OnSpeakerTalkableChanged;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, bool> SpeakerTalkableCache;
};
