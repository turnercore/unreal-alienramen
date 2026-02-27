#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/StateTreeAIComponent.h"
#include "ARStateTreeAIComponent.generated.h"

class UStateTreeSchema;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FAROnActiveStateTagsChangedNative,
	const FGameplayTagContainer& /*AddedTags*/,
	const FGameplayTagContainer& /*RemovedTags*/);

/**
 * Enemy-focused StateTree AI component that tracks active state tags at runtime
 * and emits add/remove deltas whenever the active state-tag set changes.
 */
UCLASS(ClassGroup = AI, Blueprintable, meta = (BlueprintSpawnableComponent))
class ALIENRAMEN_API UARStateTreeAIComponent : public UStateTreeAIComponent
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UStateTreeSchema> GetSchema() const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void StartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	virtual void Cleanup() override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Enemy|AI|State")
	FGameplayTagContainer GetCurrentActiveStateTags() const { return CurrentActiveStateTags; }

	FAROnActiveStateTagsChangedNative OnActiveStateTagsChanged;

private:
	void RefreshActiveStateTags();
	void EmitTagDelta(const FGameplayTagContainer& NewTags);

private:
	UPROPERTY(Transient)
	FGameplayTagContainer CurrentActiveStateTags;
};
