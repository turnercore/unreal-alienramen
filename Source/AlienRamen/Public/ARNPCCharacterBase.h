/**
 * @file ARNPCCharacterBase.h
 * @brief World NPC actor base for dialogue interactions.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "ARNPCTalkComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "ARNPCCharacterBase.generated.h"

class AARPlayerController;
class UARCustomerComponent;
class UAREmotionComponent;

UCLASS()
class ALIENRAMEN_API AARNPCCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AARNPCCharacterBase();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void InteractByController(AARPlayerController* InteractingController);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	FGameplayTag GetNpcTag() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkable() const;

	// Per-slot talkable state. Use this for per-player local interaction indicators.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkableForPlayerSlot(EARPlayerSlot PlayerSlot) const;

	// Convenience per-controller query for per-player local interaction indicators.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsTalkableForController(const AARPlayerController* QueryController) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	bool IsNpcLocalStateAllowingDialogue() const;

	// Server-authoritative local state gate (for example ordering mode) applied on top of global dialogue availability.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Alien Ramen|NPC")
	void SetNpcLocalStateAllowsDialogue(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|NPC")
	FAROnNpcTalkableStateChanged OnNpcTalkableStateChanged;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	UARNPCTalkComponent* GetNpcTalkComponent() const { return NpcTalkComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	UAREmotionComponent* GetEmotionComponent() const { return EmotionComponent; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|NPC")
	UARCustomerComponent* GetCustomerComponent() const { return CustomerComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void HandleTalkComponentTalkableStateChanged(bool bNewTalkable);

	UFUNCTION()
	void OnRep_NpcLocalStateAllowsDialogue(bool bOldAllowsDialogue);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void RefreshTalkableFromSubsystem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UARNPCTalkComponent> NpcTalkComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAREmotionComponent> EmotionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UARCustomerComponent> CustomerComponent;

	// Legacy serialized field kept for migration from actor-authored talk data to component-authored data.
	UPROPERTY()
	FGameplayTag NpcTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_NpcLocalStateAllowsDialogue, Category = "Alien Ramen|NPC")
	bool bNpcLocalStateAllowsDialogue = true;
};
