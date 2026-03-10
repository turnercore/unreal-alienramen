/**
 * @file AREmotionComponent.h
 * @brief Replicated emotion display component with slot-aware state.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "AREmotionTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AREmotionComponent.generated.h"

class AARPlayerController;
class APlayerController;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnEmotionDisplayStateChanged);

UCLASS(ClassGroup=(AlienRamen), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ALIENRAMEN_API UAREmotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAREmotionComponent();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void SetEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void SetEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void ClearEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void ClearEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void ClearAllEmotionTags();

	// Dialogue override helpers (higher priority than base state).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void SetDialogueEmotionTag(FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void SetDialogueEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot, FGameplayTag NewEmotionTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void ClearDialogueEmotionTag();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void ClearDialogueEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void ClearAllDialogueEmotionTags();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Emotion")
	FGameplayTag GetDisplayedEmotionTagForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Emotion")
	FGameplayTag GetDisplayedEmotionTagForController(const AARPlayerController* ViewerController) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	bool TryResolveDisplayedEmotionIconForPlayerSlot(
		EARPlayerSlot PlayerSlot,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	bool TryResolveDisplayedEmotionIconForController(
		const AARPlayerController* ViewerController,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	bool TryResolveEmotionIconForTag(
		FGameplayTag EmotionTag,
		TSoftObjectPtr<UTexture2D>& OutIconTexture,
		FGameplayTag& OutResolvedEmotionTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Emotion")
	FVector GetEmotionAnchorWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	bool GetEmotionFacingRotationForController(const APlayerController* ViewerController, FRotator& OutFacingRotation) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Emotion")
	void SetRegisteredSpeakerTag(FGameplayTag NewSpeakerTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Emotion")
	FGameplayTag GetRegisteredSpeakerTag() const { return RegisteredSpeakerTag; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Emotion")
	FAROnEmotionDisplayStateChanged OnEmotionDisplayStateChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_BaseEmotionState(FAREmotionDisplayState OldState);

	UFUNCTION()
	void OnRep_DialogueOverrideState(FAREmotionDisplayState OldState);

private:
	static FGameplayTag GetStateSlotTag(const FAREmotionDisplayState& State, EARPlayerSlot PlayerSlot);
	static void SetStateSlotTag(FAREmotionDisplayState& State, EARPlayerSlot PlayerSlot, const FGameplayTag& EmotionTag);
	static bool AreDisplayStatesEqual(const FAREmotionDisplayState& Left, const FAREmotionDisplayState& Right);

	bool IsAuthorityOwner() const;
	void ForceOwnerNetUpdate() const;
	void BuildEmotionLookupCandidates(const FGameplayTag& RequestedTag, TArray<FGameplayTag>& OutCandidates) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Alien Ramen|Emotion", meta = (AllowPrivateAccess = "true", Categories = "Dialogue.Speaker"))
	FGameplayTag RegisteredSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Emotion", meta = (AllowPrivateAccess = "true"))
	FName AnchorSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Emotion", meta = (AllowPrivateAccess = "true"))
	FVector AnchorWorldOffset = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(ReplicatedUsing = OnRep_BaseEmotionState, BlueprintReadOnly, Category = "Alien Ramen|Emotion", meta = (AllowPrivateAccess = "true"))
	FAREmotionDisplayState BaseEmotionState;

	UPROPERTY(ReplicatedUsing = OnRep_DialogueOverrideState, BlueprintReadOnly, Category = "Alien Ramen|Emotion", meta = (AllowPrivateAccess = "true"))
	FAREmotionDisplayState DialogueOverrideState;
};
