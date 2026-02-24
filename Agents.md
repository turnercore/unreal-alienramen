# Alien Ramen Project Notes

## Non-Negotiable Agent Rules

- Treat this file as the handoff contract for future agents. Keep it accurate, concise, and actionable.
- Update this file immediately after meaningful architecture, system-behavior, ownership, replication, API-surface, or tooling changes.
- Do not leave stale entries. If behavior changed, either update or remove the old statement in the same change.
- Prefer durable contracts over implementation noise:
- keep invariants, authority/replication rules, data/config sources, and cross-system dependencies
- avoid listing temporary details, obvious framework behavior, or verbose per-field dumps likely to rot
- Blueprint-only facts are first-class documentation requirements. If critical behavior/shape only exists in BP assets (for example struct variable contracts), record it here clearly.
- Record decisions, not guesses. If uncertain, verify in code before writing; if still uncertain, add a clearly labeled open question.
- Preserve intent and constraints for future work (what must stay true), not just what exists today.
- API exposure default: prefer Blueprint exposure for gameplay-facing utilities unless told otherwise.
- If exposure choice is unclear, ask before locking API surface.
- Blueprint API categories should be under `Alien Ramen|...` (or existing subsystem category path following that prefix).
- When you add a new subsystem or major flow, include:
    1. ownership/authority model
    2. configuration source (settings/data assets/tables)
    3. runtime entry points and expected lifecycle
    4. integration touchpoints with existing systems
    5. If you remove or deprecate a system, explicitly note current status and replacement direction.
    6. Keep wording specific enough that a new agent can act without rediscovery.
- Always try to compile after making changes.

## High-Level Architecture

- Core runtime module: `Source/AlienRamen`
- Editor tooling module: `Source/AlienRamenEditor`
- Invader authoring editor tab is implemented in `Source/AlienRamenEditor/Private/ARInvaderAuthoringPanel.*` and registered from `AlienRamenEditorModule.cpp`.
- Gameplay/content is Blueprint-heavy in `Content/CodeAlong/Blueprints`
- GAS modules enabled in `AlienRamen.Build.cs`: `GameplayAbilities`, `GameplayTags`, `GameplayTasks`

## Multiplayer Architecture Assumptions

- Game model is cooperative multiplayer.
- Networking model is server-authoritative.
- Primary host model is listen server.
- Local coop means couch coop (split screen and/or same screen) and should always be supported.
- LAN sessions and internet sessions are separate targets and both should be supported.
- Internet multiplayer should be treated as a first-class requirement: replication behavior and server-authoritative flow should be robust, predictable, and production-ready.
- Architecture decisions should default to multiplayer-safe patterns (authority checks, replication correctness, deterministic server-owned state flow) even for features that appear single-player at first.

## Blueprint-Only Contracts (Keep Updated)

- Use this section for critical contracts that are only visible in Blueprint assets (for example required struct variables, tag containers, save schema expectations, or BP-only lifecycle dependencies).
- If a C++ system depends on BP-only data shape, document the minimum required fields here.

## GAS Runtime Contract

- `AARPlayerStateBase` owns the authoritative ASC (`UAbilitySystemComponent`) and `UARAttributeSetCore`.
- Pawn (`AARShipCharacterBase`) binds as ASC avatar (owner/avatar split, Lyra-style).
- Possession baseline flow (server): clear prior grants/effects/tags -> grant common ability set -> read `PlayerState.LoadoutTags` -> resolve content rows -> apply row baseline.
- Ability selection/matching is deterministic:
- exact tag match preferred over hierarchy match
- tie-break by ability level then stable order
- `DynamicSpecSourceTags` considered with asset tags

## Content Lookup System

- `UContentLookupSubsystem` resolves gameplay tags to DataTable rows via `UContentLookupRegistry` routes.
- Route resolution uses best root-prefix match; row name uses tag leaf segment.
- Registry source is Project Settings, not subsystem config:
- `Project Settings -> Alien Ramen -> Alien Ramen Content Lookup`
- settings class: `UARContentLookupSettings`
- property: `RegistryAsset` (default: `/Game/Data/DA_ContentLookupReg.DA_ContentLookupReg`)
- Runtime override still supported via `UContentLookupSubsystem::SetRegistry(...)` (takes priority).
- BP-facing subsystem APIs should stay under `Alien Ramen|Content Lookup`.

## State Serialization Contract

