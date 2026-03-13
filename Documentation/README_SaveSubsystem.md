# Save Subsystem Guide (`UARSaveSubsystem`)

This document describes the current C++ save/travel/hydration contracts used by runtime and Blueprints.

## Runtime Ownership

- Primary runtime API: `UARSaveSubsystem` (`Source/AlienRamen/Public/ARSaveSubsystem.h`)
- Save object schema: `UARSaveGame`
- Save index schema: `UARSaveIndexGame`
- Save structs: `FARSaveSlotDescriptor`, `FARSaveResult`, `FARPlayerStateSaveData`, `FARPlayerCharacterSaveData`, `FARCharacterSaveData`, `FARCharacterShopSnapshot`, `FARMeatState`
- Save schema version is `v12`; minimum supported is `v10`.
- Save-backed GameState fields are native on `AARGameStateBase`: `Unlocks`, `Money`, `Scrap`, `Meat`, `Cycles` (replicated with change dispatchers).
- Save ownership is explicit:
  - shared world state -> `UARSaveGame`
  - player-owned state -> `PlayerStates[]` keyed by player identity
  - character-owned state -> `CharacterStates[]` keyed by canonical character gameplay tag
  - player-character-owned state -> nested `PlayerStates[].CharacterStates[]`

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
  - `DialogueRelationshipStates`
  - `StoredEnergyDrinkStacks`
  - `QueuedEnergyDrinkStacks`
  - `ActiveRunBuffPayloads`
  - `ActiveRunBuffCycleId`
  - `ShopTransientCarryables`
- Player payload:
  - `PlayerStates[]`:
    - `Identity` (`PlayerSlot`, optional online id/type, legacy id/name)
    - canonical `CurrentCharacterTag`
    - compatibility `CharacterPicked`
    - `bDialogueAutoAdvanceEnabled`
    - player-owned `ProgressionTags`
    - compatibility `LoadoutTags` for the active character
    - nested `CharacterStates[]` for player-character-owned state (currently per-character loadouts)
- Character payload:
  - `CharacterStates[]`:
    - canonical `CharacterTag`
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
- `GetPlayerProgressionTags(Requester, OutTags, bAllowSlotFallback)`
- `HasPlayerProgressionTag(Requester, Tag, bAllowSlotFallback)`
- `AddPlayerProgressionTag(Requester, Tag)`
- `RemovePlayerProgressionTag(Requester, Tag)`
- `UARSaveTypesLibrary::GetTotalMeatAmount(FARMeatState)` (Blueprint pure helper for aggregate meat)

## Hydration helpers

- `RequestGameStateHydration(Requester)`
- `TryHydratePlayerStateFromCurrentSave(Requester, bAllowSlotFallback)`
- `SetPendingTravelGameStateData(PendingStateData)`
- `HasPendingFreshLoadEntry()`
- `GetPendingLoadedSaveModeTag()`
- `GetPendingLoadedSaveMapPath()`
- `ClearPendingFreshLoadEntry()`

Hydration identity policy:
- If requester has a strict online identity (`UniqueNetIdString` + non-null provider type), hydration requires identity match and does not slot-fallback.
- Slot fallback is only used for local-only identities (PIE/offline/null subsystem style flows).
- When multiple rows share the same online identity (for example two local couch players on one Steam account), identity lookup prefers the row matching requester `PlayerSlot`.
- `ClearPendingTravelGameStateData()`
- `HasPendingTravelGameStateData()`

`UARSaveGame` BP readers:
- `FindPlayerStateDataBySlot(Slot, OutData, OutIndex)`
- `FindPlayerStateDataByIdentity(Identity, OutData, OutIndex)`

## Travel helpers

- `RequestServerTravel(URL, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bPersistSaveBeforeTravel)`
- `RequestOpenLevel(LevelName, Options, bSkipReadyChecks, bAbsolute, bPersistSaveBeforeTravel)`
- `TravelToLoadedSaveDestination(bUseOpenLevelInPIE, TransitionMapURL)`

Both capture one-shot `PendingTravelGameStateData` before map travel:
- If `bPersistSaveBeforeTravel=true`, travel saves to disk first, then clears pending travel data.
- If `bPersistSaveBeforeTravel=false`, travel skips disk save and carries pending travel data to next map hydration.

`TravelToLoadedSaveDestination(...)` is the standard save-load gameplay entry path:
- it reads the loaded save's recorded destination map
- builds a `FARTransitionContext` with `SourceMode=SaveLoad`, `Reason=SaveLoadEntry`, `bFreshLoadEntry=true`
- routes through the transition map URL by default so downstream gameplay maps receive the same fresh-load signal

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
- Player hydration is two-stage:
  1. hydrate player-owned fields by identity (or slot fallback for local-only identities)
  2. resolve active `CurrentCharacterTag` and project character-owned plus player-character-owned state onto `AARPlayerStateBase`
- `AARPlayerStateBase` remains the runtime owner surface, but character-owned persistence is not keyed by player id.

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
1. Decide the ownership bucket first: shared world, player-owned, character-owned, or player-character-owned.
2. Add the field to `UARSaveGame`, `FARPlayerStateSaveData`, `FARCharacterSaveData`, or `FARPlayerCharacterSaveData` accordingly.
3. Populate in `UARSaveSubsystem::GatherRuntimeData(...)`.
4. Apply in the correct hydration/projection flow.
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
