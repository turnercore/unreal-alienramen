# Persistence Guide

This page is the top-level reference for how persistence works in Alien Ramen.

Use this document when you need to answer:

- what gets saved
- who owns a piece of saved data
- how save/load hydration works
- how travel interacts with persisted state
- which API to call from gameplay or Blueprint

For subsystem detail, also see:

- [Saving, Loading, and Hydration](README_SaveSubsystem.md)
- [Transition Mode](README_TransitionMode.md)
- [Progression & Unlocks](README_ProgressionUnlocks.md)
- [Parley Runtime](README_DialogueNPC.md)
- [Shop Game Mode Runtime](README_ShopRamenSystem.md)

## Ownership Model

Persistence is split into three buckets. The key rule is to choose the owner by what the data follows.

### 1. Shared world state

Owner:
- `UARSaveGame`
- runtime mirror: `AARGameStateBase`

Use this for:
- economy (`Money`, `Scrap`, `Meat`, `Cycles`)
- shared progression/unlocks
- faction state
- shared directed dialogue relationship matrix (`SourceSpeakerTag -> TargetSpeakerTag`)
- shared run-buff storage/queue/active payloads
- loose shop carryable reload snapshots

If the whole save slot/world should see one authoritative value, it belongs here.

### 2. Player-owned state

Owner:
- `UARSaveGame::PlayerStates[]`
- keyed by `FARPlayerIdentity`
- runtime mirror/projection: `AARPlayerStateBase`

Use this for:
- display name / identity
- player preferences such as dialogue auto-advance
- player-owned progression tags
- active or last-selected character pointer

If the data should follow the player identity even when they swap characters, it belongs here.

### 3. Character-owned state

Owner:
- `UARSaveGame::CharacterStates[]`
- keyed by canonical character gameplay tag

Use this for:
- character dialogue history / completion / choice memory
- character-owned loadouts
- character-specific inventory/equipment state
- shop-only character world restore snapshot

If the data should stay with the character regardless of which player controls them, it belongs here.

## Canonical Identity Rules

- Character identity is canonical as a gameplay tag.
- Runtime controller identity is `AARPlayerStateBase::PlayerSlotId` (controller/profile-owned, not gameplay ownership).
- `EARPlayerSlot`/slot-tag helpers are legacy migration/plugin-boundary shims only and are no longer authoritative runtime ownership keys.
- Save player rows do not persist slot snapshot as a durable identity key.
- Shared-account local players are disambiguated by a persisted primary/secondary profile flag under the same online id (`FARPlayerIdentity::bSharedOnlineIdSecondaryProfile`), while runtime join/travel normalization remains authoritative for controller slot-id assignment.
- Character runtime ownership is on `AARCharacterStateRuntime`; runtime control projects through `AARPlayerStateBase` current-character pointers.

## Runtime Flow

### Save flow

Primary entrypoint:
- `UARSaveSubsystem::SaveCurrentGame(...)`

High-level sequence:
1. gather current runtime state into a fresh `UARSaveGame`
2. merge in persisted state that does not have a live runtime mirror in the current map
3. sanitize/migrate payload
4. queue disk persistence asynchronously
5. on completion, update the current in-memory canonical save and index/prune state
6. send canonical save bytes to remote clients

If the revision file is already on disk but the index write fails, the subsystem rolls back the revision slot before surfacing the failure.
Canonical snapshot sends also log payload size and recipient count, with configurable warning/critical thresholds in `UARSaveUserSettings` so the path becomes monitorable before chunking is needed.

Blocking travel/save-gate flows use `UARSaveSubsystem::SaveCurrentGameBlocking(...)` instead.

Important expectations:
- save is authority-only
- canonical saves are blocked during active dialogue
- revisioned saves use `<SlotBase>__<Revision>`
- save-facing `AARGameStateBase` mutations mark the canonical save dirty when they change live authoritative state, so quit/leave autosaves can persist shop/shared-economy changes without each gameplay system hand-marking dirtiness
- Parley/ParleyFaction runtime is save-agnostic; `UARParleySaveBridge` listens to plugin events and marks dirty (no forced autosave).
- Important conversation completion should only mark save dirty through the bridge policy; it does not directly force autosave from Parley.

### Load flow

Primary entrypoint:
- `UARSaveSubsystem::LoadGame(...)`

High-level sequence:
1. load save object from disk with rollback support
2. validate schema support
3. run migration + sanitize
4. install the loaded save as the current canonical save
5. raise load-complete events
6. gameplay then travels into the saved destination using `UARTravelSubsystem::TravelToLoadedSaveDestination(...)`

