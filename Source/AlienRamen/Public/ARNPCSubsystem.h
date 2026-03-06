/**
 * @file ARNPCSubsystem.h
 * @brief Server-authoritative NPC relationship/want runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARDialogueTypes.h"
#include "ARNPCSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnNpcTalkableChanged, FGameplayTag, NpcTag, bool, bNewTalkable);

UCLASS()
class ALIENRAMEN_API UARNPCSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	bool SubmitNpcRamenDelivery(FGameplayTag NpcTag, FGameplayTag DeliveredRamenTag, bool& bOutAccepted);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool TryGetNpcRelationshipState(FGameplayTag NpcTag, FARNpcRelationshipState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsNpcTalkable(FGameplayTag NpcTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	bool RefreshNpcTalkableState(FGameplayTag NpcTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void RefreshAllNpcTalkableStates();

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|NPC")
	FAROnNpcTalkableChanged OnNpcTalkableChanged;
};
