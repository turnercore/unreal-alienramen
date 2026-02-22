#include "ARInvaderRuntimeStateComponent.h"
#include "ARLog.h"

#include "Net/UnrealNetwork.h"

UARInvaderRuntimeStateComponent::UARInvaderRuntimeStateComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UARInvaderRuntimeStateComponent::SetRuntimeSnapshot(const FARInvaderRuntimeSnapshot& InSnapshot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const FARInvaderRuntimeSnapshot Previous = RuntimeSnapshot;
	RuntimeSnapshot = InSnapshot;
	BroadcastSnapshotDelta(Previous, RuntimeSnapshot);
}

void UARInvaderRuntimeStateComponent::OnRep_RuntimeSnapshot(const FARInvaderRuntimeSnapshot& PreviousSnapshot)
{
	BroadcastSnapshotDelta(PreviousSnapshot, RuntimeSnapshot);
}

void UARInvaderRuntimeStateComponent::BroadcastSnapshotDelta(
	const FARInvaderRuntimeSnapshot& OldSnapshot,
	const FARInvaderRuntimeSnapshot& NewSnapshot)
{
	if (OldSnapshot.StageRowName != NewSnapshot.StageRowName)
	{
		OnStageChanged.Broadcast(NewSnapshot.StageRowName);
	}

	TMap<int32, EARWavePhase> OldPhasesByWaveId;
	OldPhasesByWaveId.Reserve(OldSnapshot.ActiveWaves.Num());
	for (const FARWaveInstanceState& OldWave : OldSnapshot.ActiveWaves)
	{
		OldPhasesByWaveId.Add(OldWave.WaveInstanceId, OldWave.Phase);
	}

	for (const FARWaveInstanceState& NewWave : NewSnapshot.ActiveWaves)
	{
		const EARWavePhase* OldPhase = OldPhasesByWaveId.Find(NewWave.WaveInstanceId);
		if (!OldPhase || *OldPhase != NewWave.Phase)
		{
			OnWavePhaseChanged.Broadcast(NewWave.WaveInstanceId, NewWave.Phase);
		}
	}
}

void UARInvaderRuntimeStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UARInvaderRuntimeStateComponent, RuntimeSnapshot);
}