Important expectations:
- `LoadGame(...)` loads the save into memory; it does not by itself hydrate a gameplay map
- travel into gameplay from a loaded save should use the standard save-load entry path

## Hydration Flow

### GameState hydration

Primary entrypoint:
- `UARSaveSubsystem::RequestGameStateHydration(...)`

Order:
1. start from runtime defaults
2. apply current save shared fields
3. apply configured starting unlock baseline (`UARLoadoutSettings::DefaultStartingUnlocks`) into runtime unlocks
4. if pending travel overlay exists, apply that on top once

### PlayerState hydration

Primary entrypoint:
- `UARSaveSubsystem::TryHydratePlayerStateFromCurrentSave(...)`

Order:
1. resolve player row by identity
2. apply player-owned fields to `AARPlayerStateBase`
3. resolve active `CurrentCharacterTag`
4. ensure/hydrate `AARCharacterStateRuntime` for the resolved character tag and apply character-owned save data there
5. bind `AARPlayerStateBase::CurrentCharacterRuntime` and sync active runtime projection reads
6. bind runtime to active pawn through `UARCharacterSubsystem` orchestration
7. gameplay-mode join normalization enforces unique runtime character occupancy (`Brother`/`Sister`) and does not source ownership from save slot fallbacks
8. if projected character-owned `LoadoutTags` are empty after hydration, runtime setup leaves them blank; Invader resolves its own fallback later when it needs a ship class.

### Seamless travel

Primary runtime carry path:
- `AARPlayerStateBase::CopyProperties(...)`

Expectation:
- seamless travel keeps the active projected runtime state alive without requiring disk save/load
- `AARPlayerStateBase::CopyProperties(...)` only carries player-owned fields (slot id, character pointer identity, display name, dialogue preference, and selected transient resets) and intentionally does not run generic StructSerializable by-name overlays for PlayerState handoff
- character-owned runtime carry/hydration is explicit via save/travel + `UARCharacterSubsystem` + `AARCharacterStateRuntime`; it is not implicit via PlayerState state copies
- character-owned loadouts remain canonical by `CurrentCharacterTag`; `PlayerState` no longer owns a mirrored authoritative loadout payload
- authoritative mode join/travel normalization ensures `CurrentCharacterTag` remains valid (`Brother`/`Sister`) and resolves non-taken fallback selection when a tag is missing/invalid
- `AARGameModeBase::HandleStartingNewPlayer(...)` normalizes character assignment before spawn so `ChoosePlayerStart` and pawn-class resolution do not run on unknown identity.
- `HandleFirstSessionJoinSetup(...)` hydrates player identity from save using strict identity matching (no slot fallback), then preserves character-native ownership.
- `AARGameModeBase::ChoosePlayerStart_Implementation(...)` performs an authority-side just-in-time character normalization pass before querying tagged starts, covering editor/raw-map startup timing where character selection can otherwise lag.
- `ChoosePlayerStart_Implementation(...)` prioritizes canonical character-tag start points.
- `ChoosePlayerStart_Implementation(...)` emits warnings when identity is unresolved before spawn-point selection (and if still unresolved after normalization), so startup spawn races are visible in logs.
- `HandleStartingNewPlayer(...)` performs a one-shot `RestartPlayer(...)` retry only when `Super::HandleStartingNewPlayer_Implementation(...)` leaves the controller without a pawn after normalization, recovering failed initial spawn without forcing extra respawns when identity later changes.
- first-session/no-save joins assign a random available canonical character (`Brother`/`Sister`) while preserving uniqueness when possible.
- `AARGameModeBase::HandleSeamlessTravelPlayer(...)` immediately re-runs character/controller-id normalization (`EnsureJoinedPlayerHasUniqueIdentity` + `NormalizeConnectedPlayersIdentity`) so transient handoff overlap cannot leave duplicate ownership.
- seamless-travel controller replacement must flow through `GetPlayerControllerClassToSpawnForSeamlessTravel(...)` + engine handoff (`SeamlessTravelTo/From`) rather than post-super manual `SwapPlayerControllers` calls
- authoritative gameplay-mode normalization leaves missing ship loadouts blank outside Invader; Invader resolves its own fallback only when it needs a pawn class
- `AARPlayerStateBase::UpdateLoadoutWithTag(...)` ignores invalid/empty incoming tags and leaves an empty runtime loadout blank instead of auto-seeding defaults.

## Travel and Persistence

### Normal mode travel

