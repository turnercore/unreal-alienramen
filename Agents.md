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
- Always try to compile after making changes. Compile before updating Agents.md and other documentation.

## High-Level Architecture

- Core runtime module: `Source/AlienRamen`
- Editor tooling module: `Source/AlienRamenEditor`
- Native GameInstance base now exists: `UARGameInstance` (`Source/AlienRamen/Public/ARGameInstance.h`) for future central orchestration.
- `UARGameInstance` exposes `GetARSaveSubsystem()` and Blueprint lifecycle extension hooks:
  - `BP_OnARGameInstanceInitialized`
  - `BP_OnARGameInstanceShutdown`
- Native authoritative lobby/runtime bases:
- `AARGameModeBase` (`Source/AlienRamen/Public/ARGameModeBase.h`)
- `AARGameStateBase` (`Source/AlienRamen/Public/ARGameStateBase.h`)
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
- `AARPlayerStateBase::LoadoutTags` is the source-of-truth loadout container. Do not create/shadow a Blueprint variable named `LoadoutTags` on derived PlayerState BPs.
- PlayerState ownership migrated to C++ for lobby/runtime identity flags:
- `CharacterPicked` (`EARCharacterChoice`: `None`, `Brother`, `Sister`) is native replicated state on `AARPlayerStateBase` with server setter path (`SetCharacterPicked` / `ServerPickCharacter`) and change signal `OnCharacterPickedChanged`.
- `DisplayName` is native replicated state on `AARPlayerStateBase` with server setter path (`SetDisplayNameValue` / `ServerUpdateDisplayName`), mirrored into `PlayerName` (`SetPlayerName`), and change signal `OnDisplayNameChanged`.
- `bIsReady` is native replicated transient state on `AARPlayerStateBase` with server setter path (`SetReadyForRun` / `ServerUpdateReady`) and change signal `OnReadyStatusChanged`.
- `bIsSetup` is native replicated setup gate on `AARPlayerStateBase` with authority setter `SetIsSetupComplete(...)` and signal `OnSetupStateChanged`.
- `LoadoutTags` replicated change signaling is explicit: `OnRep_Loadout` now broadcasts `OnLoadoutTagsChanged`; server-side writes in C++ save/setup paths route through `SetLoadoutTags(...)` so server-local listeners also receive change notifications.
- `AARPlayerStateBase::InitializeForFirstSessionJoin()` (authority-only) is the first-join default initializer (non-seamless-travel path): resets `CharacterPicked` to `None` and ensures default loadout tags are present.
- `bIsReady` is explicitly non-persistent/transient across seamless travel handoff (resets false on `CopyProperties` target).
- `AARPlayerStateBase` now owns replicated player slot identity (`EARPlayerSlot`: `Unknown`, `P1`, `P2`) with authority setter `SetPlayerSlot(...)` and RepNotify signal `OnPlayerSlotChanged`.
- PlayerState attribute UI delegates (`OnCoreAttributeChanged`, `OnHealthChanged`, `OnMaxHealthChanged`, `OnSpiceChanged`, `OnMaxSpiceChanged`, `OnMoveSpeedChanged`) include both `SourcePlayerState` and `SourcePlayerSlot` directly (no separate `...WithSource`/`...WithSlot` variants).
- Server applies a default debug-safe loadout when `LoadoutTags` is empty (`Unlock.Ship.Sammy`, `Unlock.Gadget.Vac`, `Unlock.Secondary.Mine`) during `BeginPlay` and after server struct-state apply.
- Pawn (`AARShipCharacterBase`) binds as ASC avatar (owner/avatar split, Lyra-style).
- `UARAttributeSetCore` owns shared combat/survivability attributes for both players and enemies, including transient meta attribute `IncomingDamage`.
- Enemy-only attributes live in `UAREnemyAttributeSet` (v1: `CollisionDamage`), while shared attributes stay in `Core`.
- Possession baseline flow (server): clear prior grants/effects/tags -> grant common ability set -> read `PlayerState.LoadoutTags` -> resolve content rows -> apply row baseline.
- Ship loadout application only runs when possessed by a gameplay `AARPlayerController`; possession by any other controller logs an error (loud) and skips init, leaving abilities/stats absent until a proper gameplay controller possesses the pawn.
- Ship loadout application is server-deferred with short retries after possess when `LoadoutTags` are not yet available, to handle network/order races (remote joiners and late server loadout assignment).
- Ship runtime weapon tuning setup is C++-owned in `AARShipCharacterBase` (no required BP `_Init`): it applies/refreshes a primary fire-rate gameplay effect from `PrimaryWeaponFireRateEffectClass` using SetByCaller tag `Data.FireRate` from `UARWeaponDefinition::FireRate`, and tracks the active handle for cleanup/refresh.
- Player HUD-facing attribute contract is PlayerState-owned: `AARPlayerStateBase` exposes Blueprint-assignable signals for core attributes (`OnHealthChanged`, `OnMaxHealthChanged`, `OnSpiceChanged`, `OnMaxSpiceChanged`, `OnMoveSpeedChanged`) plus generic `OnCoreAttributeChanged(EARCoreAttributeType, NewValue, OldValue)`.
- PlayerState exposes Blueprint getters for HUD polling/snapshot (`GetCoreAttributeValue`, `GetCoreAttributeSnapshot`, `GetSpiceNormalized`) so HUD can read local and remote teammate stats from each replicated PlayerState.
- Spice meter control is PlayerState-owned and server-authoritative via `SetSpiceMeter` / `ClearSpiceMeter` (client calls route through `ServerSetSpiceMeter`); current spice is clamped to `[0, MaxSpice]` and written to GAS `Spice` attribute base.
- Player move-speed runtime flow is now C++/GAS-driven like enemies: `AARShipCharacterBase` binds to core `MoveSpeed` attribute changes and syncs `CharacterMovement.MaxWalkSpeed` and `MaxFlySpeed` from GAS on init and on every replicated/runtime update.
- Ability selection/matching is deterministic:
- exact tag match preferred over hierarchy match
- tie-break by ability level then stable order
- `DynamicSpecSourceTags` considered with asset tags
- Enemy damage routing is GAS-first:
- `AAREnemyBase::ApplyDamageViaGAS(...)` builds/apply instant `UAREnemyIncomingDamageEffect` with SetByCaller `Data.Damage`
- `UARAttributeSetCore::PostGameplayEffectExecute(...)` consumes `IncomingDamage` into shield/health using `DamageTakenMultiplier`
- `AAREnemyBase::TakeDamage(...)` now forwards into this GAS damage path (authority), rather than direct health subtraction.
- Loose-tag replication rule: when server mutates ASC loose gameplay tags that clients must read (mode/runtime/state tags), use replication-state-aware loose-tag APIs (`Add/RemoveLooseGameplayTags(..., EGameplayTagReplicationState::TagOnly)`) rather than non-replicated loose-tag mutation paths.

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
- Seamless travel handoff for PlayerState is C++-owned via `AARPlayerStateBase::CopyProperties`:
- extracts/applies struct state through `IStructSerializable`
- then explicitly carries critical replicated fields (`PlayerSlot`, `LoadoutTags`, `CharacterPicked`, `DisplayName`)
- marks destination setup complete (`bIsSetup=true`) for copied/continued players
- and explicitly resets transient readiness (`bIsReady=false`) on the destination PlayerState.

