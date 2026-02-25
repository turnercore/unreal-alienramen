# UARInvaderDirectorSettings
Path: `Source/AlienRamen/Public/ARInvaderDirectorSettings.h`

## Purpose
- Project settings for runtime invader director behavior.
- Loaded with `GetDefault<UARInvaderDirectorSettings>()`.

## Key Config Groups
- Data:
- `WaveDataTable`, `StageDataTable`, `InitialStageRow`
- Run:
- `BaseThreatGainPerSecond`
- `NewWaveDelayAfterClear`
- `NewWaveDelayWhenOvertime`
- `StageTransitionDelay`
- `StageChoiceAutoSelectSeconds`
- `bStageChoiceAutoSelectLeft`
- `LeakLossThreshold`
- Spawn:
- `SpawnOrigin`
- `SpawnOffscreenDistance`
- `SpawnLaneSpacing`
- `SpawnFacingYawOffset`
- Bounds:
- `GameplayBoundsMin`, `GameplayBoundsMax`
- `OffscreenCullSeconds`
- `EnteredScreenInset`
- Soft caps:
- `SoftCapAliveEnemies`
- `SoftCapActiveProjectiles`
- `bBlockSpawnsWhenEnemySoftCapExceeded`
- Telemetry:
- `ProjectileActorClass`

## Blueprint Exposure
- All settings fields are `BlueprintReadOnly` for query/access patterns.

