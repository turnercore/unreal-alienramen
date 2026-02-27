#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "ARInvaderTypes.h"
#include "AREnemyAIController.generated.h"

class UStateTree;
class UStateTreeAIComponent;

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

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void StartStateTreeForPawn(APawn* InPawn);
	void StartStateTreeForPawn_Deferred(APawn* InPawn);
	void StopStateTree(const FString& Reason);
	bool IsStateTreeRunning() const;

private:
	bool bPendingStateTreeStart = false;
	FTimerHandle DeferredStartTimerHandle;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

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