## Save Runtime Contract (C++ Cutover In Progress)

- Runtime save objects now exist in C++:
- `UARSaveGame` (`Source/AlienRamen/Public/ARSaveGame.h`)
- `UARSaveIndexGame` (`Source/AlienRamen/Public/ARSaveIndexGame.h`)
- `UARSaveSubsystem` (`Source/AlienRamen/Public/ARSaveSubsystem.h`) is the new `UGameInstanceSubsystem` entrypoint for save/load/list/create/delete and hydration requests.
- Save schema version is now manually controlled from a single native source:
- `UARSaveGame::CurrentSchemaVersion` (manual bump point)
- `UARSaveGame::MinSupportedSchemaVersion` (manual support floor for migrations)
- write paths stamp `SaveGameVersion` from `UARSaveGame::GetCurrentSchemaVersion()`.
- load path rejects unsupported versions and warns when loading older-but-supported versions (migration hook point).
- `UARSaveGame` preserves top-level save property names used by prior BP flow (`SeenDialogue`, `DialogueFlags`, `Unlocks`, `Choices`, `Money`, `Meat`, `Material`, `Cycles`, `SaveSlot`, `SaveGameVersion`, `SaveSlotNumber`, `LastSaved`, `PlayerStates`) to minimize hydration rewiring.
- BP hydration compatibility helpers are exposed on `UARSaveGame`:
- `GetGameStateDataInstancedStruct()`
- `GetPlayerStateDataInstancedStructByIndex(int32)`
- `FindPlayerStateDataBySlot(...)`
- `FindPlayerStateDataByIdentity(...)`
- Save identity is hybrid via `FARPlayerIdentity` (`PlayerSlot` + optional `UniqueNetIdString`, with legacy id/display name fields).
- Save player payload now stores native `CharacterPicked` enum (`EARCharacterChoice`) in `FARPlayerStateSaveData` (not string/name reflection).
- Save runtime keeps revisioned physical slot naming (`<SlotBase>__<Revision>`). Load path includes rollback behavior: if requested/latest revision fails to deserialize, older revisions are attempted in descending order.
- Save slot base-name generation is C++/subsystem-owned (`UARSaveSubsystem::GenerateRandomSlotBaseName`) using thematic word pools plus a numeric ticket; when uniqueness is requested, candidates are checked against existing index entries before selection.
- Save backup retention is user-configured via `UARSaveUserSettings` (`Config=GameUserSettings`, `MaxBackupRevisions`, default `5`, clamped `1..100`) and can be read/updated at runtime through `UARSaveSubsystem::GetMaxBackupRevisions` / `SetMaxBackupRevisions`.
- Save write paths prune old revision files per slot base after successful save (`SaveCurrentGame`) and canonical-client persist (`PersistCanonicalSaveFromBytes`) using the configured max backup count.
- Save validation policy is clamp-and-warn (`UARSaveGame::ValidateAndSanitize`), currently clamping negative scalar resource fields.
- GameState hydration precedence:
- `UARSaveSubsystem::RequestGameStateHydration` consumes one-shot `PendingTravelGameStateData` first when available.
- When pending travel data exists, it now applies the current save's GameState struct first as a baseline, then overlays travel data to avoid zeroing unrelated fields. If no pending travel exists, hydration falls back to `CurrentSaveGame` game-state struct.
- `AARGameStateBase::BeginPlay` (authority only) calls `RequestGameStateHydration(this)` automatically.
- Cycles progression is now save-owned (not GameState-owned). `GatherRuntimeData` copies cycles from the current save, not from GameState. Authority helper `IncrementSaveCycles(Delta, bSaveAfterIncrement, OutResult)` lives on `UARSaveSubsystem` for run-complete increments and optional immediate persistence.
- Travel/save flow is now C++-owned in `UARSaveSubsystem`:
    - Authority-only `RequestServerTravel(URL, bSkipReadyChecks, bAbsolute, bSkipGameNotify)` and `RequestOpenLevel(LevelName, bSkipReadyChecks, bAbsolute)` are callable from Blueprints.
    - Optional readiness gate (default on) requires every `AARPlayerStateBase` to have a non-`Unknown` `PlayerSlot`, a non-`None` `CharacterPicked`, and `bIsReady=true`. Use `bSkipReadyChecks` for menus/debug.
    - Before travel, subsystem captures `AARGameStateBase` into `PendingTravelGameStateData` (via `IStructSerializable::ExtractStateToStruct`) so it hydrates first on the next map.
    - Travel always saves via `SaveCurrentGame(..., bCreateNewRevision=true)`; if no slot exists it will auto-create one.
    - Travel/open enforce listen hosting by auto-appending `?listen` to URLs/options; both paths require authority (server/standalone) and log a warning instead of traveling on failure.