Primary entrypoints:
- `AARGameModeBase::TryStartTravel(...)`
- `UARTravelSubsystem::RequestServerTravel(...)`
- `UARTravelSubsystem::RequestOpenLevel(...)`

What happens:
1. readiness is checked unless skipped
2. shared `GameState` travel overlay is captured
3. optional disk save runs when the mode is configured to save on exit
4. travel starts

Additional durability rules:
- first authoritative entry into `Mode.Shop` persists an immediate canonical save when the current save still points at a different mode/map (for example fresh new-game start or shop re-entry after non-shop save state)
- `Invader -> Transition -> Scrapyard` now advances world day/cycle by +1 and persists immediately in transition init, so completed invader runs survive transition-map or early-scrapyard crashes/quits
- `Scrapyard -> Transition -> Shop` now commits a canonical save immediately after finalization and before travel so rewards/economy survive transition-map crashes or immediate post-run host quits

### Save-load gameplay entry

Primary entrypoint:
- `UARTravelSubsystem::TravelToLoadedSaveDestination(...)`

This is the standard path after `LoadGame(...)`.

What it does:
1. reads the loaded save's recorded destination map
   - if missing, resolves fallback destination from `LastSavedModeTag`
2. builds `FARTransitionContext` with:
   - `SourceMode=SaveLoad`
   - `Reason=SaveLoadEntry`
   - `bFreshLoadEntry=true`
3. routes through transition map by default
4. transition mode preserves that same context on the continue leg
5. final gameplay map receives the same save-load entry signal in travel options
   - caller-provided destination options are preserved through transition-map routing

Use this path for any load-only gameplay behavior.

## Fresh-Load-Only Logic

Two signals exist for fresh-load-only behavior:

- explicit transition context in travel options
- save-subsystem one-shot flag:
  - `HasPendingFreshLoadEntry()`
  - `GetPendingLoadedSaveModeTag()`
  - `GetPendingLoadedSaveMapPath()`
  - `ClearPendingFreshLoadEntry()`

Design expectation:
- new gameplay restore logic should key off the standard save-load entry contract
- do not invent one-off per-mode load-entry booleans when the transition context already expresses the reason for entry

Current example:
- shop character transform / held-item restore only applies when loading back into a save that was saved in `Mode.Shop`
- shop loads may also materialize missing unpossessed character pawns from `CharacterStates[]` so local character switching has both runtime bodies available immediately

## Blueprint Usage

### Create a new save

1. Get `ARSaveSubsystem` from `GameInstance`.
2. Call `CreateNewSave(...)`.
3. On success, gameplay continues with a dirty in-memory canonical save; nothing is written to disk yet.
4. Canonical save sync for joining clients is also deferred until the first persisted save.
5. First explicit save/autosave persists revision `0` for that slot.

### Save current game

1. Get `ARSaveSubsystem`.
2. Call `SaveCurrentGame(...)`.
3. Use `FARSaveResult` and save events for feedback.

### Load a save and enter gameplay

1. Get `ARSaveSubsystem`.
2. Call `LoadGame(...)`.
3. Get `ARTravelSubsystem` and call `TravelToLoadedSaveDestination(...)`.

### Query player-owned progression

Use:
- `GetPlayerProgressionTags(...)`
- `HasPlayerProgressionTag(...)`
- `AddPlayerProgressionTag(...)`
- `RemovePlayerProgressionTag(...)`

Use this for:
- first-time cinematics per player
- profile-specific milestones
- tutorial completion per player identity

Do not use shared `ProgressionTags` for those.

## What To Expect

### Things that persist

- save-owned shared state
- player-owned state keyed by identity
- character-owned state keyed by character tag

### Things that do not persist unless explicitly modeled

- arbitrary transient actor state
- generic per-map runtime scratch state
- station processing runtime
- temporary GAS cooldown timers/effects (cooldowns reset on spawn/travel by design)
- anything not extracted into save structs or travel overlay structs

### Common pitfalls

- putting character-owned data on `GameState`
- keying player-owned data by character tag
- mutating persistence on clients
- loading a save and then entering gameplay through an ad hoc travel path instead of `UARTravelSubsystem::TravelToLoadedSaveDestination(...)`

## Rules For New Persistence

When adding something new:
1. choose the ownership bucket first
2. add the field to the correct save struct
3. decide whether it hydrates to `GameState`, `PlayerState`, or a runtime actor/component
4. update gather, hydrate, sanitize, and migration
5. update docs in the same pass

If ownership is unclear, ask:
- does this follow the save/world?
- the player identity?
- the character identity?
