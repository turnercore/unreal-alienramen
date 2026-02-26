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
- Enemy authoring editor tab is implemented in `Source/AlienRamenEditor/Private/AREnemyAuthoringPanel.*` and registered from `AlienRamenEditorModule.cpp`.
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
- `UARAttributeSetCore` owns shared combat/survivability attributes for both players and enemies, including transient meta attribute `IncomingDamage`.
- Enemy-only attributes live in `UAREnemyAttributeSet` (v1: `CollisionDamage`), while shared attributes stay in `Core`.
- Possession baseline flow (server): clear prior grants/effects/tags -> grant common ability set -> read `PlayerState.LoadoutTags` -> resolve content rows -> apply row baseline.
- Ability selection/matching is deterministic:
- exact tag match preferred over hierarchy match
- tie-break by ability level then stable order
- `DynamicSpecSourceTags` considered with asset tags
- Enemy damage routing is GAS-first:
- `AAREnemyBase::ApplyDamageViaGAS(...)` builds/apply instant `UAREnemyIncomingDamageEffect` with SetByCaller `Data.Damage`
- `UARAttributeSetCore::PostGameplayEffectExecute(...)` consumes `IncomingDamage` into shield/health using `DamageTakenMultiplier`
- `AAREnemyBase::TakeDamage(...)` now forwards into this GAS damage path (authority), rather than direct health subtraction.

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
- Enemy definition schema is `FARInvaderEnemyDefRow` + nested `FARInvaderEnemyRuntimeInitData` (no `StartingHealth`; health initializes from `MaxHealth` via attributes).
- `AAREnemyBase` defaults `CharacterMovement.bOrientRotationToMovement=false`; per-enemy child Blueprints can opt in when pilot-style steering rotation is desired.
- Enemy death is one-shot and server-gated (`bIsDead`), with BP hooks:
- `BP_OnEnemyInitialized`
- `BP_OnEnemyDied`
- Enemy spawn identity is tag-first: `FARWaveEnemySpawnDef::EnemyIdentifierTag` is authoritative at runtime; `EnemyClass` in spawn rows is legacy/migration-only.
- On possess (authority), enemy resolves definition from content lookup by identifier tag and applies base stats via `SetNumericAttributeBase`:
- core: `MaxHealth`, `Health` (from max), `Damage`, `MoveSpeed`, `FireRate`, `DamageTakenMultiplier`
- enemy set: `CollisionDamage`
- Enemy move-speed runtime flow is C++/GAS-driven: after init and on MoveSpeed attribute changes, `AAREnemyBase` syncs core `MoveSpeed` to `CharacterMovement.MaxWalkSpeed` and `MaxFlySpeed`.
- Enemy startup ability/effect layering on possess (authority) is deterministic and deduped:
- 1) `UARInvaderDirectorSettings::EnemyCommonAbilitySet` (global)
- 2) best archetype match from `UARInvaderDirectorSettings::EnemyArchetypeAbilitySets` using `RuntimeInit.EnemyArchetypeTag` (exact match preferred, then closest parent tag)
- 3) row `RuntimeInit.EnemySpecificAbilities` (non-DA per-enemy ability entries)
- `RuntimeInit.StartupAbilitySet` has been removed from enemy row schema/runtime.
- Startup loose tags/effects still come from enemy row runtime-init payload.
- Enemy identifier tag is replicated with notify (`OnRep_EnemyIdentifierTag`) and forwards to BP hook `BP_OnEnemyIdentifierTagChanged`; server `SetEnemyIdentifierTag` fires the same hook immediately.
- Enemy definition application flag `bEnemyDefinitionApplied` was removed; rely on BP hook `BP_OnEnemyDefinitionApplied` if you need lifecycle notifications.
- Enemy archetype is row-authored runtime data (`RuntimeInit.EnemyArchetypeTag`) replicated on `AAREnemyBase` (not hand-authored per enemy BP).
- Invader authority brain: `UARInvaderDirectorSubsystem` (server-only `UTickableWorldSubsystem`).
- Run control contract: `StartInvaderRun` is rejected when a run is already active (must stop first).
- Run stop contract: `StopInvaderRun` performs full cleanup (destroys managed run enemies, clears runtime wave/choice/cache state, resets run/stage/leak/threat counters, and pushes stopped snapshot).
- Replicated read model: `UARInvaderRuntimeStateComponent` on `GameState`.
- Invader data/config from DataTables and `UARInvaderDirectorSettings`.
- Director runtime no longer requires/loading the enemy DataTable to start runs; enemy definitions are resolved through `ContentLookupSubsystem` (registry asset) and cached per identifier tag. Enemy class preloading still uses the resolved definition.
- Director keeps an enemy definition cache keyed by identifier tag and preloads likely enemy classes ahead of time (`EnemyPreloadWaveLookahead`).
- Disabled enemy rows are hard-rejected from runtime spawn.
- Player-down loss condition contract: `AreAllPlayersDown()` only evaluates non-spectator players whose ASC survivability state is initialized (`MaxHealth > 0`); players without initialized health are excluded (not treated as down) to avoid false loss on startup/load transitions.
- Offscreen cull contract: enemies are only eligible for offscreen culling after first gameplay entry (`AAREnemyBase::HasEnteredGameplayScreen()`), preventing false culls during valid offscreen entering trajectories.
- Spawn placement contract: authored spawn offsets are treated as in-bounds target formation positions; runtime offscreen spawn applies edge-based translation (Top/Left/Right) while preserving authored formation geometry, so non-side-edge waves can enter already arranged in formation.
- Director sets explicit per-enemy formation target world location at spawn (`AAREnemyBase::SetFormationTargetWorldLocation(...)`) derived from authored offset (+ optional flips) before offscreen edge translation.
- Wave phases in runtime flow: `Active`, `Berserk` (waves start `Active`, then time-transition to `Berserk`; waves clear when spawned enemies are dead and fully spawned).
- Entering behavior is per-enemy movement/state logic (on-screen/formation arrival), not a wave phase.
- `FARWaveDefRow::WaveDuration` controls `Active -> Berserk` timing.
- stage rows no longer include/use a berserk/wave-time multiplier; stage timing knobs do not scale per-wave active duration
- Stage intro timing is no longer owned by invader stage data/director flow; startup/intro pacing should be driven externally (for example GameMode/state orchestration) before calling `StartInvaderRun`.
- Wave/stage rows include authored enable toggles (`FARWaveDefRow::bEnabled`, `FARStageDefRow::bEnabled`); director selection skips disabled rows.
- Enemy AI wave phase StateTree events use gameplay tags under `Event.Wave.Phase.*`:
- `Event.Wave.Phase.Active`
- `Event.Wave.Phase.Berserk`
- Enemy AI emits one-shot enemy entry events for StateTree:
- `Event.Enemy.EnteredScreen` fires first time enemy is inside entered-screen bounds.
- `Event.Enemy.InFormation` fires first time enemy has reached authored formation slot while on screen.
- Added dedicated StateTree schema class `UAREnemyStateTreeSchema` (`AR Enemy StateTree AI Component`) for enemy AI authoring defaults:
- defaults `AIControllerClass` to `AAREnemyAIController`
- defaults `ContextActorClass` to `AAREnemyBase`
- Wave schema no longer includes formation-node graph data (`FormationNodes`, `FormationNodeId`) or `FormationMode`; formation behavior is driven by runtime AI/state + wave lock flags.
- Wave runtime spawn ordering is deterministic by `SpawnDelay`; equal-delay entries preserve authored `EnemySpawns` array order.
- Formation lock flags are authored at wave level (`FARWaveDefRow`), not per-spawn:
- `bFormationLockEnter` (lock during `Entering`)
- `bFormationLockActive` (lock during `Active`)
- Director applies these flags to each spawned enemy via `AAREnemyBase::SetFormationLockRules(...)`.
- StateTree startup/initialization order: enemy AI controller defers StateTree start to next tick after possess so invader runtime context and wave lock flags are applied before logic begins.
- StateTree startup/initialization order: enemy pawn (`AAREnemyBase::PossessedBy`) explicitly triggers controller StateTree start after enemy possess-init has run, and controller start remains idempotent/ownership-guarded (`GetPawn()==InPawn && InPawn->GetController()==this`).
- Enemy AI StateTree startup is idempotent per possession: controller skips redundant `StartStateTreeForPawn(...)` calls when pawn changed or logic is already running, preventing `SetStateTree` on running-instance warnings.
- Enemy AI controller only forwards wave/entry StateTree events once logic is running; pre-start events are dropped to avoid `SendStateTreeEvent`-before-start warnings.
- Formation lock flags (`bFormationLockEnter`, `bFormationLockActive`) are set by director at spawn via `AAREnemyBase::SetFormationLockRules(...)` and remain readable from actor context in StateTree.
- `FARWaveDefRow` no longer carries `EntryMode`, `BerserkDuration`, `StageTags`, or wave-level `BannedArchetypeTags`.
- `FARWaveEnemySpawnDef` no longer carries `SlotIndex` or per-spawn formation lock flags.
- Formation slot index is runtime-only context on `AAREnemyBase` (`FormationSlotIndex`), assigned from deterministic spawn ordinal when the director spawns enemies.
- Spawn edge behavior contract:
- `Top` translates authored formation offscreen on `+X` while preserving formation geometry
- `Left`/`Right` translate authored formation offscreen on `Y` edges while preserving formation geometry
- wave-level random mirror options `bAllowFlipX`/`bAllowFlipY` can mirror authored offsets around gameplay-bounds center before offscreen translation
- Runtime enemy spawn facing is fixed to straight-down gameplay progression (toward low-X/player side) at spawn in `UARInvaderDirectorSubsystem`, with configurable `UARInvaderDirectorSettings::SpawnFacingYawOffset` for mesh-forward correction.
- default gameplay bounds are tuned for current invader debug camera extents: `X=[0,1400]`, `Y=[-1350,1550]`; default `SpawnOffscreenDistance=350`
- entered-screen detection uses inset bounds (`UARInvaderDirectorSettings::EnteredScreenInset`, default `40`) to avoid firing on first edge contact
- Leak detection ownership:
- leak detection/reporting is enemy-owned (BP/C++ enemy logic decides leak timing and calls director report API)
- Blueprint/manual trigger path should call `UARInvaderDirectorSubsystem::ReportEnemyLeaked(Enemy)` directly (for example from Earth trigger overlap).
- Director no longer performs boundary leak polling/destruction; it owns aggregate leak progression/loss rules and increments run `LeakCount` only via `UARInvaderDirectorSubsystem::ReportEnemyLeaked(...)` with per-enemy dedupe.
- Enemy runtime exposes entry-completion hook:
- `AAREnemyBase::SetReachedFormationSlot(bool)` (authority)
- `AAREnemyBase::HasReachedFormationSlot()`
- Enemy runtime exposes formation target context from director:
- `AAREnemyBase::SetFormationTargetWorldLocation(FVector)` (authority)
- `AAREnemyBase::GetFormationTargetWorldLocation()`
- `AAREnemyBase::HasFormationTargetWorldLocation()`
- Enemy runtime exposes replicated lock state for AI/StateTree reads:
- `AAREnemyBase.bFormationLockEnter`
- `AAREnemyBase.bFormationLockActive`
- Enemy color is replicated with notify (`OnRep_EnemyColor`) and forwards to BP hook `BP_OnEnemyColorChanged(EAREnemyColor)`; server-side `SetEnemyColor(...)` also triggers the same BP hook immediately.
- Enemy exposes Blueprint/StateTree-friendly ASC tag query helpers on actor context:
- `AAREnemyBase::HasASCGameplayTag(FGameplayTag)`
- `AAREnemyBase::HasAnyASCGameplayTags(FGameplayTagContainer)`
- Enemy facing helper API:
- `UAREnemyFacingLibrary::ReorientEnemyFacingDown(...)` (`Alien Ramen|Enemy|Facing`) can be called from BP movement/collision responses to snap an enemy back to straight-down progression yaw (with optional settings offset).
- Damage helper APIs exposed for BP/gameplay wiring:
- `AAREnemyBase::ApplyDamageViaGAS(float Damage, AActor* Offender)` (authority)
- `AARShipCharacterBase::ApplyDamageViaGAS(float Damage, AActor* Offender)` (authority)
- `AAREnemyBase::GetCurrentDamageFromGAS()`
- `AAREnemyBase::GetCurrentCollisionDamageFromGAS()`
- `AARShipCharacterBase::GetCurrentDamageFromGAS()`
- `AAREnemyBase::ApplyDamageToTargetViaGAS(AActor* Target, float DamageOverride=-1)` (authority; override < 0 uses current Damage)
- `AAREnemyBase::ApplyCollisionDamageToTargetViaGAS(AActor* Target, float DamageOverride=-1)` (authority; override < 0 uses current CollisionDamage)
- `AARShipCharacterBase::ApplyDamageToTargetViaGAS(AActor* Target, float DamageOverride=-1)` (authority; override < 0 uses current Damage)
- Both `GetCurrentDamageFromGAS` helpers read `UARAttributeSetCore::Damage` from ASC so callers can pass attacker-authored damage into the corresponding `ApplyDamageViaGAS`.
- GameState replicated leak read model:
- `UARInvaderRuntimeStateComponent::LeakCount` is replicated with RepNotify.
- `UARInvaderRuntimeStateComponent::OnEnemyLeaked` broadcasts on both server updates and client RepNotify updates with `(NewLeakCount, Delta)`.
- Director has stage-choice loop and overlap/early-clear spawning rules.
- Invader console commands are registered via `IConsoleManager::RegisterConsoleCommand(...)` and stored as `IConsoleObject*` handles in the subsystem; deinit must `UnregisterConsoleObject(...)` with null guards (no `FAutoConsoleCommand...` ownership) to avoid map-transition teardown crashes.
- `UARInvaderDirectorSubsystem::RegisterConsoleCommands()` now clears existing `ar.invader.*` registrations first, so PIE map travel cannot double-register global command names across overlapping world/subsystem lifetimes.
- Console controls implemented:
- `ar.invader.start [Seed]`
- `ar.invader.stop`
- `ar.invader.choose_stage <left|right>`
- `ar.invader.force_wave <RowName>`
- `ar.invader.force_phase <WaveId> <Active|Berserk>`
- `ar.invader.force_threat <Value>`
- `ar.invader.force_stage <RowName>`
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
- enemy table is also loaded for wave-spawn validation/summary resolution
- Tool supports row CRUD + save for both tables, with transaction-based edits and package dirtying.
- Row list supports multi-select duplicate/delete for wave/stage rows.
- Row list supports right-click context menu actions (`Rename`, `Duplicate`, `Delete`) for selected wave/stage rows.
- Row list right-click context menu includes `Enable`/`Disable` toggle for selected wave/stage rows; disabled rows render grayed with `[Disabled]` tag in the row list.
- New-row creation uses rename textbox content as base name when present (with `_N` uniqueness suffix as needed).
- Table persistence now follows standard editor dirty-package flow by default: edits mark wave/stage DataTable packages dirty and rely on Unreal save prompts/manual save actions (no per-edit autosave churn).
- PIE save bootstrap settings are in `UARInvaderToolingSettings` (`Project Settings -> Alien Ramen -> Tooling -> Invader Authoring|PIE Save Bootstrap`):
- `bEnablePIESaveBootstrap`
- `PIEBootstrapLoadingMap`
- `PIELoadSlotName` (default `Debug`)
- `PIELoadSlotNumber` (default `0`)
- `PIEBootstrapDebugMap`
- When enabled, PIE harness launches from the destination/debug map first (so PIE exit returns to that map), then performs a one-time runtime hop to the configured loading map before running save bootstrap and deferred travel back to destination/debug map.
- Before tool-driven PIE launch, the panel prompts save/checkout for a dirty currently loaded editor map package (even if already on startup map); cancelling save aborts launch.
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
- canvas supports spawn copy/paste (`Ctrl+C` / `Ctrl+V`) and spawn context-menu copy/paste; pasted spawns are offset and become the active selection
- spawn context actions (delete, color set) apply to the current spawn selection set.
- Enemy palette rows are click-select only (palette drag/drop spawning is disabled); canvas add-spawn uses the currently active palette class/color selection.
- Keyboard `Delete/Backspace` in wave mode only deletes selected spawn(s); it does not fall through to deleting the selected wave row when no spawns are selected.
- The panel listens to object transaction events for authored wave/stage/enemy tables and refreshes row/layer/spawn/details/issue views after undo/redo or other table transactions to keep UI state in sync.
- Enemy-table transactions now also force a palette rebuild (including compatibility cache reset) so newly authored enemy classes appear immediately.
- Wave authoring model is delay-layered:
- layers are unique `EnemySpawns[*].SpawnDelay` buckets (no extra persisted layer metadata)
- same-layer ordering is authored array order and should be treated as deterministic tie-break behavior
- Wave spawn identity authoring is tag-first (`EnemyIdentifierTag`); class path remains visible only as migration support.
- Selected wave spawn details include resolved enemy summary text only (no inline `Open Enemy Row` button).
- Wave panel UX:
- top toolbar actions (`Reload Tables`, `Validate Selected`, `Validate All`) have explicit tooltips and separated layout
- `Reload Tables` now also refreshes the enemy palette immediately.
- `Add Layer` moved into `Wave Layers` header; `Add Spawn`/`Delete` moved into `Layer Spawns` header
- layer spawn list supports drag-drop reordering within a layer (same delay bucket)
- wave canvas exposes authoring controls for `Snap To Grid` and `Grid Size`
- wave panel shows computed clear metrics for selected wave (`Damage to Clear` and required `DPS` over `WaveDuration`), using enemy row `MaxHealth`, optional reflected `MaxShield` when present, and `DamageTakenMultiplier` for approximation
- wave panel layout keeps `Snap To Grid` / `Grid Size` above the canvas; preview timeline + phase text + computed clear metrics render below the canvas
- wave canvas glyphs are intentionally larger for readability (approx 2x prior size), and authored spawn offsets are clamped to gameplay bounds during add/drag/paste/details-edit so spawns cannot drift outside canvas bounds
- stages mode hides enemy palette + spawn details panels
- validation issues are in a collapsible panel and auto-collapse when empty
- Wave/stage/spawn authoring detail categories are flattened to `Wave`, `Stage`, and `Spawn` (no `AR|Invader|...` category-path nesting in the authoring details panels).
- Palette contract:
- derives enemy classes from authored enemy DataTable rows (`EnemyDataTable -> FARInvaderEnemyDefRow::EnemyClass`), not folder-based asset discovery
- excludes base class `AAREnemyBase` and transient skeleton/reinst classes
- palette/list display names strip blueprint class noise (`BP_EnemyBase_` / `_C`) and render underscores as spaces
- applies class+color chips (Red/Blue/White) to spawned entries
- favorites persist in editor-per-project settings (`FavoriteEnemyClasses`)
- class-level preview shape cycle persists in editor-per-project settings (`EnemyClassShapeCycles`) and is used by wave-canvas glyph rendering (Square/Circle/Triangle/Diamond)
- palette uses star toggle glyphs for favorites (`★`/`☆`)
- palette row right-click opens a context menu with `Find in Content Browser` and `Edit Enemy Data` (opens Enemy Authoring for the resolved identifier tag)
- active palette selection has explicit visual feedback: selected enemy class row is highlighted and selected color chip (`R/B/W`) shows a highlight outline
- Editor settings source: `UARInvaderAuthoringEditorSettings` (`Config=EditorPerProjectUserSettings`):
- `DefaultTestMap` (default `/Game/Maps/Lvl_InvaderDebug`)
- `LastSeed`
- `FavoriteEnemyClasses`
- preview flags (`bHideOtherLayersInWavePreview`, `bShowApproximatePreviewBanner`, `bSnapCanvasToGrid`, `CanvasGridSize`)
- Validation is author-time only and currently checks missing references, wave/stage range issues, missing/invalid spawn enemy identifier tags, enemy-row resolution/disabled rows, incompatible stage-wave tag constraints, and warns on non-enforced archetype tags.
- PIE harness uses existing runtime console commands (`ar.invader.*`) and is intended to run as listen-server single-player by default.