- Save conveniences:
    - `IncrementSaveCycles(Delta, bSaveAfterIncrement, OutResult)` (authority) updates the canonical progression counter; cycles are save-owned.
    - `GetTimeSinceLastSave(FTimespan& OutElapsed)`, `FormatTimeSinceLastSave(FText&)`, `GetLastSaveTimestamp(FDateTime&)` for UI-friendly timestamps.
    - Saves are guarded by `bSaveInProgress`; `OnSaveStarted` fires before disk write. Concurrent save requests log/return failure (`SaveCurrentGame`), so UI can safely gate spinners/buttons via `IsSaveInProgress`.
    - Save throttling via `MinSaveIntervalSeconds` (default 1s). Throttled attempts return `EARSaveResultCode::Throttled`.
    - Save result codes now surface in `FARSaveResult.ResultCode` (`Success`, `AuthorityRequired`, `NoWorld`, `InProgress`, `Throttled`, `ValidationFailed`, `NotFound`, `Unknown`) for cheaper BP error handling.
    - Autosave helper `RequestAutosaveIfDirty(bCreateNewRevision, OutResult)` only runs when `bSaveDirty` is true; `MarkSaveDirty` is exposed. Cycle increments and loadout writes set `bSaveDirty`; successful saves clear it.
- Save hydration entrypoints enforce authority on requesters:
- `RequestGameStateHydration` ignores non-authority requesters (verbose log) to preserve server-authoritative state mutation.
- `UARSaveSubsystem::TryHydratePlayerStateFromCurrentSave(...)` returns whether a matching player row was found/applied (identity first, optional slot fallback).
- PlayerState hydration no longer applies blank/default `FARPlayerStateSaveData` when no match exists (no accidental clobber of character/display/loadout on misses).
- Client-join save parity handshake is controller-initiated and subsystem-served:
- local non-authority `AARPlayerController::BeginPlay` sends `ServerRequestCanonicalSaveSync()`
- server routes through `UARSaveSubsystem::PushCurrentSaveToPlayer(...)`
- target client persists snapshot via `ClientPersistCanonicalSave(...)` -> `UARSaveSubsystem::PersistCanonicalSaveFromBytes(...)`
- if a join request arrives before the server has a current save, subsystem queues that controller request and flushes it automatically after the next successful load/save sets `CurrentSaveGame`
- Save subsystem utility accessors now expose current runtime save identity without BP class-casting: `HasCurrentSave()`, `GetCurrentSlotBaseName()`, `GetCurrentSlotRevision()`.
- Save listing supports parallel namespaces:
- `ListSaves(...)` reads/writes the canonical save index slot (`SaveIndex`).
- `ListDebugSaves(...)` reads/writes a separate debug index slot (`SaveIndexDebug`) so debug-save slot discovery is isolated from canonical runtime saves.
- Multiplayer persistence parity seam added:
- server save path serializes canonical save bytes and sends to remote clients via `AARPlayerController::ClientPersistCanonicalSave(...)`
- clients persist received canonical bytes through `UARSaveSubsystem::PersistCanonicalSaveFromBytes(...)`
- C++ debug save tooling class resolution now targets native save classes (`UARSaveGame`/`UARSaveIndexGame`) rather than BP class-path loading.

