/**
 * @file ARNPCCharacterBase.h
 * @brief World speaker actor base for dialogue interactions.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "ARSpeakerComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "ARNPCCharacterBase.generated.h"

class AARPlayerController;
class UARCustomerComponent;
class UAREmotionComponent;

UCLASS(meta = (DisplayName = "Speaker Character", ToolTip = "World speaker actor base with dialogue talk, emotion display, and optional shop-customer behavior."))
class ALIENRAMEN_API AARNPCCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AARNPCCharacterBase();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
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

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue|Speaker")
	void RefreshTalkableFromSubsystem();

	void RefreshAutoWantsToTalkEmotion(bool bEffectiveTalkable);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Components", meta = (AllowPrivateAccess = "true", ToolTip = "Speaker-talk runtime component."))
	TObjectPtr<UARSpeakerComponent> SpeakerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Components", meta = (AllowPrivateAccess = "true", ToolTip = "Emotion display runtime component."))
	TObjectPtr<UAREmotionComponent> EmotionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Components", meta = (AllowPrivateAccess = "true", ToolTip = "Optional shop-customer runtime component."))
	TObjectPtr<UARCustomerComponent> CustomerComponent;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_SpeakerLocalStateAllowsDialogue,
		Category = "Alien Ramen|Speaker",
		meta = (DisplayName = "Speaker Local State Allows Dialogue", ToolTip = "Server-resolved local speaker gate (for example shop mode behavior windows)."))
	bool bSpeakerLocalStateAllowsDialogue = true;

	bool bAutoWantsToTalkEmotionApplied = false;
};
