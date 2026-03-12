/**
 * @file ARTransitionGameState.h
 * @brief Transition-map GameState read model and replicated context payload.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARGameStateBase.h"
#include "ARTransitionTypes.h"
#include "ARTransitionGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnTransitionContextChangedSignature, const FARTransitionContext&, Context);

UCLASS()
class ALIENRAMEN_API AARTransitionGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARTransitionGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	const FARTransitionContext& GetTransitionContext() const { return TransitionContext; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Transition", meta = (BlueprintAuthorityOnly))
	void SetTransitionContext(const FARTransitionContext& NewContext);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Transition")
	bool HasValidTransitionDestination() const { return !TransitionContext.DestinationURL.IsEmpty(); }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Transition")
	FAROnTransitionContextChangedSignature OnTransitionContextChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_TransitionContext(const FARTransitionContext& OldContext);

private:
	UPROPERTY(ReplicatedUsing = OnRep_TransitionContext, BlueprintReadOnly, Category = "Alien Ramen|Transition", meta = (AllowPrivateAccess = "true"))
	FARTransitionContext TransitionContext;
};

