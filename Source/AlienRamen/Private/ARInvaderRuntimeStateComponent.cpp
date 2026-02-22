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

	const bool bOldInChoice = OldSnapshot.FlowState == EARInvaderFlowState::StageChoice;
	const bool bNewInChoice = NewSnapshot.FlowState == EARInvaderFlowState::StageChoice;
	if (bOldInChoice != bNewInChoice
		|| OldSnapshot.StageChoiceLeftRowName != NewSnapshot.StageChoiceLeftRowName
		|| OldSnapshot.StageChoiceRightRowName != NewSnapshot.StageChoiceRightRowName)
	{
		OnStageChoiceChanged.Broadcast(bNewInChoice, NewSnapshot.StageChoiceLeftRowName, NewSnapshot.StageChoiceRightRowName);
	}

	if (NewSnapshot.RewardEventId > OldSnapshot.RewardEventId)
	{
		OnStageRewardGranted.Broadcast(NewSnapshot.LastRewardStageRowName, NewSnapshot.LastRewardDescriptor);
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
