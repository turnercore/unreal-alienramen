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
- ownership/authority model
- configuration source (settings/data assets/tables)
- runtime entry points and expected lifecycle
- integration touchpoints with existing systems
- If you remove or deprecate a system, explicitly note current status and replacement direction.
- Keep wording specific enough that a new agent can act without rediscovery.

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
- Wave phases: `Entering`, `Active`, `Berserk`, `Expired`.
- Enemy AI wave phase StateTree events use gameplay tags under `Event.Wave.Phase.*`:
- `Event.Wave.Phase.Entering`
- `Event.Wave.Phase.Active`
- `Event.Wave.Phase.Berserk`
- `Event.Wave.Phase.Expired`
- Wave schema no longer includes formation-node graph data (`FormationNodes`, `FormationNodeId`); formation behavior should be implemented via AI/state logic using runtime context (`FormationMode`, `SlotIndex`) until a dedicated formation system is reintroduced.
- Wave runtime spawn ordering is deterministic by `SpawnDelay`; equal-delay entries preserve authored `EnemySpawns` array order.
- Director has stage-choice loop and overlap/early-clear spawning rules.
- Invader console commands are registered via `IConsoleManager::RegisterConsoleCommand(...)` and stored as `IConsoleObject*` handles in the subsystem; deinit must `UnregisterConsoleObject(...)` with null guards (no `FAutoConsoleCommand...` ownership) to avoid map-transition teardown crashes.
- Console controls implemented:
- `ar.invader.start [Seed]`
- `ar.invader.stop`
- `ar.invader.choose_stage <left|right>`
- `ar.invader.force_wave <RowName>`
- `ar.invader.force_phase <WaveId> <Entering|Active|Berserk|Expired>`
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
- wave rows: `FARWaveDefRow` in `UARInvaderDirectorSettings::WaveDataTable`
- stage rows: `FARStageDefRow` in `UARInvaderDirectorSettings::StageDataTable`
- Tool supports row CRUD + save for both tables, with transaction-based edits and package dirtying.
- Wave authoring model is delay-layered:
- layers are unique `EnemySpawns[*].SpawnDelay` buckets (no extra persisted layer metadata)
- same-layer ordering is authored array order and should be treated as deterministic tie-break behavior
- Palette contract:
- auto-discovers `AAREnemyBase` subclasses (loaded/native + Blueprint generated classes)
- applies class+color chips (Red/Blue/White) to spawned entries
- favorites persist in editor-per-project settings (`FavoriteEnemyClasses`)
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