## Enemy Authoring Editor Tool (Current)

- Tab name: `AR_EnemyAuthoringTool`, display name `Enemy Authoring Tool`.
- Main menu entry: `Window -> Alien Ramen Enemy Authoring`.
- Data source is direct DataTable authoring of enemy rows:
- preferred source: `UARInvaderToolingSettings::EnemyDataTable`
- fallback source: `UARInvaderDirectorSettings::EnemyDataTable`
- Enemy authoring panel treats row-struct mismatch as a hard guardrail: if the selected enemy DataTable row struct is not `FARInvaderEnemyDefRow`, the panel reports a status error and skips row parsing/population (prevents invalid row-memory reinterpret crashes).
- Supports row CRUD + duplicate + rename + delete + enable/disable + save, with transactions + dirty package flow.
- Enemy identifier tag is editor-normalized as source-of-truth key: every row must have `EnemyIdentifierTag`, and its leaf segment must exactly equal the DataTable row name.
- Enemy tool auto-syncs identifier tags on tool open/reload and row create/duplicate/rename/detail-edit by preserving existing tag parent path (or defaulting to `Enemy.Identifier`) and forcing leaf to row name.
- Enemy row list supports multi-select operations and right-click context menu actions (`Rename`, `Enable/Disable`, `Duplicate`, `Delete`), matching invader row-list interaction patterns.
- Enemy row list supports sortable columns (only): `Enabled`, `DisplayName`, `EnemyClass`, `MaxHealth`, `ArchetypeTag` (no row-name or identifier-tag sortable columns).
- Disabled enemy rows render visually muted and include `[Disabled]` in row display text.
- Includes details editor + validation issues panel with duplicate identifier-tag checks and runtime-init stat sanity checks.
- Exposes deep-link open/select by enemy identifier tag for wave-tool integration (`Open Enemy Row` from selected spawn summary).

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
- Avoid parameter/local names `Tags` on `AActor`-derived classes and related helpers; this shadows `AActor::Tags` and can fail builds under warning-as-error settings. Prefer names like `InTags` / `TagContainer`.
- UE 5.7 editor API compatibility for `ARInvaderAuthoringPanel`:
- include `FileHelpers.h` for both `FEditorFileUtils` and `UEditorLoadingAndSavingUtils`
- use `ULevelEditorPlaySettings` setters (`SetPlayNetMode`, `SetPlayNumberOfClients`, `SetRunUnderOneProcess`) instead of direct member access
- `AlienRamenEditor.Build.cs` must include `GameplayTags` and `GameplayAbilities` because authoring code reflects/copies row structs containing gameplay tags and `FARAbilitySet_AbilityEntry` (`TSubclassOf<UGameplayAbility>`).
