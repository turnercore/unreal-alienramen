# Save Subsystem Guide (`UARSaveSubsystem`)

This document describes the current C++ save/travel/hydration contracts used by runtime and Blueprints.

## Runtime Ownership

- Primary runtime API: `UARSaveSubsystem` (`Source/AlienRamen/Public/ARSaveSubsystem.h`)
- Save object schema: `UARSaveGame`
- Save index schema: `UARSaveIndexGame`
- Save structs: `FARSaveSlotDescriptor`, `FARSaveResult`, `FARPlayerStateSaveData`, `FARCharacterSaveData`, `FARCharacterShopSnapshot`, `FARMeatState`
- Save schema version is `v19`; minimum supported is `v19`.
- Save-backed GameState fields are native on `AARGameStateBase`: `Unlocks`, `Money`, `Scrap`, `Meat`, `Cycles` (replicated with change dispatchers).
- Save ownership is explicit:
  - shared world state -> `UARSaveGame`
  - player-owned state -> `PlayerStates[]` keyed by player identity
  - character-owned state -> `CharacterStates[]` keyed by canonical character gameplay tag

## Persisted Payload Contract (`UARSaveGame`)

Authoritative persisted fields currently include:

- Economy/resources:
  - `Money`
  - `Scrap`
  - `Meat`
  - `Cycles`
- Progression + unlocks:
  - `ProgressionTags`
  - `Unlocks`
  - `FactionClout`
  - `ActiveFactionTag`
  - `ActiveFactionEffectTags`
  - `FactionPopularityStates`
- Shared dialogue/run payload:
  - `DialogueCompletedConversationTagsByGame`
  - `DialogueSpeakerRelationshipStates`
  - `StoredEnergyDrinkStacks`
  - `QueuedEnergyDrinkStacks`
  - `ActiveRunBuffPayloads`
  - `ActiveRunBuffCycleId`
  - `ShopTransientCarryables`
    - meat snapshots include `MeatQualityTier` (`EARVendingQualityTier`, default `Standard`)
- Player payload:
  - `PlayerStates[]`:
    - `Identity` (optional online id/type, display name, shared-account primary/secondary profile flag)
    - canonical `CurrentCharacterTag`
    - `bDialogueAutoAdvanceEnabled`
    - player-owned `ProgressionTags`
- Character payload:
  - `CharacterStates[]`:
    - canonical `CharacterTag`
    - canonical character-owned `LoadoutTags`
    - character-owned core attributes (`Health`, `MaxHealth`, `Spice`, `MaxSpice`, `MoveSpeed`, `Strength`)
    - character-owned life state (`bIsDowned`, `bIsDeadState`)
    - character-owned invader runtime state (`PlayerColor`, `ComboCount`, `ActivatedUpgradeTags`, `bIsSharingSpice`, `SpicyTrackCursorTier`)
    - dialogue progression/completion/choice-memory state
    - shop-only character world snapshot (`CharacterTransform`, held supported carryable snapshot)
- Save metadata:
  - `SaveSlot`
  - `SaveGameVersion`
  - `SaveSlotNumber`
  - `LastSaved`
  - `LastSavedModeTag`
  - `LastSavedMapPath`

For progression/unlock usage details, see [Progression + Unlocks Guide](README_ProgressionUnlocks.md).

The subsystem is a `UGameInstanceSubsystem`, so in Blueprint:
- `Get Game Instance Subsystem` -> `ARSaveSubsystem`

## What Happens Automatically

## 1) Revisioned saves and rollback load

- Physical save files use `<SlotBase>__<Revision>`.
- `LoadGame(SlotBase, RevisionOrLatest, Result)` supports rollback: if requested/latest revision fails, older revisions are tried in descending order.

## 2) Backup retention and pruning

- Max backup revisions comes from `UARSaveUserSettings::MaxBackupRevisions` (default `5`).
- Canonical save payload logging is controlled by `UARSaveUserSettings::bLogCanonicalSavePayloadSize`; the warning and critical thresholds come from `CanonicalSaveWarningSizeBytes` and `CanonicalSaveCriticalSizeBytes`.
- Successful writes prune older revisions beyond retention:
  - `SaveCurrentGame(...)` async path
  - `SaveCurrentGameBlocking(...)` for travel-critical blocking saves
  - `PersistCanonicalSaveFromBytes(...)`
- If the index commit fails after a revision file is written, the subsystem rolls back the just-written revision slot before reporting failure.

## 3) Multiplayer canonical snapshot distribution

- Server save builds one canonical `UARSaveGame` snapshot.
- Normal saves queue disk persistence asynchronously; the completion callback finalizes index/prune/client sync.
- Server serializes snapshot and sends to remote clients.
- Clients persist equivalent snapshot locally.
- Every canonical snapshot send logs payload size in bytes and KiB, and escalates to warning/error once the configurable thresholds are crossed.
- Index commit failures attempt to delete the just-written revision so orphaned latest revisions do not linger after a failed save.

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
- `AdvanceWorldDays(DeltaDays, bPersistImmediately, OutResult)`
- `GetPlayerProgressionTags(Requester, OutTags)`
- `HasPlayerProgressionTag(Requester, Tag)`
- `AddPlayerProgressionTag(Requester, Tag)`
- `RemovePlayerProgressionTag(Requester, Tag)`
- `UARSaveTypesLibrary::GetTotalMeatAmount(FARMeatState)` (Blueprint pure helper for aggregate meat)

## Hydration helpers

- `RequestGameStateHydration(Requester)`
- `TryHydratePlayerStateFromCurrentSave(Requester)`
- `SetPendingTravelGameStateData(PendingStateData)`
- `HasPendingFreshLoadEntry()`
- `GetPendingLoadedSaveModeTag()`
- `GetPendingLoadedSaveMapPath()`
- `ClearPendingFreshLoadEntry()`

