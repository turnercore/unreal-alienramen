/**
 * @file ARNPCSubsystem.h
 * @brief Server-authoritative NPC talkable-state runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARNPCSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnNpcTalkableChanged, FGameplayTag, NpcTag, bool, bNewTalkable);

UCLASS()
class ALIENRAMEN_API UARNPCSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsNpcTalkable(FGameplayTag NpcTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	bool RefreshNpcTalkableState(FGameplayTag NpcTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void RefreshAllNpcTalkableStates();

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|NPC")
	FAROnNpcTalkableChanged OnNpcTalkableChanged;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, bool> NpcTalkableCache;
};
