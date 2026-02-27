#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "StateTreeEvents.h"
#include "ARInvaderTypes.h"
#include "AREnemyAIController.generated.h"

class UStateTree;
class UARStateTreeAIComponent;

UCLASS()
class ALIENRAMEN_API AAREnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAREnemyAIController();
	void NotifyWavePhaseChanged(int32 WaveInstanceId, EARWavePhase NewPhase);
	void NotifyEnemyEnteredScreen(int32 WaveInstanceId);
	void NotifyEnemyInFormation(int32 WaveInstanceId);
	void TryStartStateTreeForCurrentPawn();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|State", meta = (BlueprintAuthorityOnly))
	void PushPawnASCStateTag(FGameplayTag StateTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|State", meta = (BlueprintAuthorityOnly))
	void PopPawnASCStateTag(FGameplayTag StateTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|State", meta = (BlueprintAuthorityOnly))
	void PushPawnASCStateTags(const FGameplayTagContainer& StateTags);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|State", meta = (BlueprintAuthorityOnly))
	void PopPawnASCStateTags(const FGameplayTagContainer& StateTags);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Enemy|AI|State")
	UARStateTreeAIComponent* GetEnemyStateTreeComponent() const { return StateTreeComponent; }

	// Sends a fully-authored StateTree event to this controller's state tree (authority only).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|State", meta = (BlueprintAuthorityOnly))
	bool SendStateTreeEvent(const FStateTreeEvent& Event);

	// Convenience helper for tag-only events with optional origin.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|State", meta = (BlueprintAuthorityOnly))
	bool SendStateTreeEventByTag(FGameplayTag EventTag, FName Origin = NAME_None);

	// Pawn -> controller fact signal. Pawn reports observations; controller decides routing/behavior.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Enemy|AI|Signal", meta = (BlueprintAuthorityOnly))
	bool ReceivePawnSignal(
		FGameplayTag SignalTag,
		AActor* RelatedActor = nullptr,
		FVector WorldLocation = FVector::ZeroVector,
		float ScalarValue = 0.f,
		bool bForwardToStateTree = true);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Enemy|AI|Signal")
	void BP_OnPawnSignal(
		FGameplayTag SignalTag,
		AActor* RelatedActor,
		FVector WorldLocation,
		float ScalarValue);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void StartStateTreeForPawn(APawn* InPawn);
	void StartStateTreeForPawn_Deferred(APawn* InPawn);
	void StopStateTree(const FString& Reason);
	bool IsStateTreeRunning() const;
	void BindStateTreeTagBridge();
	void UnbindStateTreeTagBridge(bool bPopAppliedTags);
	void HandleStateTreeActiveTagsChanged(const FGameplayTagContainer& AddedTags, const FGameplayTagContainer& RemovedTags);

private:
	bool bPendingStateTreeStart = false;
	FTimerHandle DeferredStartTimerHandle;
	FGameplayTagContainer AppliedStateTreeTags;
	int32 DeferredStartAttemptCounter = 0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|AI")
	TObjectPtr<UARStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|AI")
	TObjectPtr<UStateTree> DefaultStateTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|AI|Events")
	FGameplayTag ActivePhaseEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|AI|Events")
	FGameplayTag BerserkPhaseEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|AI|Events")
	FGameplayTag EnteredScreenEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|AI|Events")
	FGameplayTag InFormationEventTag;

};
