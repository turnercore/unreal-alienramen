/**
 * @file ARNPCTalkComponent.h
 * @brief Reusable NPC dialogue/talkability component.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARNPCTalkComponent.generated.h"

class AARPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnNpcTalkableStateChanged, bool, bNewTalkable);

UCLASS(ClassGroup=(AlienRamen), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ALIENRAMEN_API UARNPCTalkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARNPCTalkComponent();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void InteractByController(AARPlayerController* InteractingController);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	FGameplayTag GetNpcTag() const { return NpcTag; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void SetNpcTag(FGameplayTag NewNpcTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkable() const { return bIsTalkable; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkableForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkableForController(const AARPlayerController* QueryController) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void RefreshTalkableFromSubsystem();

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|NPC")
	FAROnNpcTalkableStateChanged OnNpcTalkableStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsTalkable(bool bOldTalkable);

	UFUNCTION()
	void OnRep_TalkablePlayerSlotMask(uint8 bOldTalkablePlayerSlotMask);

	UFUNCTION()
	void HandleNpcTalkableChanged(FGameplayTag ChangedNpcTag, bool bNewTalkable);

private:
	bool IsAuthorityOwner() const;
	void ForceOwnerNetUpdate() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC", meta = (AllowPrivateAccess = "true"))
	FGameplayTag NpcTag;

	UPROPERTY(ReplicatedUsing = OnRep_IsTalkable, BlueprintReadOnly, Category = "Alien Ramen|NPC", meta = (AllowPrivateAccess = "true"))
	bool bIsTalkable = false;

	// Bitmask of talkable slots (P1=bit0, P2=bit1).
	UPROPERTY(ReplicatedUsing = OnRep_TalkablePlayerSlotMask, BlueprintReadOnly, Category = "Alien Ramen|NPC", meta = (AllowPrivateAccess = "true"))
	uint8 TalkablePlayerSlotMask = 0;
};
