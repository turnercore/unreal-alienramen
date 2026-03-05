#pragma once
/**
 * @file ARAIControllerBase.h
 * @brief ARAIControllerBase header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "ARAIControllerBase.generated.h"

UCLASS()
class ALIENRAMEN_API AARAIControllerBase : public AAIController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|AI|Signal", meta = (BlueprintAuthorityOnly))
	virtual bool ReceivePawnSignal(
		FGameplayTag SignalTag,
		AActor* RelatedActor = nullptr,
		FVector WorldLocation = FVector::ZeroVector,
		float ScalarValue = 0.f,
		bool bForwardToStateTree = true);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|AI|Signal")
	void BP_OnPawnSignal(
		FGameplayTag SignalTag,
		AActor* RelatedActor,
		FVector WorldLocation,
		float ScalarValue);
};
