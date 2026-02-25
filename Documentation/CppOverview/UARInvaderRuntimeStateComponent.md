# UARInvaderRuntimeStateComponent
Path: `Source/AlienRamen/Public/ARInvaderRuntimeStateComponent.h`, `.../Private/ARInvaderRuntimeStateComponent.cpp`

## Purpose
- Replicated read model for UI/gameplay observers.
- Attached to `GameState` and updated by director.

## Automatic/Replication Behavior
- `RuntimeSnapshot` replicated with `OnRep_RuntimeSnapshot`.
- `LeakCount` replicated with `OnRep_LeakCount`.
- Delegates broadcast both on:
- server update path (`SetRuntimeSnapshot`)
- client rep-notify path (`OnRep_*`)

## Blueprint API
- `GetRuntimeSnapshot()` (`BP Pure`)
- `SetRuntimeSnapshot(...)` (`BP Callable`, authority-only)
- `GetLeakCount()` (`BP Pure`)

## Blueprint Assignable Delegates
- `OnWavePhaseChanged(WaveInstanceId, NewPhase)`
- `OnStageChanged(NewStageRowName)`
- `OnStageChoiceChanged(bInStageChoice, LeftStageRowName, RightStageRowName)`
- `OnStageRewardGranted(StageRowName, RewardDescriptor)`
- `OnEnemyLeaked(NewLeakCount, Delta)`

## Key Variables
- `RuntimeSnapshot` (replicated)
- `LeakCount` (replicated)

