/**
 * @file ARNPCCharacterBase.h
 * @brief World speaker actor base for dialogue interactions.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "ARShopRamenTypes.h"
#include "ARSpeakerComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "ARNPCCharacterBase.generated.h"

class AARPlayerController;
class AActor;
class UARCustomerComponent;
class UAREmotionComponent;

UCLASS(meta = (DisplayName = "NPC Character", ToolTip = "Lean NPC character shell. Speaker, emotion, and customer behavior are provided by optional components."))
class ALIENRAMEN_API AARNPCCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AARNPCCharacterBase();

	// Optional forwarding helper for BI_Interactable-style calls. Accepts controller or pawn and routes through controller RPCs.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Forward Use To Controller"))
	void ForwardUseToController(AActor* UsingActor);

	// Optional convenience interaction path. If a customer order can be served, it serves first; otherwise it falls back to speaker dialogue.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Interaction", meta = (DisplayName = "Interact (Auto)"))
	void InteractByController(AARPlayerController* InteractingController);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker", meta = (DisplayName = "Get Speaker Tag"))
	FGameplayTag GetSpeakerTag() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsTalkable() const;

	// Per-slot talkable state. Use this for per-player local interaction indicators.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsTalkableForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	// Convenience per-controller query for per-player local interaction indicators.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsTalkableForController(const AARPlayerController* QueryController) const;

	// Returns true when another player currently owns an active per-player dialogue session for this speaker.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	bool IsSpeakerBusyForController(const AARPlayerController* QueryController) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker", meta = (DisplayName = "Is Speaker Local State Allowing Dialogue"))
	bool IsSpeakerLocalStateAllowingDialogue() const;

	// Server-authoritative local state gate (for example ordering mode) applied on top of global dialogue availability.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Alien Ramen|Dialogue|Speaker", meta = (DisplayName = "Set Speaker Local State Allows Dialogue"))
	void SetSpeakerLocalStateAllowsDialogue(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Speaker")
	FAROnSpeakerTalkableStateChanged OnSpeakerTalkableStateChanged;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Speaker")
	UARSpeakerComponent* GetSpeakerComponent() const { return SpeakerComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue|Emotion")
	UAREmotionComponent* GetEmotionComponent() const { return EmotionComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer")
	UARCustomerComponent* GetCustomerComponent() const { return CustomerComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void HandleSpeakerComponentTalkableStateChanged(bool bNewTalkable);

	UFUNCTION()
	void OnRep_SpeakerLocalStateAllowsDialogue(bool bOldAllowsDialogue);

	UFUNCTION()
	void HandleCustomerOrderChanged(const FARRamenOrderRequest& NewOrder);

	UFUNCTION()
	void HandleCustomerOrderResolved(const FARRamenServeResult& ServeResult);

	UFUNCTION()
	void HandleCustomerDoneOrdering(int32 OrdersGeneratedCount, int32 OrdersServedCount, int32 RemainingOrdersToGenerate);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	void RefreshTalkableFromSubsystem();

	void RefreshAutoWantsToTalkEmotion(bool bEffectiveTalkable);
	void RefreshStateTreeInteractionFlags();
	void ResolveOptionalComponents();
	AARPlayerController* ResolveUsingController(AActor* UsingActor) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Components", meta = (AllowPrivateAccess = "true", ToolTip = "Cached canonical speaker component (optional)."))
	TObjectPtr<UARSpeakerComponent> SpeakerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Components", meta = (AllowPrivateAccess = "true", ToolTip = "Cached canonical emotion component (optional)."))
	TObjectPtr<UAREmotionComponent> EmotionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Components", meta = (AllowPrivateAccess = "true", ToolTip = "Cached canonical customer component (optional)."))
	TObjectPtr<UARCustomerComponent> CustomerComponent;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_SpeakerLocalStateAllowsDialogue,
		Category = "Alien Ramen|Speaker",
		meta = (DisplayName = "Speaker Local State Allows Dialogue", ToolTip = "Server-resolved local speaker gate (for example shop mode behavior windows)."))
	bool bSpeakerLocalStateAllowsDialogue = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Alien Ramen|StateTree", meta = (AllowPrivateAccess = "true", DisplayName = "ST Has Active Order", ToolTip = "Cached StateTree gate. True when owner currently has an active customer order."))
	bool bST_HasActiveOrder = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Alien Ramen|StateTree", meta = (AllowPrivateAccess = "true", DisplayName = "ST Has Dialogue To Say", ToolTip = "Cached StateTree gate. True when speaker dialogue is currently available."))
	bool bST_HasDialogueToSay = false;

	bool bAutoWantsToTalkEmotionApplied = false;
};