## GameMode/GameState Player Lifecycle

- `AARGameModeBase::HandleStartingNewPlayer_Implementation` owns authority-side join flow:
- resolves joined `AARPlayerStateBase`
- if `bIsSetup==false` (first session join, not seamless-travel copy), runs C++ first-join setup:
- assigns slot (`P1` first-free, else `P2`)
- attempts save hydration from server `UARSaveSubsystem::CurrentSaveGame` via `TryHydratePlayerStateFromCurrentSave`
- if no matching save identity row exists, initializes defaults via `InitializeForFirstSessionJoin()`
- preserves assigned join-time slot for session consistency after hydration
- resolves character choice conflicts (if hydrated choice is already taken by the other player, auto-switch to alternate `Brother/Sister`; fallback `None` if needed)
- marks setup complete
- then emits BP extension hook `BP_OnPlayerJoined`
- `AARGameModeBase::Logout` emits `BP_OnPlayerLeft`; player membership itself is maintained by built-in `PlayerArray` lifecycle.
- `AARGameStateBase` player tracking mirrors `PlayerArray` into replicated `Players`; `OnTrackedPlayersChanged` is emitted from `AddPlayerState` / `RemovePlayerState` and on repnotify.
- GameState UI hooks: replicated `CyclesForUI` set via authority-only `SyncCyclesFromSave(int32)`; hydration completion fires `OnHydratedFromSave`.
- PlayerState UI hooks: `IsTravelReady()` (slot + character + ready) and `OnTravelReadinessChanged` multicast fire on slot/character/ready changes (server + clients).
- GameMode helper: `TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify)` wraps readiness, save, and travel; logs blocking players and delegates travel to SaveSubsystem (listen enforced).
- `AARGameStateBase` provides BP convenience lookups for coop player access:
- `GetPlayerBySlot(EARPlayerSlot)` (direct P1/P2 resolution from `PlayerArray` player slots)
- `GetOtherPlayerStateFromPlayerState(...)`
- `GetOtherPlayerStateFromController(...)`
- `GetOtherPlayerStateFromPawn(...)`
- `GetOtherPlayerStateFromContext(...)` (accepts PlayerState/Controller/Pawn/UObject and resolves other player)

