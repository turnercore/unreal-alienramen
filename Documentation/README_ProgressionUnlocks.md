# Progression + Unlocks Contract

This document defines how progression tags and unlock tags are stored, mutated, and consumed across systems.

## Ownership and Persistence

- Save owner: `UARSaveSubsystem` / `UARSaveGame`
- Runtime mirror owner for unlocks: `AARGameStateBase`
- Persisted fields:
  - shared `ProgressionTags`
  - player-owned `PlayerStates[].ProgressionTags`
  - `Unlocks`
  - `FactionClout`
  - `ActiveFactionTag`
  - `ActiveFactionEffectTags`
  - `FactionPopularityStates`

Shared/player progression tags and unlocks are long-lived save state. They are not transient per-map runtime flags.

## Semantic Split

- shared `ProgressionTags`: narrative/world-state milestones that apply to the whole save
- player-owned `PlayerStates[].ProgressionTags`: milestones that should follow a specific player identity regardless of active character
- `Unlocks`: gameplay inventory/loadout/content-unlock surface used by mode systems and player setup.

Keep the split intentional:
- Use progression tags for story/world gating and faction modifier rules.
- Use unlock tags for equipment/content availability and run-level player options.

## Mutation APIs (C++)

Save-owned progression APIs on `UARSaveSubsystem`:
- `GetProgressionTags()`
- `HasProgressionTag(Tag)`
- `AddProgressionTag(Tag)`
- `RemoveProgressionTag(Tag)`
- `GetPlayerProgressionTags(PlayerState, OutTags)`
- `HasPlayerProgressionTag(PlayerState, Tag)`
- `AddPlayerProgressionTag(PlayerState, Tag)`
- `RemovePlayerProgressionTag(PlayerState, Tag)`
- `GetFactionClout()`
- `SetFactionClout(NewClout)`

Unlock mutation normally flows through replicated GameState (`AARGameStateBase`) helpers and then save persistence:
- `AddUnlockTag`
- `RemoveUnlockTag`
- `SetUnlocksFromSave`

When unlocks are changed at runtime, save should be marked dirty so normal autosave/manual save writes include the update.

## Placed-Actor Unlock Reactivity

`UARUnlockReactiveComponent` provides a reusable placed-actor/Blueprint hook into `AARGameStateBase` unlock tags.

- Runtime truth still lives on `AARGameStateBase::GetUnlocks()` and its replicated delegates.
- The component binds `OnHydratedFromSave` and `OnUnlocksChanged`, then calls `RefreshFromGameState()` in `BeginPlay`.
- Base unlock gate uses `RequiredUnlockTags`:
  - empty `RequiredUnlockTags` => unlocked
  - otherwise unlocked when `GetUnlocks().HasAll(RequiredUnlockTags)` is true
- Upgrade replay uses authored `OrderedUpgradeTags` and checks each tag independently against current unlocks.
- Refresh replay order is deterministic:
  1. `OnLocked()` when base gate fails (no upgrade replay)
  2. `OnUnlocked()` when base gate passes
  3. `OnUpgrade(Tag)` for each active authored upgrade tag, in authored order

## Default Seeding and Hydration

- Default unlock baseline comes from `UARLoadoutSettings::DefaultStartingUnlocks`.
- Effective default baseline (`UARLoadoutSettings::GetEffectiveDefaultStartingUnlocks`) is merged into unlocks during:
  - runtime gather before save write
  - authority GameState hydration
  - save sanitize/normalize on load

Hydration precedence:
1. Runtime/default values
2. Current save payload
3. Optional pending-travel overlay (consumed once)

## Dialogue Integration

Dialogue unlock conditions are evaluated server-side from both save and GameState:
- progression checks:
  - `RequiredProgressionTags`
  - `BlockedProgressionTags`
- unlock checks:
  - `RequiredUnlockTags`
  - `BlockedUnlockTags`

Dialogue progression grants write into save progression:
- row enter: `GrantProgressionTagsOnEnter`
- choice commit: `GrantProgressionTags`

These writes mark save dirty.

## Faction Integration

Faction ranking and election read/save progression state:
- popularity modifier rules evaluate against save `ProgressionTags`
- candidate count derives from save `FactionClout`
- finalization writes:
  - `ActiveFactionTag`
  - `ActiveFactionEffectTags`
  - `FactionPopularityStates`

These writes mark save dirty and are applied to runtime GameState elected-faction mirrors.

## Networking/Authority Rules

- Mutations must be server-authoritative in networked sessions.
- Clients should call gameplay/controller/subsystem entrypoints that route to authority; do not mutate progression/unlocks directly on clients.
- Replication source of truth for runtime unlock state is `AARGameStateBase`; long-term canonical persistence remains `UARSaveGame`.

## Practical Use Guidelines

- Add a new story gate: use a new `Progression.*` tag.
- Add a new equip/content gate: use an `Unlock.*` tag.
- If a feature must survive restart, ensure it writes to save-owned progression/unlocks and marks save dirty.
- If a feature is run-temporary only, keep it out of progression/unlocks and use runtime-only state.
