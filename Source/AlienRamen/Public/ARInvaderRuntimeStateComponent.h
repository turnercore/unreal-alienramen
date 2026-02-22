#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARInvaderTypes.h"
#include "ARInvaderRuntimeStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnWavePhaseChangedSignature, int32, WaveInstanceId, EARWavePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnStageChangedSignature, FName, NewStageRowName);

UCLASS(ClassGroup=(AR), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ALIENRAMEN_API UARInvaderRuntimeStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARInvaderRuntimeStateComponent();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	const FARInvaderRuntimeSnapshot& GetRuntimeSnapshot() const { return RuntimeSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	void SetRuntimeSnapshot(const FARInvaderRuntimeSnapshot& InSnapshot);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnWavePhaseChangedSignature OnWavePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnStageChangedSignature OnStageChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_RuntimeSnapshot, BlueprintReadOnly, Category = "Alien Ramen|Invader")
	FARInvaderRuntimeSnapshot RuntimeSnapshot;

	UFUNCTION()
	void OnRep_RuntimeSnapshot(const FARInvaderRuntimeSnapshot& PreviousSnapshot);

	void BroadcastSnapshotDelta(const FARInvaderRuntimeSnapshot& OldSnapshot, const FARInvaderRuntimeSnapshot& NewSnapshot);
};