## Enemy/Invader Runtime

- Enemies are `ACharacter`-based for movement/nav reliability (even if visuals are flying).
- `AAREnemyBase` is actor-owned ASC (owner/avatar = enemy), server-authoritative.
- Enemy definition schema is `FARInvaderEnemyDefRow` + nested `FARInvaderEnemyRuntimeInitData` (no `StartingHealth`; health initializes from `MaxHealth` via attributes).
- `AAREnemyBase` defaults `CharacterMovement.bOrientRotationToMovement=false`; per-enemy child Blueprints can opt in when pilot-style steering rotation is desired.
- Enemy death is one-shot and server-gated (`bIsDead`), with BP hooks:
- `BP_OnEnemyInitialized`
- `BP_OnEnemyDied`
- Death release contract: after C++ death cleanup, enemy fires `BP_OnEnemyDied`, then `BP_OnEnemyPreRelease`, then calls `ReleaseEnemyActor()`; default `ReleaseEnemyActor` destroys actor and is the pool-replacement seam.
- Enemy spawn identity is tag-first: `FARWaveEnemySpawnDef::EnemyIdentifierTag` is authoritative at runtime; `EnemyClass` in spawn rows is legacy/migration-only.
- On possess (authority), enemy resolves definition from content lookup by identifier tag and applies base stats via `SetNumericAttributeBase`:
- core: `MaxHealth`, `Health` (from max), `Damage`, `MoveSpeed`, `FireRate`, `DamageTakenMultiplier`
- enemy set: `CollisionDamage`
- Enemy move-speed runtime flow is C++/GAS-driven: after init and on MoveSpeed attribute changes, `AAREnemyBase` syncs core `MoveSpeed` to `CharacterMovement.MaxWalkSpeed` and `MaxFlySpeed`.
- Enemy startup ability/effect layering on possess (authority) is deterministic and deduped:
-   1. `UARInvaderDirectorSettings::EnemyCommonAbilitySet` (global)
-   2. best archetype match from `UARInvaderDirectorSettings::EnemyArchetypeAbilitySets` using `RuntimeInit.EnemyArchetypeTag` (exact match preferred, then closest parent tag)
-   3. row `RuntimeInit.EnemySpecificAbilities` (non-DA per-enemy ability entries)
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
- Projectile runtime base exists in C++: `AARProjectileBase` (`Source/AlienRamen/Public/ARProjectileBase.h`).
- default behavior: if `bReleaseWhenOutsideGameplayBounds` is true, projectile evaluates XY gameplay bounds (`UARInvaderDirectorSettings::GameplayBoundsMin/Max`) and calls `ReleaseProjectile()` once it has remained offscreen for `OffscreenReleaseDelay` seconds.
- `EvaluateOffscreenRelease()` (BlueprintCallable) advances offscreen time using world delta seconds, so manual/BP calls preserve the same delay semantics as tick-driven evaluation.
- projectile cull delay defaults to project settings value `UARInvaderDirectorSettings::ProjectileOffscreenCullSeconds` (default `0.1s`) when `bUseProjectSettingsOffscreenCullSeconds` is true.
- offscreen checks run on authority only when `bOffscreenCheckAuthorityOnly` is true (default); set false for purely local/projectile cases.
- `OffscreenReleaseMargin` expands XY bounds padding before the offscreen timer starts.
- release path is override-friendly for pooling via `ReleaseProjectile` (`BlueprintNativeEvent`); default implementation destroys actor.
- Pickup runtime base exists in C++: `AARPickupBase` (`Source/AlienRamen/Public/ARPickupBase.h`).
- default behavior: if `bReleaseWhenOutsideGameplayBounds` is true, pickup evaluates XY gameplay bounds each tick and tracks offscreen time; once offscreen for `OffscreenReleaseDelay` seconds, it calls `ReleasePickup()`.
- pickup cull delay defaults to project settings value `UARInvaderDirectorSettings::PickupOffscreenCullSeconds` (default `0.1s`) when `bUseProjectSettingsOffscreenCullSeconds` is true.
- offscreen checks run on authority only when `bOffscreenCheckAuthorityOnly` is true (default); set false for client-only pickups.
- `OffscreenReleaseMargin` expands XY bounds padding before the offscreen timer starts.
- pickup release path is override-friendly for pooling via `ReleasePickup` (`BlueprintNativeEvent`); default implementation destroys actor.
- Spawn placement contract: authored spawn offsets are treated as in-bounds target formation positions; runtime offscreen spawn applies edge-based translation (Top/Left/Right) while preserving authored formation geometry, so non-side-edge waves can enter already arranged in formation.
- Director sets explicit per-enemy formation target world location at spawn (`AAREnemyBase::SetFormationTargetWorldLocation(...)`) derived from authored offset (+ optional flips) before offscreen edge translation.
- Wave phases in runtime flow: `Active`, `Berserk` (waves start `Active`, then time-transition to `Berserk`; waves clear when spawned enemies are dead and fully spawned).
- Entering behavior is per-enemy movement/state logic (on-screen/formation arrival), not a wave phase.
- `FARWaveDefRow::WaveDuration` controls `Active -> Berserk` timing.
- Wave color-swap contract: if `FARWaveDefRow::bAllowColorSwap` is true, each spawned wave instance rolls an independent 30% chance to swap Red<->Blue spawn colors (White unchanged); this is no longer tied to repeating the same wave row.
- `FARWaveDefRow` no longer contains `BerserkProfile` tuning fields (move-speed multiplier, fire-rate multiplier, behavior tags); berserk behavior is currently state/logic driven only.
- stage rows no longer include/use a berserk/wave-time multiplier; stage timing knobs do not scale per-wave active duration
- Stage intro timing is no longer owned by invader stage data/director flow; startup/intro pacing should be driven externally (for example GameMode/state orchestration) before calling `StartInvaderRun`.
- Wave/stage rows include authored enable toggles (`FARWaveDefRow::bEnabled`, `FARStageDefRow::bEnabled`); director selection skips disabled rows.
- Enemy AI wave phase StateTree events use gameplay tags under `Event.Wave.Phase.*`:
- `Event.Wave.Phase.Active`
- `Event.Wave.Phase.Berserk`
- Enemy AI emits one-shot enemy entry events for StateTree:
- `Event.Enemy.EnteredScreen` fires first time enemy is inside entered-screen bounds.
- `Event.Enemy.InFormation` fires first time enemy has reached authored formation slot while on screen.
- Added dedicated StateTree schema class `UARStateTreeAIComponentSchema` (`AR StateTree AI Schema`) for enemy AI authoring defaults:
- defaults `AIControllerClass` to `AAREnemyAIController`
- defaults `ContextActorClass` to `AAREnemyBase`
- `UARStateTreeAIComponent` now returns `UARStateTreeAIComponentSchema` from `GetSchema()`, so StateTree assets assigned for enemy AI must compile against that schema.
- Wave schema no longer includes formation-node graph data (`FormationNodes`, `FormationNodeId`) or `FormationMode`; formation behavior is driven by runtime AI/state + wave lock flags.
- Wave runtime spawn ordering is deterministic by `SpawnDelay`; equal-delay entries preserve authored `EnemySpawns` array order.
- Formation lock flags are authored at wave level (`FARWaveDefRow`), not per-spawn:
- `bFormationLockEnter` (lock during `Entering`)
- `bFormationLockActive` (lock during `Active`)
- Director applies these flags atomically with wave runtime context via `AAREnemyBase::SetWaveRuntimeContext(...)`.
- StateTree startup/initialization order is wave-context-driven:
- enemy AI controller enforces no auto-start on possess (stops running logic if needed) to avoid pre-context evaluation
- enemy runtime context assignment (`SetWaveRuntimeContext`) triggers `TryStartStateTreeForCurrentPawn()` once lock flags/slot/phase are already set
- controller start remains idempotent/ownership-guarded (`GetPawn()==InPawn && InPawn->GetController()==this`) and defers one tick while `WaveInstanceId == INDEX_NONE`
- StateTree start always defers at least one tick after a valid context to allow enemy definition/effects/tags to finish applying before logic runs.
- Enemy AI StateTree startup is idempotent per possession: controller skips redundant `StartStateTreeForPawn(...)` calls when pawn changed or logic is already running, preventing `SetStateTree` on running-instance warnings.
- Enemy AI controller only forwards wave/entry StateTree events once logic is running; pre-start events are dropped to avoid `SendStateTreeEvent`-before-start warnings.
- After StateTree startup, controller resends the pawn's current runtime context (`WavePhase`, entered-screen, in-formation) so dropped pre-start events do not leave StateTree out of sync (for example missing Berserk transition).
- Formation lock flags (`bFormationLockEnter`, `bFormationLockActive`) are set by director at spawn via `AAREnemyBase::SetWaveRuntimeContext(...)` and remain readable from actor context in StateTree.
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
- Runtime loose-tag application/removal on server (ship + enemy init/runtime paths) uses ASC replication-state-aware loose-tag APIs (`EGameplayTagReplicationState::TagOnly`) so client-side ASC tag checks see the same mode/runtime tags.
- Damage helper API contract: `ApplyDamageViaGAS(...)` on enemy/ship outputs current Health after application (same function, no separate result variant).
- Enemy facing helper API:
- `UAREnemyFacingLibrary::ReorientEnemyFacingDown(...)` (`Alien Ramen|Enemy|Facing`) can be called from BP movement/collision responses to snap an enemy back to straight-down progression yaw (with optional settings offset).
- Enemy ASC state-tag bridge helpers exist (authority only, ref-counted):
- `AAREnemyBase::PushASCStateTag` / `PopASCStateTag`
- `AAREnemyBase::PushASCStateTags` / `PopASCStateTags`
- controller forwarding helpers (useful for explicit StateTree task wiring):
- `AAREnemyAIController::PushPawnASCStateTag` / `PopPawnASCStateTag`
- `AAREnemyAIController::PushPawnASCStateTags` / `PopPawnASCStateTags`
- controller event forwarding helpers for BP/runtime:
- `AAREnemyAIController::SendStateTreeEvent(const FStateTreeEvent&)`
- `AAREnemyAIController::SendStateTreeEventByTag(FGameplayTag, FName Origin=NAME_None)`
- `AAREnemyAIController::GetEnemyStateTreeComponent()`
- enemy-side BP convenience wrappers:
- `AAREnemyBase::GetEnemyAIController()`
- `AAREnemyBase::GetEnemyStateTreeComponent()`
- `AAREnemyBase::SendEnemyStateTreeEvent(const FStateTreeEvent&)`
- `AAREnemyBase::SendEnemyStateTreeEventByTag(FGameplayTag, FName Origin=NAME_None)`
- pawn->controller signal bridge (fact reporting; controller remains decision owner):
- `AAREnemyBase::SendEnemySignalToController(FGameplayTag SignalTag, AActor* RelatedActor=nullptr, FVector WorldLocation=FVector::ZeroVector, float ScalarValue=0.f, bool bForwardToStateTree=true)`
- `AAREnemyAIController::ReceivePawnSignal(...)`
- `AAREnemyAIController::BP_OnPawnSignal(...)` (optional BP hook)
- Enemy AI now uses `UARStateTreeAIComponent` (subclass of `UStateTreeAIComponent`) which computes active StateTree state-tag set and emits tag deltas.
- `AAREnemyAIController` subscribes to those deltas and automatically mirrors active StateTree state tags onto pawn ASC loose tags (pop removed, push added).
- Active-path semantics apply: if both parent and child states are active and tagged, both tags are present on ASC.
- Avoid double-wiring: if using this automatic bridge, do not also push/pop the same tags manually in StateTree tasks.
- Automation coverage exists for the mirror bridge in `Source/AlienRamen/Private/Tests/AREnemyAIStateTagBridgeTest.cpp` (`AlienRamen.AI.StateTree.ASCStateTagBridge`), validating add/dedupe/remove behavior from StateTree active-tag deltas to enemy ASC loose tags.
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
- `ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`
  - deprojects viewport corners at runtime, intersects with horizontal plane `Z=PlaneZ` (default `SpawnOrigin.Z`), logs suggested `GameplayBoundsMin/Max`, and optionally applies+saves when `apply` is provided

## Debug Save Tool (Current)

- Editor tab `AR_DebugSaveTool` now drives the C++ save system directly (`UARSaveSubsystem`) instead of the legacy `UARDebugSaveToolLibrary/Widget` (removed).
- Requires an active world/GameInstance (run PIE or a Play session); otherwise the tab reports that a subsystem is unavailable.
- Slot listing/creation/loading/deletion uses `UARSaveSubsystem::ListSaves/CreateNewSave/LoadGame/DeleteSave` and filters slots ending with `"_debug"`.
- New debug slot bases auto-append `"_debug"`; random names use `GenerateRandomSlotBaseName` when empty.
- Saving uses `SaveCurrentGame(CurrentSlotName, /*bCreateNewRevision=*/true)` to keep revision history aligned with runtime saves.
- Unlock-all action now writes directly to the loaded `UARSaveGame::Unlocks` with every `Unlock.*` gameplay tag discovered from the tag manager (no legacy library helper).
- Native meat save schema remains `FARMeatState` (`ARSaveTypes.h`) for meat edits/inspection via the property editor.

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
- wave panel visually groups summary/timeline and layer-spawn table regions with bordered section containers and subtle background tinting to improve section separation
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
