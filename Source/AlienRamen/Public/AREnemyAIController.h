#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AREnemyAIController.generated.h"

class UStateTree;
class UStateTreeAIComponent;

UCLASS()
class ALIENRAMEN_API AAREnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAREnemyAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	void StartStateTreeForPawn(APawn* InPawn);
	void StopStateTree(const FString& Reason);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AR|Enemy|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AR|Enemy|AI")
	TObjectPtr<UStateTree> DefaultStateTree;
};

