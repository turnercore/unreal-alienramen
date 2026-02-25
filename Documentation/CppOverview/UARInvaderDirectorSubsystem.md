# UARInvaderDirectorSubsystem
Path: `Source/AlienRamen/Public/ARInvaderDirectorSubsystem.h`, `.../Private/ARInvaderDirectorSubsystem.cpp`

## Purpose
- Server-authoritative invader runtime brain (`UTickableWorldSubsystem`).
- Selects stages/waves, spawns enemies, drives wave phases, evaluates loss.
- Writes replicated read-model snapshot to `UARInvaderRuntimeStateComponent`.

## Automatic Lifecycle
- `Initialize`: registers console commands.
- `Deinitialize`: unregisters console commands.
- `Tick`: runs only in game world + authority game mode + `bRunActive`.

## Blueprint API
- `StartInvaderRun(int32 Seed)` (`BP Callable`, authority-only)
- `StopInvaderRun()` (`BP Callable`, authority-only)
- `IsRunActive()` (`BP Pure`)
- `ForceWaveByRow(FName)` (`BP Callable`, authority-only)
- `ForceWavePhase(int32, EARWavePhase)` (`BP Callable`, authority-only)
- `ForceThreat(float)` (`BP Callable`, authority-only)
- `ForceStage(FName)` (`BP Callable`, authority-only)
- `SubmitStageChoice(bool bChooseLeft)` (`BP Callable`, authority-only)
- `GetFlowState()` (`BP Pure`)
- `DumpRuntimeState()` (`BP Callable`)
- `ReportEnemyLeaked(AAREnemyBase* Enemy)` (`BP Callable`, deduped)

## High-use Internal Functions
- Selection/spawn:
- `SelectStage`, `SelectWave`, `SpawnWaveFromDefinition`
- `ComputeFormationTargetLocation`, `ComputeSpawnLocation`
- `ApplyEnemyGameplayEffects`
- Runtime progression:
- `UpdateWaves`, `SpawnWavesIfNeeded`, `TransitionWavePhase`
- `UpdateStage`, `EnterAwaitStageClear`, `EnterStageChoice`, `EnterTransition`
- Safety/telemetry:
- `RecountAliveAndHandleLeaks`, `EvaluateLossConditions`, `AreAllPlayersDown`
- Snapshot:
- `PushSnapshotToGameState`, `GetOrCreateRuntimeComponent`

## Key State Variables (current)
- Run state: `bRunActive`, `FlowState`, `RunSeed`, `RunElapsed`, `StageElapsed`, `Threat`
- Wave cadence: `TimeSinceLastWaveSpawn`, `NextWaveInstanceId`
- Loss/reward: `LeakCount`, `StageSequence`, `RewardEventId`
- Current stage/choice/transition rows + defs
- Runtime sets/maps:
- `ActiveWaves`
- `OneTimeWaveRowsUsed`
- `ReportedLeakedEnemies` (dedupe for `ReportEnemyLeaked`)
- `OffscreenDurationByEnemy`
- Data assets: `WaveTable`, `StageTable`

## Behavior Notes
- Start is explicit; subsystem does not auto-run without `StartInvaderRun`.
- Stage intro timing was removed from director flow.
- Disabled rows (`bEnabled=false`) are skipped during stage/wave selection.
- Director sets per-enemy formation target location before StateTree starts.
- Director no longer force-destroys enemies for leak boundary polling; leak reporting is enemy-driven via `ReportEnemyLeaked`.