- Interface: `IStructSerializable` (`StructSerializable.h/.cpp`).
- Core methods:
- `ExtractStateToStruct`
- `ApplyStateFromStruct`
- `GetStateStruct`
- Default implementation uses `UHelperLibrary` reflection helpers (`ExtractObjectToStructByName`, `ApplyStructToObjectByName`).
- Implementers currently include:
- `AARPlayerStateBase`
- `AARGameStateBase`
- Implementers expose `ClassStateStruct` (`UScriptStruct*`) to declare persisted schema.
- Hydration is server-authoritative:
- interface default blocks non-authority actor execution
- PlayerState/GameState override forwards client calls via `ServerApplyStateFromStruct` RPC

## Enemy/Invader Runtime

- Enemies are `ACharacter`-based for movement/nav reliability (even if visuals are flying).
- `AAREnemyBase` is actor-owned ASC (owner/avatar = enemy), server-authoritative.
- Enemy death is one-shot and server-gated (`bIsDead`), with BP hooks:
- `BP_OnEnemyInitialized`
- `BP_OnEnemyDied`
- Invader authority brain: `UARInvaderDirectorSubsystem` (server-only `UTickableWorldSubsystem`).
- Replicated read model: `UARInvaderRuntimeStateComponent` on `GameState`.
- Invader data/config from DataTables and `UARInvaderDirectorSettings`.
- Player-down loss condition contract: `AreAllPlayersDown()` only evaluates non-spectator players whose ASC survivability state is initialized (`MaxHealth > 0`); players without initialized health are excluded (not treated as down) to avoid false loss on startup/load transitions.
- Offscreen cull contract: enemies are only eligible for offscreen culling after first gameplay entry (`AAREnemyBase::HasEnteredGameplayScreen()`), preventing false culls during valid offscreen entering trajectories.
- Spawn placement contract: authored spawn offsets are treated as in-bounds target formation positions; runtime offscreen spawn applies edge-based translation (Top/Left/Right) while preserving authored formation geometry, so non-side-edge waves can enter already arranged in formation.
- Wave phases in runtime flow: `Active`, `Berserk` (waves start `Active`, then time-transition to `Berserk`; waves clear when spawned enemies are dead and fully spawned).
- Entering behavior is per-enemy movement/state logic (on-screen/formation arrival), not a wave phase.
- `FARWaveDefRow::WaveDuration` controls `Active -> Berserk` timing.
- stage rows no longer include/use a berserk/wave-time multiplier; stage timing knobs do not scale per-wave active duration
- Enemy AI wave phase StateTree events use gameplay tags under `Event.Wave.Phase.*`:
- `Event.Wave.Phase.Active`
- `Event.Wave.Phase.Berserk`
- Enemy AI also emits one-shot entry event tag `Event.Wave.Entered` per enemy when entry condition is met.
- Added dedicated StateTree schema class `UAREnemyStateTreeSchema` (`AR Enemy StateTree AI Component`) for enemy AI authoring defaults:
- defaults `AIControllerClass` to `AAREnemyAIController`
- defaults `ContextActorClass` to `AAREnemyBase`
- Wave schema no longer includes formation-node graph data (`FormationNodes`, `FormationNodeId`) or `FormationMode`; formation behavior is driven by runtime AI/state + wave lock flags.
- Wave runtime spawn ordering is deterministic by `SpawnDelay`; equal-delay entries preserve authored `EnemySpawns` array order.
- Formation lock flags are authored at wave level (`FARWaveDefRow`), not per-spawn:
- `bFormationLockEnter` (lock during `Entering`)
- `bFormationLockActive` (lock during `Active`)
- Director applies these flags to each spawned enemy via `AAREnemyBase::SetFormationLockRules(...)`.
- Entered event dispatch to enemy StateTree is gated by entry-lock rules:
- when `bFormationLockEnter=false`, Entered event is dispatched once enemy first enters gameplay bounds
- when `bFormationLockEnter=true`, Entered event is dispatched once enemy reports formation-slot arrival (`SetReachedFormationSlot(true)`)
- `FARWaveDefRow` no longer carries `EntryMode`, `BerserkDuration`, `StageTags`, or wave-level `BannedArchetypeTags`.
- `FARWaveEnemySpawnDef` no longer carries `SlotIndex` or per-spawn formation lock flags.
- Formation slot index is runtime-only context on `AAREnemyBase` (`FormationSlotIndex`), assigned from deterministic spawn ordinal when the director spawns enemies.
- Spawn edge behavior contract:
- `Top` translates authored formation offscreen on `+X` while preserving formation geometry
- `Left`/`Right` translate authored formation offscreen on `Y` edges while preserving formation geometry
- wave-level random mirror options `bAllowFlipX`/`bAllowFlipY` can mirror authored offsets around gameplay-bounds center before offscreen translation
- default gameplay bounds are tuned for current invader debug camera extents: `X=[0,1400]`, `Y=[-1350,1550]`; default `SpawnOffscreenDistance=350`
- leak bound check uses low-X boundary (`Location.X >= GameplayBoundsMin.X`) for player-side loss detection in this coordinate layout
- Enemy runtime exposes entry-completion hook:
- `AAREnemyBase::SetReachedFormationSlot(bool)` (authority)
- `AAREnemyBase::HasReachedFormationSlot()`
- Enemy runtime exposes replicated lock state for AI/StateTree reads:
- `AAREnemyBase.bFormationLockEnter`
- `AAREnemyBase.bFormationLockActive`
- Director has stage-choice loop and overlap/early-clear spawning rules.
- Invader console commands are registered via `IConsoleManager::RegisterConsoleCommand(...)` and stored as `IConsoleObject*` handles in the subsystem; deinit must `UnregisterConsoleObject(...)` with null guards (no `FAutoConsoleCommand...` ownership) to avoid map-transition teardown crashes.
- Console controls implemented:
- `ar.invader.start [Seed]`
- `ar.invader.stop`
- `ar.invader.choose_stage <left|right>`
- `ar.invader.force_wave <RowName>`
- `ar.invader.force_phase <WaveId> <Active|Berserk>`
- `ar.invader.force_threat <Value>`
- `ar.invader.force_stage <RowName>`
- `ar.invader.force_intro <Seconds|clear>`
- `ar.invader.dump_state`