Hydration identity policy:
- If requester has a strict online identity (`UniqueNetIdString` + non-null provider type), hydration requires identity match.
- When multiple rows share the same online identity (for example two local couch players on one Steam account), identity lookup uses a shared-account primary/secondary profile discriminator (`bSharedOnlineIdSecondaryProfile`) assigned by runtime claim order.
- `ClearPendingTravelGameStateData()`
- `HasPendingTravelGameStateData()`

`UARSaveGame` BP readers:
- `FindPlayerStateDataByIdentity(Identity, OutData, OutIndex)`

## Travel Boundary

Travel execution is owned by `UARTravelSubsystem` (not `UARSaveSubsystem`).

`UARSaveSubsystem` still owns the save-side contracts consumed by travel orchestration:
- `SetPendingTravelGameStateData(...)` / `ClearPendingTravelGameStateData(...)`
- `SaveCurrentGame(...)` / `SaveCurrentGameUnthrottled(...)`
- loaded-save destination metadata:
  - `GetCurrentSaveGame()->LastSavedMapPath`
  - `GetCurrentSaveGame()->LastSavedModeTag`
  - `GetPendingLoadedSaveMapPath()`
  - `GetPendingLoadedSaveModeTag()`

## BP Events

- `OnSaveStarted`
- `OnSaveCompleted`
- `OnLoadCompleted`
- `OnSaveFailed`
- `OnLoadFailed`
- `OnGameLoaded`
- `CreateNewSave(...)` does not broadcast `OnSaveCompleted` because it intentionally does not persist to disk.

## Hydration Order (Current Contract)

GameState hydration (`RequestGameStateHydration`) is authority-only and runs at `AARGameStateBase::BeginPlay`:
1. Runtime starts from class/default values.
2. If a current save exists, persisted GameState fields are applied.
3. Default starting unlock baseline (`UARLoadoutSettings::GetEffectiveDefaultStartingUnlocks`) is merged into runtime unlocks.
4. If pending travel GameState data exists, it overlays the current runtime values and is consumed/reset.

PlayerState hydration is split by lifecycle:
- First join path (GameMode): `TryHydratePlayerStateFromCurrentSave(...)` if possible, else `InitializeForFirstSessionJoin()`.
- Seamless travel path: `AARPlayerStateBase::CopyProperties(...)` copies player-owned fields only.
- Player hydration is two-stage:
  1. hydrate player-owned fields by identity
  2. resolve active `CurrentCharacterTag`, ensure/hydrate `AARCharacterStateRuntime`, and bind `CurrentCharacterRuntime` on PlayerState
- Character runtime setup is coordinated by `UARCharacterSubsystem` (runtime registry + pawn/controller binding), not by save structs directly.
- If projected character-owned `LoadoutTags` are empty after hydration, runtime setup seeds `UARLoadoutSettings::DefaultPlayerLoadoutTags` so editor raw-map starts and runtime joins both get a deterministic baseline.
- `AARPlayerStateBase` is the player-owned access surface, while `AARCharacterStateRuntime` is the replicated owner of character runtime state.

## Typical Blueprint Flows

## New game / new save

1. Get subsystem.
2. Call `CreateNewSave(NAME_None or custom base, OutSlot, OutResult, bUseDebugSaves)`.
3. On success, the subsystem keeps a new canonical save in memory, marks it dirty, and does not write any slot/index files yet.
4. The in-memory-only new save keeps `LastSaved` unset (`FDateTime()`/zero ticks) until the first persisted write.
5. Joining clients do not receive/persist canonical save bytes for that new run until the first real save has happened.
6. First explicit save/autosave writes revision `0` for that slot base.

## Save current run

1. Get subsystem.
2. Call `SaveCurrentGame(CurrentSlotBaseName or None, true, OutResult, bUseDebugSaves)`.
3. Treat `OutResult` as "save started"; use `OnSaveCompleted` / `OnSaveFailed` for final status.

If you need a blocking save before travel or another hard gate, call `SaveCurrentGameBlocking(...)` from C++.

## Load save

1. Get subsystem.
2. Call `LoadGame(SlotBaseName, -1, OutResult, bUseDebugSaves)` for latest revision.
3. On `OnGameLoaded`, continue map/gameplay flow.

## Extend Save Data

When adding new persisted data:
1. Decide the ownership bucket first: shared world, player-owned, or character-owned.
2. Add the field to `UARSaveGame`, `FARPlayerStateSaveData`, or `FARCharacterSaveData` accordingly.
3. Populate in `UARSaveSubsystem::GatherRuntimeData(...)`.
4. Apply in the correct hydration/projection flow (`AARCharacterStateRuntime` for character-owned runtime state, `AARPlayerStateBase` for player-owned state).
5. If BP-facing, add reflected `UPROPERTY`.
6. Extend `UARSaveGame::ValidateAndSanitize(...)` and migration logic when schema compatibility requires it.

## Schema Versioning

When changing schema in a breaking way:
1. Bump `UARSaveGame::CurrentSchemaVersion`.
2. Update `UARSaveGame::MinSupportedSchemaVersion` policy as needed.
3. Add migration behavior when supporting older schema versions.

## Troubleshooting

- Save fails with authority error:
  - Ensure save/travel save calls run on server in networked sessions.
- Join sync does not push immediately:
  - Expected when server has no current save yet; request stays queued.
- Hydration looks stale:
  - Verify `CurrentSaveGame` exists (`HasCurrentSave`) and GameState hydration is being called on authority.
