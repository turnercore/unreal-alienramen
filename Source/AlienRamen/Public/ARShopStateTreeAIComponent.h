/**
 * @file ARShopStateTreeAIComponent.h
 * @brief Shop customer/speaker StateTree component exposing active runtime state tags.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/StateTreeAIComponent.h"
#include "ARShopStateTreeAIComponent.generated.h"

class UStateTreeSchema;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FAROnShopActiveStateTagsChangedNative,
	const FGameplayTagContainer& /*AddedTags*/,
	const FGameplayTagContainer& /*RemovedTags*/);

UCLASS(ClassGroup = AI, Blueprintable, meta = (BlueprintSpawnableComponent))
class ALIENRAMEN_API UARShopStateTreeAIComponent : public UStateTreeAIComponent
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UStateTreeSchema> GetSchema() const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void StartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	virtual void Cleanup() override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Shop|Customer|StateTree")
	FGameplayTagContainer GetCurrentActiveStateTags() const { return CurrentActiveStateTags; }

	FAROnShopActiveStateTagsChangedNative OnActiveStateTagsChanged;

private:
	void RefreshActiveStateTags();
	void EmitTagDelta(const FGameplayTagContainer& NewTags);

	UPROPERTY(Transient)
	FGameplayTagContainer CurrentActiveStateTags;
};
