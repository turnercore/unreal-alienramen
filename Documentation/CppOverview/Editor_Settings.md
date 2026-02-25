# Editor Settings Objects

## UARInvaderToolingSettings
Path: `Source/AlienRamenEditor/Public/ARInvaderToolingSettings.h`

Purpose:
- Project-level editor tooling config for Invader Authoring.

Key fields:
- Data tables: `WaveDataTable`, `StageDataTable`
- Enemy palette source folder: `EnemiesFolder`
- Persistence: `bAutoSaveTablesOnEdit`
- Backups: `bCreateBackupOnToolOpen`, `BackupRetentionCount`, `BackupsFolder`
- PIE bootstrap:
- `bEnablePIESaveBootstrap`
- `PIEBootstrapLoadingMap`
- `PIELoadSlotName`, `PIELoadSlotNumber`
- `PIEBootstrapDebugMap`

---

## UARInvaderAuthoringEditorSettings
Path: `Source/AlienRamenEditor/Public/ARInvaderAuthoringEditorSettings.h`

Purpose:
- Per-user editor settings for authoring workflow.

Key fields:
- Testing: `DefaultTestMap`, `LastSeed`
- Palette: `FavoriteEnemyClasses`, `EnemyClassShapeCycles`
- Preview toggles: `bHideOtherLayersInWavePreview`, `bShowApproximatePreviewBanner`

