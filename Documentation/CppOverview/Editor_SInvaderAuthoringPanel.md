# Editor: SInvaderAuthoringPanel
Path: `Source/AlienRamenEditor/Private/ARInvaderAuthoringPanel.h/.cpp`

## Purpose
- Main custom editor tab (`Invader Authoring Tool`) for wave/stage DataTable authoring.

## Automatic Behavior
- Constructs UI + details views + list views.
- Tracks object transactions for undo/redo refresh.
- Maintains synchronized selection between row list, layer list, spawn list, and canvas.

## Major Function Groups
- Setup/refresh:
- `BuildLayout`, `RefreshTables`, `RefreshRowItems`, `RefreshLayerItems`, `RefreshSpawnItems`, `RefreshPalette`, `RefreshIssues`, `RefreshDetailsObjects`
- CRUD:
- `OnCreateRow`, `OnDuplicateRow`, `OnRenameRow`, `OnDeleteRow`, `OnToggleSelectedRowsEnabled`, `OnSaveTable`
- Wave editing:
- `OnAddLayer`, `OnAddSpawnToLayer`, `OnDeleteSelectedSpawn`, `SetSelectedSpawnColor`, reordering/drag handlers
- Context menus:
- `BuildSpawnContextMenu`, `BuildRowContextMenu`
- Validation:
- `OnValidateSelected`, `OnValidateAll`, row validators
- PIE harness:
- `OnStartOrAttachPIE`, `OnStartRun`, `OnStopRun`, `OnForceStage`, `OnForceWave`, `OnForceThreat`, `OnDumpState`
- Save bootstrap:
- `SchedulePIESaveBootstrap`, `RunPIESaveBootstrap`

## Key Editor State Variables
- Active mode: waves/stages
- Tables: `WaveTable`, `StageTable`
- Selection: selected row/layer/spawn sets
- Working collections: row/layer/spawn/issue items, palette entries
- UI refs: list views, details views, canvas, status text, etc.

## Current UX Contracts Implemented
- row multiselect duplicate/delete
- row context menu rename/duplicate/delete/enable-disable
- disabled rows shown as gray with `[Disabled]`
- canvas multiselect + drag move + rectangle select
- standard dirty-package persistence flow (no forced autosave every edit)