## Debug Save Tool (Current)

- Still active and used by runtime widget + editor tab, but expected to be replaced during upcoming C++ save-system refactor.
- Keep only currently necessary integration notes here; avoid expanding detailed schema/behavior docs for this legacy path unless needed for active work.

## Invader Authoring Editor Tool (Current)

- Tab name: `AR_InvaderAuthoringTool`, display name `Invader Authoring Tool`.
- Main menu entry: `Window -> Alien Ramen Invader Authoring`.
- Data source is direct DataTable authoring (no intermediate asset format):
- preferred source: `UARInvaderToolingSettings` (`Project Settings -> Alien Ramen -> Tooling`)
- fallback source: `UARInvaderDirectorSettings` when tooling table refs are unset
- wave rows: `FARWaveDefRow`
- stage rows: `FARStageDefRow`
- Tool supports row CRUD + save for both tables, with transaction-based edits and package dirtying.
- Row list supports multi-select duplicate/delete for wave/stage rows.
- Row list supports right-click context menu actions (`Rename`, `Duplicate`, `Delete`) for selected wave/stage rows.
- New-row creation uses rename textbox content as base name when present (with `_N` uniqueness suffix as needed).
- Table persistence now follows standard editor dirty-package flow by default: edits mark wave/stage DataTable packages dirty and rely on Unreal save prompts/manual save actions (no per-edit autosave churn).
- PIE save bootstrap settings are in `UARInvaderToolingSettings` (`Project Settings -> Alien Ramen -> Tooling -> Invader Authoring|PIE Save Bootstrap`):
- `bEnablePIESaveBootstrap`
- `PIEBootstrapLoadingMap`
- `PIELoadSlotName` (default `Debug`)
- `PIELoadSlotNumber` (default `0`)
- `PIEBootstrapDebugMap`
- When enabled, PIE harness starts on loading map, waits for PIE world, calls `GameInstance.LoadSave(SlotName, SlotNumber)` by reflection, then performs deferred debug travel.
- Deferred debug travel subscribes to GameInstance dispatcher `SignalOnGameLoaded` (when present) and opens the debug level on signal; fallback path uses load-complete bool function/property detection and short-delay timeout protection before `OpenLevel` to debug map (or editor default test map fallback).
- First tool-open backup snapshot support:
- when enabled (`bCreateBackupOnToolOpen`), the panel duplicates currently loaded wave/stage DataTables into `UARInvaderToolingSettings::BackupsFolder` (default `/Game/Data/Backups`) on first panel initialization
- backup creation is deferred briefly via ticker after panel construct to avoid early editor-startup validator registration warnings
- backup package saves use autosave-style save flags (`SAVE_FromAutosave`) to reduce validator churn/noise during automatic backup writes
- retention is capped per source table by `BackupRetentionCount` (oldest backups are pruned beyond the cap)
- backup assets are suffixed with timestamp (`<SourceAsset>__YYYYMMDD_HHMMSS`)
- Wave spawn selection is multi-select aware and synchronized between canvas + spawn list:
- list uses multi-selection
- canvas supports click select, ctrl-toggle, shift range-select (same layer), and drag-rectangle selection
- clicking empty canvas clears selection when not using additive modifiers
- canvas drag on a selected spawn moves the full selected group
- spawn context actions (delete, color set) apply to the current spawn selection set.
- Keyboard `Delete/Backspace` in wave mode only deletes selected spawn(s); it does not fall through to deleting the selected wave row when no spawns are selected.
- The panel listens to object transaction events for authored wave/stage tables and refreshes row/layer/spawn/details/issue views after undo/redo or other table transactions to keep UI state in sync.
- Wave authoring model is delay-layered:
- layers are unique `EnemySpawns[*].SpawnDelay` buckets (no extra persisted layer metadata)
- same-layer ordering is authored array order and should be treated as deterministic tie-break behavior
- Wave panel UX:
- top toolbar actions (`Reload Tables`, `Validate Selected`, `Validate All`) have explicit tooltips and separated layout
- `Add Layer` moved into `Wave Layers` header; `Add Spawn`/`Delete` moved into `Layer Spawns` header
- layer spawn list supports drag-drop reordering within a layer (same delay bucket)
- stages mode hides enemy palette + spawn details panels
- validation issues are in a collapsible panel and auto-collapse when empty
- Wave/stage/spawn authoring detail categories are flattened to `Wave`, `Stage`, and `Spawn` (no `AR|Invader|...` category-path nesting in the authoring details panels).
- Palette contract:
- scans Blueprint enemy classes under `UARInvaderToolingSettings::EnemiesFolder` (does not scan whole project)
- excludes base class `AAREnemyBase` and transient skeleton/reinst classes
- palette/list display names strip blueprint class noise (`BP_EnemyBase_` / `_C`) and render underscores as spaces
- applies class+color chips (Red/Blue/White) to spawned entries
- favorites persist in editor-per-project settings (`FavoriteEnemyClasses`)
- class-level preview shape cycle persists in editor-per-project settings (`EnemyClassShapeCycles`) and is used by wave-canvas glyph rendering (Square/Circle/Triangle/Diamond)
- palette uses star toggle glyphs for favorites (`★`/`☆`)
- palette row right-click opens a context menu with `Find in Content Browser`
- Editor settings source: `UARInvaderAuthoringEditorSettings` (`Config=EditorPerProjectUserSettings`):
- `DefaultTestMap` (default `/Game/Maps/Lvl_InvaderDebug`)
- `LastSeed`
- `FavoriteEnemyClasses`
- preview flags (`bHideOtherLayersInWavePreview`, `bShowApproximatePreviewBanner`)
- Validation is author-time only and currently checks missing references, wave/stage range issues, missing enemy classes, incompatible stage-wave tag constraints, and warns on non-enforced archetype tags.
- PIE harness uses existing runtime console commands (`ar.invader.*`) and is intended to run as listen-server single-player by default.

## Logging

- Global category: `ARLog`
- Keep logs concise and decision-focused; avoid frame spam.
- Debug save prefixes in use:
- `[DebugSaveTool]`
- `[DebugSaveTool|IO]`
- `[DebugSaveTool|Validation]`

## Integration Notes (Keep in Mind)

- `ARLog` is exported for cross-module use: `ALIENRAMEN_API DECLARE_LOG_CATEGORY_EXTERN(...)`.
- Use `UToolMenus::TryGet()` pattern (not `UToolMenus::IsToolMenusAvailable()` in this engine branch).
- `AlienRamen.cpp` include order must keep `AlienRamen.h` first.
- UE 5.7 editor API compatibility for `ARInvaderAuthoringPanel`:
- include `FileHelpers.h` for both `FEditorFileUtils` and `UEditorLoadingAndSavingUtils`
- use `ULevelEditorPlaySettings` setters (`SetPlayNetMode`, `SetPlayNumberOfClients`, `SetRunUnderOneProcess`) instead of direct member access
- `AlienRamenEditor.Build.cs` must include `GameplayTags` because authoring code copies row structs containing `FGameplayTagContainer`.
