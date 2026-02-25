# AAREnemyAIController
Path: `Source/AlienRamen/Public/AREnemyAIController.h`, `.../Private/AREnemyAIController.cpp`

## Purpose
- Enemy AI controller that owns `UStateTreeAIComponent`.
- Bridges invader runtime events into StateTree event tags.

## Automatic Lifecycle
- `OnPossess`: defers StateTree start by one tick, then starts logic.
- `OnUnPossess`: stops logic and clears focus.

## Public API
- `NotifyWavePhaseChanged(int32 WaveInstanceId, EARWavePhase NewPhase)`
- `NotifyEnemyEnteredScreen(int32 WaveInstanceId)`
- `NotifyEnemyInFormation(int32 WaveInstanceId)`

These are called from `AAREnemyBase` when runtime context changes.

## Config Variables
- `StateTreeComponent` (`UPROPERTY`, BP read-only)
- `DefaultStateTree` (`UPROPERTY`, BP read-only)
- Event tags (`UPROPERTY`, BP read-only):
- `ActivePhaseEventTag` (default `Event.Wave.Phase.Active`)
- `BerserkPhaseEventTag` (default `Event.Wave.Phase.Berserk`)
- `EnteredScreenEventTag` (default `Event.Enemy.EnteredScreen`)
- `InFormationEventTag` (default `Event.Enemy.InFormation`)

## Blueprint Exposure
- No custom `UFUNCTION` Blueprint API on this class currently.
- Properties are BP-readable.

