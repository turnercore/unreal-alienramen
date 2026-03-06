/**
 * @file ARNPCCharacterBase.h
 * @brief World NPC actor base for dialogue interactions.
 */
#pragma once

#include "CoreMinimal.h"
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

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|NPC")
	FAROnNpcTalkableStateChanged OnNpcTalkableStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsTalkable(bool bOldTalkable);

	UFUNCTION()
	void HandleNpcTalkableChanged(FGameplayTag ChangedNpcTag, bool bNewTalkable);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|NPC")
	void RefreshTalkableFromSubsystem();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|NPC")
	FGameplayTag NpcTag;

	UPROPERTY(ReplicatedUsing=OnRep_IsTalkable, BlueprintReadOnly, Category = "Alien Ramen|NPC")
	bool bIsTalkable = false;
};
