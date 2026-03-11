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

UCLASS(
	ClassGroup=(AlienRamen),
	BlueprintType,
	Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Dialogue Speaker Talk Component", ToolTip="Server-authoritative talkability and interaction surface for a dialogue speaker."))
class ALIENRAMEN_API UARNPCTalkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARNPCTalkComponent();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	void InteractByController(AARPlayerController* InteractingController);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker", meta = (DisplayName = "Get Speaker Tag"))
	FGameplayTag GetNpcTag() const { return NpcTag; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker", meta = (DisplayName = "Set Speaker Tag"))
	void SetNpcTag(FGameplayTag NewNpcTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsTalkable() const { return bIsTalkable; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsTalkableForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsTalkableForController(const AARPlayerController* QueryController) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	void RefreshTalkableFromSubsystem();

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue|Speaker")
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

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Alien Ramen|Dialogue|Speaker",
		meta = (AllowPrivateAccess = "true", Categories = "Dialogue.Speaker", DisplayName = "Speaker Tag", ToolTip = "Primary speaker identity tag used for dialogue lookup and speaker-bound emotion routing."))
	FGameplayTag NpcTag;

	UPROPERTY(ReplicatedUsing = OnRep_IsTalkable, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Speaker", meta = (AllowPrivateAccess = "true", DisplayName = "Speaker Is Talkable", ToolTip = "Resolved global talkability for this speaker from the dialogue runtime."))
	bool bIsTalkable = false;

	// Bitmask of talkable slots (P1=bit0, P2=bit1).
	UPROPERTY(ReplicatedUsing = OnRep_TalkablePlayerSlotMask, BlueprintReadOnly, Category = "Alien Ramen|Dialogue|Speaker", meta = (AllowPrivateAccess = "true", DisplayName = "Talkable Player Slot Mask", ToolTip = "Per-player talkability mask (P1 bit 0, P2 bit 1)."))
	uint8 TalkablePlayerSlotMask = 0;
};
