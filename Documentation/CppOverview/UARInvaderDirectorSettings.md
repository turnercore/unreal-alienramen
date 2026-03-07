# UARInvaderDirectorSettings
Path: `Source/AlienRamen/Public/ARInvaderDirectorSettings.h`

## Purpose
- Project settings for runtime invader director behavior.
- Loaded with `GetDefault<UARInvaderDirectorSettings>()`.

## Key Config Groups
- Data:
- `WaveDefinitionRootTag`, `StageDefinitionRootTag`, `InitialStageRow`
- Run:
- `BaseThreatGainPerSecond`
- `NewWaveDelayAfterClear`
- `NewWaveDelayWhenOvertime`
- `StageTransitionDelay`
- `StageChoiceAutoSelectSeconds`
- `bStageChoiceAutoSelectLeft`
- Spawn:
- `SpawnOrigin`
- `SpawnOffscreenDistance`
- `SpawnLaneSpacing`
- `SpawnFacingYawOffset`
- Bounds:
- `GameplayBoundsMin`, `GameplayBoundsMax`
- `OffscreenCullSeconds`
- `EnteredScreenInset`
- `ProjectileOffscreenCullSeconds`, `PickupOffscreenCullSeconds`
- Drops:
- `DefaultEnemyDropChance` (enemy `DropChance` baseline)
- `DefaultEnemyScrapDropChance`, `DefaultEnemyMeatDropChance` (per-drop-type chance baselines)
- `DropAmountVarianceFraction` (default +/-25%)
- `DropAmountVarianceCurve` (optional center-weighted custom distribution)
- `ScrapDropAmountVarianceFraction` / `MeatDropAmountVarianceFraction`
- `ScrapDropAmountVarianceCurve` / `MeatDropAmountVarianceCurve` (per-drop-type variance overrides)
- `DropInitialLinearSpeedMin/Max`
- `ScrapDropStacks`, `MeatDropStacks` (`Denomination + DropClass` stack definitions)
- `DropPawnCollisionMode` (`CollideWithPawns` or `IgnoreAllPawns`)
- `InvaderDesiredUpDirection`, `DropEarthGravityAcceleration` (shared gravity frame + debug-earth-gravity tuning)
- `bUseCapsuleDerivedPlayerPickupRadius` (explicit toggle for capsule-derived player pickup radius seeding)
- `DefaultPlayerPickupRadius` (manual pickup radius seed when capsule-derived mode is disabled)
- Soft caps:
- `SoftCapAliveEnemies`
- `SoftCapActiveProjectiles`
- `bBlockSpawnsWhenEnemySoftCapExceeded`
- Telemetry:
- `ProjectileActorClass`

## Blueprint Exposure
- All settings fields are `BlueprintReadOnly` for query/access patterns.
