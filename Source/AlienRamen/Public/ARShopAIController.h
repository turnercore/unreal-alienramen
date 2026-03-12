/**
 * @file ARShopAIController.h
 * @brief ARShopAIController header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARAIControllerBase.h"
#include "GameplayTagContainer.h"
#include "ARShopAIController.generated.h"

class UStateTree;
class UARShopStateTreeAIComponent;
class APawn;
class AARNPCCharacterBase;

UCLASS()
class ALIENRAMEN_API AARShopAIController : public AARAIControllerBase
{
	GENERATED_BODY()

public:
	AARShopAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer|StateTree")
	void TryStartStateTreeForCurrentPawn();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Shop|Customer|StateTree")
	bool SendShopStateTreeEventByTag(FGameplayTag EventTag, FName Origin = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer|StateTree")
	UARShopStateTreeAIComponent* GetShopStateTreeComponent() const { return StateTreeComponent; }

protected:
	void HandleShopActiveStateTagsChanged(const FGameplayTagContainer& AddedTags, const FGameplayTagContainer& RemovedTags);
	void RefreshSpeakerDialogueGateFromStateTags();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer|StateTree", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UARShopStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alien Ramen|Shop|Customer|StateTree", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTree> DefaultStateTree;
};
