# Progression + Unlocks Contract

This document defines how progression tags and unlock tags are stored, mutated, and consumed across systems.

## Ownership and Persistence

- Save owner: `UARSaveSubsystem` / `UARSaveGame`
- Runtime mirrors:
  - save-wide progression/unlocks -> `AARGameStateBase`
  - player-owned progression -> `AARPlayerStateBase`
  - character-owned progression -> `AARCharacterStateRuntime`
- Persisted fields:
  - save-wide `GameProgressionTags`
  - player-owned `PlayerStates[].PlayerProgressionTags`
  - character-owned `CharacterStates[].CharacterProgressionTags`
  - `Unlocks`
  - `FactionClout`
  - `ActiveFactionTag`
  - `ActiveFactionEffectTags`
  - `FactionPopularityStates`

Long-lived progression is split by owner. Runtime-only routing state belongs in `Progression.Transient.*` and is assembled into interaction context on demand instead of being saved.

## Semantic Split

- `Progression.Game.*`: narrative/world-state milestones that apply to the whole save
- `Progression.Game.Unlock.*`: canonical authored unlock namespace; mirrored into `UARSaveGame::Unlocks` / `AARGameStateBase::Unlocks` convenience containers
- `Progression.Player.*`: milestones that should follow a specific player identity regardless of active character
- `Progression.Character.*`: progression that belongs to the canonical character regardless of controller
- `Progression.Transient.*`: runtime-only interaction routing tags; never serialized
- `Invader.Upgrade.*`: authored Invader upgrade namespace stays unchanged; picked/activated upgrades are character-owned runtime/save state

Keep the split intentional:
- Use progression tags for story/world gating and faction modifier rules.
- Use `Progression.Game.Unlock.*` for equipment/content availability and run-level player options.
- Use `Invader.Upgrade.*` only for character-owned acquired upgrade state, not for shared track offers.

## Mutation APIs (C++)

Save-owned progression APIs on `UARSaveSubsystem`:
- `GetGameProgressionTags()`
- `HasGameProgressionTag(Tag)`
- `AddGameProgressionTag(Tag)`
- `RemoveGameProgressionTag(Tag)`
- `GetPlayerProgressionTags(PlayerState, OutTags)`
- `HasPlayerProgressionTag(PlayerState, Tag)`
- `AddPlayerProgressionTag(PlayerState, Tag)`
- `RemovePlayerProgressionTag(PlayerState, Tag)`
- `GetCharacterProgressionTags(CharacterTag, OutTags)`
- `HasCharacterProgressionTag(CharacterTag, Tag)`
- `AddCharacterProgressionTag(CharacterTag, Tag)`
- `RemoveCharacterProgressionTag(CharacterTag, Tag)`
- `GetFactionClout()`
- `SetFactionClout(NewClout)`

Unlock mutation normally flows through replicated GameState (`AARGameStateBase`) helpers and then save persistence:
- `AddUnlockTag`
- `RemoveUnlockTag`
- `SetUnlocksFromSave`

When unlocks are changed at runtime, save should be marked dirty so normal autosave/manual save writes include the update.

## Interaction Context + Parley Boundary

Alien Ramen now builds a full interaction identity on `AARPlayerStateBase`:

- `BuildInteractionContext(AdditionalTransientTags, OutContext)`
- `GetCombinedInteractionTags(OutTags)`
- `GetCombinedInteractionTagsWithTransient(AdditionalTransientTags, OutTags)`

`FARInteractionContext::CombinedTags` intentionally contains:
- `GameProgressionTags`
- `PlayerProgressionTags`
- `CharacterProgressionTags`
- caller-supplied `TransientProgressionTags`
- projected `LoadoutTags`
- live ASC owned tags
- current speaker tags
- current player-slot / canonical character identity tags
- character-owned activated `Invader.Upgrade.*` tags

`CombinedTags` intentionally does not contain:
- shared `AARInvaderGameState` spicy-track offers
- shared slotted track availability that has not been bought yet

Parley stays generic. Alien Ramen widens the game/plugin boundary by passing this richer merged tag bucket into Parley's existing combined-tag input, while `PlayerTags` and `GameTags` stay narrow.

## Placed-Actor Unlock Reactivity

`UARUnlockReactiveComponent` provides a reusable placed-actor/Blueprint hook into `AARGameStateBase` unlock tags.

- Runtime truth still lives on `AARGameStateBase::GetUnlocks()` and its replicated delegates.
- The component binds `OnHydratedFromSave` and `OnUnlocksChanged`, then calls `RefreshFromGameState()` in `BeginPlay`.
- Refresh attempts delegate hookup again against the live world `AARGameStateBase`, so placed actors recover if they begin play before the GameState exists or if the active GameState instance changes later.
- Base unlock evaluation waits for `AARGameStateBase` to report that hydration completed before it evaluates non-empty `RequiredUnlockTags`, so startup actors do not false-lock against an empty pre-hydration unlock container.
- Base unlock gate uses `RequiredUnlockTags`:
  - empty `RequiredUnlockTags` => unlocked
  - otherwise unlocked when `GetUnlocks().HasAll(RequiredUnlockTags)` is true
- `RequiredUnlockTags` and `OrderedUpgradeTags` are both authored from `Progression.Game.Unlock.*`.
- Upgrade replay uses authored `OrderedUpgradeTags` and checks each tag independently against current unlocks.
- Blueprint listeners bind to the component's `OnLocked`, `OnUnlocked`, and `OnUpgrade` multicast events.
- `ARLog` now logs component bind/unbind, hydration waits, evaluated required tags, current unlocks, missing required tags, and applied locked/unlocked state to make authored gate debugging visible in normal logs.
- Refresh replay order is deterministic:
  1. `OnLocked` when base gate fails (no upgrade replay)
  2. `OnUnlocked` when base gate passes
  3. `OnUpgrade(UpgradeTag)` for each active authored upgrade tag, in authored order

## Default Seeding and Hydration

- Default unlock baseline comes from `UARLoadoutSettings::DefaultStartingUnlocks`.
- The configured baseline is applied directly during:
  - runtime gather before save write
  - authority GameState hydration
  - save sanitize/normalize on load
- Default unlock baseline should be authored under `Progression.Game.Unlock.*`.
- When `LoadGame(...)` runs while a gameplay world with an authoritative `AARGameStateBase` is already active, the save subsystem immediately replays the standard GameState hydration path so unlock-reactive actors re-evaluate against the newly loaded unlock set without waiting for map travel.

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

Combined-tag dialogue checks should read the game-side interaction context, not reconstruct their own partial picture. Speaker tags, loadout tags, character progression, transient routing tags, and live ASC tags all come from `AARPlayerStateBase`'s context builder at the Parley boundary.

## Faction Integration

Faction ranking and election read/save progression state:
- popularity modifier rules evaluate against save `GameProgressionTags`
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

- Add a new save-wide story gate: use `Progression.Game.*`.
- Add a new player-identity gate: use `Progression.Player.*`.
- Add a new character-only gate: use `Progression.Character.*`.
- Add a new runtime-only interaction gate: use `Progression.Transient.*`.
- Add a new equip/content gate: use `Progression.Game.Unlock.*`.
- If a feature must survive restart, ensure it writes to save-owned progression/unlocks and marks save dirty.
- If a feature is run-temporary only, keep it out of progression/unlocks and use runtime-only state.
