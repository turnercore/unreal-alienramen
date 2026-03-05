# Save Subsystem Guide (`UARSaveSubsystem`)

This document describes the current C++ save/travel/hydration contracts used by runtime and Blueprints.

## Runtime Ownership

- Primary runtime API: `UARSaveSubsystem` (`Source/AlienRamen/Public/ARSaveSubsystem.h`)
- Save object schema: `UARSaveGame`
- Save index schema: `UARSaveIndexGame`
- Save structs: `FARSaveSlotDescriptor`, `FARSaveResult`, `FARPlayerStateSaveData`, `FARMeatState`
- Save schema version is `v3`; minimum supported is also `v3`.
- Save-backed GameState fields are native on `AARGameStateBase`: `Unlocks`, `Money`, `Scrap`, `Meat`, `Cycles` (replicated with change dispatchers).

The subsystem is a `UGameInstanceSubsystem`, so in Blueprint:
- `Get Game Instance Subsystem` -> `ARSaveSubsystem`

## What Happens Automatically

## 1) Revisioned saves and rollback load

- Physical save files use `<SlotBase>__<Revision>`.
- `LoadGame(SlotBase, RevisionOrLatest, Result)` supports rollback: if requested/latest revision fails, older revisions are tried in descending order.

## 2) Backup retention and pruning

- Max backup revisions comes from `UARSaveUserSettings::MaxBackupRevisions` (default `5`).
- Successful writes prune older revisions beyond retention:
  - `SaveCurrentGame(...)`
  - `PersistCanonicalSaveFromBytes(...)`

## 3) Multiplayer canonical snapshot distribution

- Server save builds one canonical `UARSaveGame` snapshot.
- Server serializes snapshot and sends to remote clients.
- Clients persist equivalent snapshot locally.

## 4) Client join sync

- Local non-authority `AARPlayerController` requests sync in `BeginPlay` (`ServerRequestCanonicalSaveSync`).
- Server pushes current canonical save via `PushCurrentSaveToPlayer`.
- If no current save is loaded yet, request is queued and flushed after next successful load/save.

## Blueprint API Surface

## Core save operations

- `CreateNewSave(DesiredSlotBase, OutSlot, OutResult, bUseDebugSaves)`
- `SaveCurrentGame(SlotBaseName, bCreateNewRevision, OutResult, bUseDebugSaves)`
- `LoadGame(SlotBaseName, RevisionOrLatest, OutResult, bUseDebugSaves)`
- `ListSaves(OutSlots, OutResult, bUseDebugSaves)`
- `DeleteSave(SlotBaseName, OutResult, bUseDebugSaves)`

## Utility helpers

- `GetCurrentSaveGame()`
- `HasCurrentSave()`
- `GetCurrentSlotBaseName()`
- `GetCurrentSlotRevision()`
- `GenerateRandomSlotBaseName(bEnsureUnique)`
- `GetMaxBackupRevisions()`
- `SetMaxBackupRevisions(NewMaxBackups)`
- `MarkSaveDirty()`
- `RequestAutosaveIfDirty(bCreateNewRevision, OutResult)`
- `IncrementSaveCycles(Delta, bSaveAfterIncrement, OutResult)`

## Hydration helpers

- `RequestGameStateHydration(Requester)`
- `TryHydratePlayerStateFromCurrentSave(Requester, bAllowSlotFallback)`
- `SetPendingTravelGameStateData(PendingStateData)`
- `ClearPendingTravelGameStateData()`
- `HasPendingTravelGameStateData()`

`UARSaveGame` BP readers:
- `FindPlayerStateDataBySlot(Slot, OutData, OutIndex)`
- `FindPlayerStateDataByIdentity(Identity, OutData, OutIndex)`

## Travel helpers

- `RequestServerTravel(URL, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bPersistSaveBeforeTravel)`
- `RequestOpenLevel(LevelName, Options, bSkipReadyChecks, bAbsolute, bPersistSaveBeforeTravel)`

Both capture one-shot `PendingTravelGameStateData` before map travel:
- If `bPersistSaveBeforeTravel=true`, travel saves to disk first, then clears pending travel data.
- If `bPersistSaveBeforeTravel=false`, travel skips disk save and carries pending travel data to next map hydration.

## BP Events

- `OnSaveStarted`
- `OnSaveCompleted`
- `OnLoadCompleted`
- `OnSaveFailed`
- `OnLoadFailed`
- `OnGameLoaded`

## Hydration Order (Current Contract)

GameState hydration (`RequestGameStateHydration`) is authority-only and runs at `AARGameStateBase::BeginPlay`:
1. Runtime starts from class/default values.
2. If a current save exists, persisted GameState fields are applied.
3. If pending travel GameState data exists, it overlays the current runtime values and is consumed/reset.

PlayerState hydration is split by lifecycle:
- First join path (GameMode): `TryHydratePlayerStateFromCurrentSave(...)` if possible, else `InitializeForFirstSessionJoin()`.
- Seamless travel path: `AARPlayerStateBase::CopyProperties(...)` copies runtime struct + key replicated fields.

## Typical Blueprint Flows

## New game / new save

1. Get subsystem.
2. Call `CreateNewSave(NAME_None or custom base, OutSlot, OutResult, bUseDebugSaves)`.
3. On success, travel/start flow as needed.

## Save current run

1. Get subsystem.
2. Call `SaveCurrentGame(CurrentSlotBaseName or None, true, OutResult, bUseDebugSaves)`.
3. Use `OutResult` and save events for UI feedback.

## Load save

1. Get subsystem.
2. Call `LoadGame(SlotBaseName, -1, OutResult, bUseDebugSaves)` for latest revision.
3. On `OnGameLoaded`, continue map/gameplay flow.

## Extend Save Data

When adding new persisted data:
1. Add field to `UARSaveGame` (or `FARPlayerStateSaveData` for player-scoped values).
2. Populate in `UARSaveSubsystem::GatherRuntimeData(...)`.
3. Apply in hydration flow.
4. If BP-facing, add reflected `UPROPERTY`.
5. Extend `UARSaveGame::ValidateAndSanitize(...)` for validation/clamping.

## Schema Versioning

When changing schema in a breaking way:
1. Bump `UARSaveGame::CurrentSchemaVersion`.
2. Update `UARSaveGame::MinSupportedSchemaVersion` policy as needed.
3. Add migration behavior only if supporting older schema versions.

## Troubleshooting

- Save fails with authority error:
  - Ensure save/travel save calls run on server in networked sessions.
- Join sync does not push immediately:
  - Expected when server has no current save yet; request stays queued.
- Hydration looks stale:
  - Verify `CurrentSaveGame` exists (`HasCurrentSave`) and GameState hydration is being called on authority.
