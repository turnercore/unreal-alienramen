# Save Subsystem Guide (`UARSaveSubsystem`)

This document explains how to use the C++ save system from Blueprints, what happens automatically, and how to extend/migrate it safely.

## Runtime Ownership

- Primary runtime API: `UARSaveSubsystem` (`Source/AlienRamen/Public/ARSaveSubsystem.h`)
- Save object schema: `UARSaveGame`
- Save index schema: `UARSaveIndexGame`
- Save types/structs: `FARSaveSlotDescriptor`, `FARSaveResult`, `FARPlayerStateSaveData`, `FARGameStateSaveData`, `FARMeatState`

The subsystem is a `UGameInstanceSubsystem`, so access it from BP via:
- `Get Game Instance Subsystem` -> class `ARSaveSubsystem`

## What Happens Automatically

## 1) Revisioned saves and rollback load

- Physical save files use: `<SlotBase>__<Revision>`
- Example: `Spicy_Ramen_Audit_108__3`
- `LoadGame(SlotBase, RevisionOrLatest, Result)` supports rollback:
  - if requested/latest revision fails to deserialize, older revisions are tried in descending order.

## 2) Backup retention and pruning

- Max backup revisions comes from `UARSaveUserSettings::MaxBackupRevisions` (default `5`).
- Save paths prune older revisions beyond retention:
  - `SaveCurrentGame(...)`
  - `PersistCanonicalSaveFromBytes(...)`

## 3) Multiplayer canonical snapshot distribution

- Server save builds one canonical `UARSaveGame` snapshot.
- Server serializes snapshot and sends to remote clients.
- Clients persist the same snapshot locally.
- This keeps host/client save parity for host-switch workflows.

## 4) Client join sync

- Local non-authority `AARPlayerController` auto-requests sync in `BeginPlay` (`ServerRequestCanonicalSaveSync`).
- Server pushes current canonical save to that client via subsystem.
- If no current save is available yet, request is queued and auto-flushed once a save becomes available.

## Blueprint API Surface

## Core save operations

- `CreateNewSave(DesiredSlotBase, OutSlot, OutResult)`
- `SaveCurrentGame(SlotBaseName, bCreateNewRevision, OutResult)`
- `LoadGame(SlotBaseName, RevisionOrLatest, OutResult)`
- `ListSaves(OutSlots, OutResult)`
- `DeleteSave(SlotBaseName, OutResult)`

## Utility helpers

- `GetCurrentSaveGame()`
- `HasCurrentSave()`
- `GetCurrentSlotBaseName()`
- `GetCurrentSlotRevision()`
- `GenerateRandomSlotBaseName(bEnsureUnique)`
- `GetMaxBackupRevisions()`
- `SetMaxBackupRevisions(NewMaxBackups)`

## Hydration helpers

- `RequestGameStateHydration(Requester)`
- `RequestPlayerStateHydration(Requester)`

`UARSaveGame` BP-compatible readers:
- `GetGameStateDataInstancedStruct()`
- `GetPlayerStateDataInstancedStructByIndex(Index)`
- `FindPlayerStateDataBySlot(Slot, OutData, OutIndex)`
- `FindPlayerStateDataByIdentity(Identity, OutData, OutIndex)`

## BP Events (bind from UI/GameInstance/etc.)

- `OnSaveCompleted`
- `OnLoadCompleted`
- `OnSaveFailed`
- `OnLoadFailed`
- `OnGameLoaded`

## Typical Blueprint Flows

## New game / new save

1. Get subsystem
2. Call `CreateNewSave(NAME_None or custom slot base, OutSlot, OutResult)`
3. On success, optionally call hydration or open next map

## Save current run

1. Get subsystem
2. Call `SaveCurrentGame(CurrentSlotBaseName or None, true, OutResult)` for new revision
3. Use `OutResult` + `OnSaveCompleted/OnSaveFailed` for UI feedback

## Load save

1. Get subsystem
2. Call `LoadGame(SlotBaseName, -1, OutResult)` for latest revision
3. On `OnGameLoaded`, run map/gameplay flow that depends on loaded state

## Hydrate world state after load

1. GameState calls `RequestGameStateHydration(self)`
2. PlayerState calls `RequestPlayerStateHydration(self)`
3. Existing `IStructSerializable`-based apply flow executes

## Adding New Save Data

## If you need to save another variable

1. Add field to `UARSaveGame` (for top-level persisted data), or to `FARPlayerStateSaveData` / `FARGameStateSaveData` for typed grouping.
2. Populate it in `UARSaveSubsystem::GatherRuntimeData(...)`.
3. Read/apply it during hydration path (or keep using instanced-struct apply if already covered by serializable state).
4. If it must be BP-visible, mark `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=...)`.
5. If data needs validation, extend `UARSaveGame::ValidateAndSanitize(...)`.

## If you need a new save operation utility

- Add function to `UARSaveSubsystem` as `UFUNCTION(BlueprintCallable/Pure)` under category `Alien Ramen|Save`.
- Keep authority-sensitive logic server-gated in subsystem.

## Renaming / Migrating Fields Safely

Renaming save fields can break old saves if not handled deliberately.

Recommended approach:
1. Add new field first, keep old field for one migration cycle.
2. During load/apply, if new field is unset, derive from old field.
3. Save writes only the new field after migration code runs.
4. Remove old field in a later version bump.

For larger schema changes:
1. Bump `UARSaveSubsystem::SaveSchemaVersion`.
2. Add compatibility migration in load/apply path.
3. Keep behavior deterministic and logged (warnings for fallback paths).

## Naming/Slot Rules

- Slot base names are logical identifiers (no revision suffix).
- Revision suffix is managed by subsystem only (`__N`).
- Do not handcraft physical revision names in BP; use subsystem APIs.

## Where to Extend by Responsibility

- `UARSaveSubsystem`: orchestration, authority flow, index, revisioning, network sync, hydration requests
- `UARSaveGame`: persisted schema + validation/sanitization
- `UARSaveUserSettings`: user-configurable save behavior (retention)
- `AARPlayerController`: join-time sync request/receive endpoints

## Troubleshooting

- Save fails with authority error:
  - Ensure `SaveCurrentGame` is called on server in networked mode.
- Client joins but no save appears:
  - Expected if server has no current save loaded yet; request is queued.
- Hydration appears stale:
  - Verify `CurrentSaveGame` is set (`HasCurrentSave`) and hydrators are called after load events.
