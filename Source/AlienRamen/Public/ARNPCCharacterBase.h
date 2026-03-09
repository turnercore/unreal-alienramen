/**
 * @file ARNPCCharacterBase.h
 * @brief World NPC actor base for dialogue interactions.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "ARNPCCharacterBase.generated.h"

class AARPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnNpcTalkableStateChanged, bool, bNewTalkable);

UCLASS()
class ALIENRAMEN_API AARNPCCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AARNPCCharacterBase();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void InteractByController(AARPlayerController* InteractingController);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	FGameplayTag GetNpcTag() const { return NpcTag; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkable() const { return bIsTalkable; }

	// Per-slot talkable state. Use this for per-player local interaction indicators.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkableForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	// Convenience per-controller query for per-player local interaction indicators.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkableForController(const AARPlayerController* QueryController) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsNpcLocalStateAllowingDialogue() const { return bNpcLocalStateAllowsDialogue; }

	// Server-authoritative local state gate (for example ordering mode) applied on top of global dialogue availability.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void SetNpcLocalStateAllowsDialogue(bool bEnabled);

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

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void RefreshTalkableFromSubsystem();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC")
	FGameplayTag NpcTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC")
	bool bNpcLocalStateAllowsDialogue = true;

	UPROPERTY(ReplicatedUsing=OnRep_IsTalkable, BlueprintReadOnly, Category = "Alien Ramen|NPC")
	bool bIsTalkable = false;

	// Bitmask of talkable slots (P1=bit0, P2=bit1).
	UPROPERTY(ReplicatedUsing=OnRep_TalkablePlayerSlotMask, BlueprintReadOnly, Category = "Alien Ramen|NPC")
	uint8 TalkablePlayerSlotMask = 0;
};
