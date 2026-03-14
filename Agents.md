# Alien Ramen Agent Handoff

## What this file is

This file is the **agent handoff contract** for the current project state.

Use it for only two things:

- instructions for future agents working in this repo
- cross-system contracts that are easy to break if forgotten

This file is **not** the main architecture doc, changelog, implementation diary, or API reference.

Detailed behavior belongs in `Documentation/` and should be maintained there.

---

## Agent Rules

- Read this file first, then use `Documentation/` for subsystem detail.
- Prefer the MkDocs docs as the source of detailed context; keep this file small.
- Update this file and the relevant docs **in the same pass** as code changes.
- Delete or rewrite stale bullets immediately; do not append contradictory notes.
- Record only:
    - ownership
    - authority
    - replication/save/travel rules
    - config sources other systems depend on
    - Blueprint-only correctness requirements not visible in C++
    - cross-system integration rules
- If a detail is local to one class/function/tool, it belongs in code comments or docs, not here.
- If a section grows beyond a few bullets, move detail into `Documentation/` and leave only a one-line summary plus link.
- If uncertain, verify in code first. If still uncertain, add one `Open Question` bullet instead of guessing.
- Prefer current-state architecture over preserving obsolete pre-production paths unless explicitly requested.

---

## Core Project Invariants

- Engine version is **Unreal Engine 5.7**.
- Architecture is **server-authoritative multiplayer**.
- Primary host model is **listen server**.
- **Local couch co-op** must continue to work alongside LAN/online play.
- Default implementation bias is **multiplayer-safe**, even for features that appear single-player.

---

## Critical Engineering Rules

- **Never** resolve Unreal/engine-dependent values in namespace or global static initialization. Resolve at runtime only.
- Prefer forward declarations in headers and move concrete includes to `.cpp` where possible.
- Shared enums/structs used across systems should live in focused shared `*Types.h` headers.
- Public headers belong in `Source/AlienRamen/Public`; implementations in `Private`.
- Blueprint-facing categories should use the `Alien Ramen|...` prefix.
- Blueprint-exposed developer surfaces (BlueprintCallable/Pure, BlueprintAssignable, editor-exposed properties/struct fields) should include `ToolTip` metadata, especially in Parley/Emo plugin APIs.
- Compile after meaningful changes before updating docs.

---

## Documentation Rule

- Detailed subsystem behavior belongs in `Documentation/`.
- MkDocs docs are the main detailed context source and must be kept current.
- Prefer linking to docs instead of restating detail here.
- When Unreal documentation is needed, prefer **UE 5.7** docs and use MCP / Context7 when available.

Key docs:

- `Documentation/README_SessionSubsystem.md`
- `Documentation/README_Persistence.md`
- `Documentation/README_FactionSubsystem.md`
- `Documentation/README_DialogueNPC.md`
- `Documentation/README_ProgressionUnlocks.md`
- `Documentation/README_ShopRamenSystem.md`
- `Documentation/README_ScrapyardMode.md`
- `Documentation/README_TransitionMode.md`
- `Documentation/CppOverview/InvaderSpicyTrack.md`

---

## Runtime Ownership

### Core State

- `AARPlayerStateBase` is the authoritative owner of player GAS state and replicated player runtime identity.
- `AARGameStateBase` is the authoritative shared world-state container for replicated run/save-facing state.
- Use built-in `PlayerArray` as authoritative player membership; do not create parallel replicated player lists.

### Session

- `UARGameInstance` is the central runtime entry point for save/session subsystem access.
- `UARSessionSubsystem` owns native session lifecycle and is the Blueprint-facing networking surface.
- Gameplay/menu flows must not depend directly on backend-specific Blueprint session nodes.
- Session/backend routing is settings-driven.

Docs: `Documentation/README_SessionSubsystem.md`

### Save / Travel

- `UARSaveSubsystem` is the authoritative save/load/hydration/travel entry point.
- `UARSaveGame` owns save schema versioning in native code.
- Save/travel logic must remain subsystem-owned, not scattered across Blueprints.
- Save files persist three ownership buckets: shared world state, player-owned state, and character-owned state.
- Canonical character identity is a gameplay tag; `EARCharacterChoice` remains a compatibility mirror for existing Blueprints.
- Canonical player slot identity is `AARPlayerStateBase::PlayerSlotTag` (`Player.Slot.P1/P2`); `EARPlayerSlot` remains a compatibility mirror.
- Character loadout is canonical character-owned state (`CharacterStates[].LoadoutTags`), not player-owned save data.
- `AARPlayerStateBase` is the runtime projection surface for the currently controlled character; character-owned save data hydrates onto `PlayerState`, not `GameState`.
- Hydration and state application are authority-only.
- Pending travel overlay state may carry between maps when not persisting to disk.
- Save-load gameplay entry should route through a standard transition context (`SaveLoad` / `SaveLoadEntry` / `bFreshLoadEntry=true`) so load-only restore logic can key off the same signal across maps.
- Canonical saves are blocked during active dialogue sessions.
- Save-facing `GameState` mutations (for example shared economy/faction fields) must leave the canonical save dirty so quit/leave autosaves can persist them.
- Shop loose carryables (energy drinks/meat) persist as transient save snapshots (`ShopTransientCarryables`) only for reload-before-run continuity.
- Run-start and post-run shop-entry flows clear transient loose-carryable snapshots via save-backed one-shot clear gate.
- Shop character transform/held-item restore is character-owned save data and only applies on fresh save-load re-entry into `Mode.Shop`; clean shop entries ignore it.
- First authoritative shop entry after a save still points at another mode/map should immediately persist a canonical shop save; scrapyard finalization should also commit a canonical save before travel back to shop.

### Dialogue / Speaker

- `UParleyDialogueSubsystem` is server-authoritative for dialogue offer selection, execution, mutation, completion, and choice memory.
- `UParleySpeakerSubsystem` owns speaker talkable-state resolution/cache.
- `UParleySpeakerComponent` owns speaker-side dialogue interaction and replicated talkable-state fields.
- `UEmoComponent` owns replicated overhead emotion display state.
- Parley conversation graph condition authoring is editor-only: `Branch` + `Check*` nodes persist in the editor graph, then compile down to existing runtime condition groups/switch routing; runtime does not execute the editor-only condition source nodes directly.
- Conversation graph redraw/open behavior is editor-graph authoritative (`EditorGraph`); editor tooling does not reconstruct authoring nodes from compiled runtime node data.
- Runtime overhead emotion rendering lives in `AEmoHUDBase` (Emo plugin) with `AARHUDBase` as the game-specific wrapper.
- `AARNPCCharacterBase` is a lean shell; speaker/emotion/customer behavior is component-driven and each component is optional per actor.
- `AARNPCCharacterBase::ForwardUseToController(AActor*)` is the optional BP forwarding helper for BI_Interactable-style flows; it resolves pawn/controller sources to `AARPlayerController` and routes to controller RPC interaction.
- `AARShopAIController` must restore speaker local dialogue gate open when `State.ShopNPC` tags are absent and during unpossess cleanup to avoid stale blocked talkability.
- `AARShopAIController` bridges shop-NPC dialogue lifecycle into StateTree events (`Event.ShopNPC.ConversationOffered`, `Event.ShopNPC.DialogueStarted`, `Event.ShopNPC.DialogueEnded`, `Event.ShopNPC.ConversationCompleted`) for animation/state transitions.
- `UEmoResolverSubsystem` owns shared emotion icon lookup/cache via TagKey route root `Dialogue.Emotion`.
- Speaker talkable refresh targets must come from dialogue runtime registered speaker tags (not synthesized speaker DataTable row-name tags).
- Dialogue-related settings pages are grouped under `Project Settings -> Alien Ramen` (`Dialogue`, `Dialogue Tooling`, `Emotion`, `Factions`).
- Speaker actors do not own dialogue authority.
- Seen state is transient only; completion and recorded choice results are persistent.
- Dialogue progression/completion/choice-memory persistence is character-owned and keyed by canonical character gameplay tag; game-completed conversations remain shared save state.
- Parley relationship runtime is a directed speaker matrix (`SourceSpeakerTag -> TargetSpeakerTag`); Alien Ramen bridge applies game-specific Brother/Sister mirroring so player-facing relationships remain shared.
- Dialogue cycle gating is character-owned: seen/skipped-this-cycle and per-speaker cycle offer counts (`MaxOffersPerCycle` on speaker rows) are persisted on character dialogue state.
- Dialogue progression resolution must prioritize live `PlayerState.CurrentCharacterTag` for the active slot/controller; slot-mapped cache is fallback only for detached/offline restore paths.
- `Dialogue.Speaker.Player` is a placeholder tag resolved by dialogue runtime to the active player's current character speaker tag (`Brother`/`Sister`) from `PlayerState` (`CurrentCharacterTag`, with slot fallback), and speaker-targeted relationship/faction checks/mutations should use that resolved tag.
- Parley `Signal` graph nodes are single-output passthrough nodes that broadcast `UParleyDialogueSubsystem::OnDialogueSignalFired` (`SignalTag`, optional payload tags, conversation/speaker/player-slot context); game systems should react in game layer listeners instead of embedding behavior in dialogue graphs.
- `UARParleySaveBridge` is the game-owned persistence adapter for Parley/ParleyFaction events; plugin events mark save dirty only and never force autosave directly.

Docs: `Documentation/README_DialogueNPC.md`

### Shop

- `UARCustomerComponent` is the authoritative customer/order runtime.
- Customer speaker identity is component-owned (`SpeakerTagOverride` or owning `UARSpeakerComponent` tag); customer DataTable rows are keyed by route tag/row name and do not store a separate identity tag field.
- Customer order UI style is component-authored via `UARCustomerComponent::OrderWidgetClass` (`UARCustomerOrderWidgetBase` subclass); runtime should use `CreateAndInitializeOrderWidget(...)` / `InitializeOrderWidget(...)` so widget binding stays delegate-driven from customer state.
- Shop NPC StateTree binding should use `AARNPCCharacterBase` cached actor bools (`bST_HasActiveOrder`, `bST_HasDialogueToSay`) for branch conditions; these are component-optional safe and refreshed from customer/speaker state changes.
- `AARShopDispenserActor` is the generic server-authoritative item dispenser surface.
- `AARShopPlayerController` owns shop-only interaction requests for carryables and stations (including `Pickup`/`Drop`/`Throw` plus station place/pickup/process/fill routes).
- Shop throw strength defaults to thrower GAS `Strength` mapping (`Strength * 100`) when `RequestShopThrowHeldCarryItem` is called with `ThrowStrength <= 0`.
- Actor-targeted interaction RPC requests on `AARPlayerController`/`AARShopPlayerController` must pass server-side reachability validation against the controller pawn (`ServerInteractionMaxDistance`) before authority gameplay mutation.
- `AARPlayerController` tracks active primary/secondary interaction targets (`ActiveInteractable`, `ActiveSecondaryInteractable`) plus shared latch state (`bIsInteracting`) for hold-style input flows; when both active targets clear (including out-of-range interruption), controller auto-clears `bIsInteracting`.
- Server-side controller tick re-validates active interaction targets at `ActiveInteractionRangeCheckInterval` and notifies opted-in interactables through `IARInteractableRangeListener::OnPlayerOutOfRange(...)` before clearing out-of-range targets.
- `AARPlayerController::OnInteractionActionCue` is the shared animation/UI cue stream for interaction outcomes (for example `Throw`, `Consume`, `Kick`, `Slap`); gameplay paths should emit cues via `NotifyInteractionActionCue(...)` when actions are successfully performed.
- `AARPlayerController::RequestKickActor(...)` emits `Kick` vs `Slap` cue using target height relative to pawn (`SlapCueMinHeightDeltaCm`) while keeping kick/slap physics behavior identical.
- `AARShopPlayerController::RequestShopUseOrDrop(AActor*)` is the preferred one-shot input entrypoint: forward-use valid targets, fallback drop when target is null.
- `AARShopPlayerController::RequestShopPickupCarryItem(nullptr)` is the no-hit fallback path and drops the currently held carry item when one exists.
- `AARShopPlayerController::RequestShopStationInteract(AARShopStationActor*)` is the smart station one-shot entrypoint: held bowl -> fill, held meat + empty slot -> place, empty hands + slotted meat -> pickup.
- Station processing supports both hold (`RequestShopStationStartProcessing`/`RequestShopStationStopProcessing`) and tap (`RequestShopStationTapProcessing`) input models.
- `AARShopStationActor` is server-authoritative for station state, processing progress, stock, and slot contents.
- `AARShopStationActor` also auto-slots loose world meat on station contact when the station can accept meat and its slot is empty.
- Manual/debug station authoring rule: if `Resolve Config from Data` is disabled and `RequiredUpgradeTags` is empty, station is treated as upgraded (no unlock dependency).
- Station processing input mode is station-configurable (`Hold`/`Tap`): in tap mode, each press consumes one pulse and release is required before the next pulse.
- Station processing is blocked when buffered stock already matches the incoming meat color (or incoming color is `None`); processing with existing stock is only allowed for a different non-`None` color swap.
- `AARShopCarryItemBase` is the shared lifecycle base for shop carryables (for example `AARRamenBowlActor` and `AARRamenMeatActor`).
- Held-item secondary actions route through `AARShopCarryItemBase::UseSecondaryByController(...)`; controller input should call `RequestUseSecondaryOnHeldCarryItem()` and let the held item decide behavior (default throw, item-specific overrides such as energy-drink consume).
- Carry-item world secondary actions route through `AARShopCarryItemBase::UseSecondaryInWorldByController(...)`; default behavior is a strength-scaled kick impulse (`Strength * 100`) for non-held world items.
- `AARShopCarryItemBase::ForwardSecondaryUseToController(AActor*)` is the BI-style held-secondary forwarding helper and only routes when the item is currently held by the interacting controller (throw/consume/etc via item override).
- `AARShopCarryItemBase::ForwardKickToController(AActor*)` is the BI-style world-item kick forwarding helper and routes to `AARPlayerController::RequestKickActor(...)`.
- Shop carryables expose shared `WeightKg` runtime (`0` = native primitive mass, `>0` = explicit mass override) so bowl/meat physics weight can be tuned per actor/Blueprint.
- Shop carryable actors replicate movement so held/drop/throw transforms remain server-authoritative across local + remote players.
- `AARRamenBowlActor` enforces strict fill order: `Noodles -> Broth -> Toppings`.
- `AARMeatStorageBoxActor` handles smart meat storage interaction: held meat + interact stores back to `GameState::Meat`; empty hands + interact dispenses from reserve.
- `AARRamenMeatActor` can auto-return to matching meat storage on world hit/overlap; `None`/unspecified meat is accepted by color-specific storage and deposited using the storage color. Auto-return still requires moving beyond storage-return arm distance (prevents instant re-store on spawn).
- Intentional player pickup of `AARRamenMeatActor` arms storage return, so throw-back interactions are not blocked by initial spawn-distance gating.
- Station processing progress is replicated runtime-only state and is intentionally **not** save-persistent.
- `AAREnergyDrinkCarryItem` is a shop carryable consumed through `AARShopPlayerController::RequestConsumeHeldEnergyDrink` and is valid only in `Mode.Shop`.
- `AAREnergyDrinkCarryItem` replicates `EnergyDrinkItemTag` so remote clients can resolve drink UI/content from world actors.
- Stored energy-drink inventory is authoritative before shop spawn; once spawned at shop anchors, drink instances are world-owned carryables until consumed/stored/sold by shop systems.
- Shop save-load restore for held items is intentionally limited to supported carryables (`AAREnergyDrinkCarryItem`, `AARRamenMeatActor`, `AARRamenBowlActor`) plus character transform; it is not a generic arbitrary actor persistence system.

Docs: `Documentation/README_ShopRamenSystem.md`

### Faction

- `UParleyFactionSubsystem` owns generic faction definitions, popularity, and faction-speaker reputation state/events.
- `UARFactionVotingSubsystem` owns AR election/vote runtime and applies winners to `AARGameStateBase` active faction state.
- Election/voting runtime is game-owned (AR layer) and builds on Parley faction data; Parley itself is vote-agnostic.
- Faction election finalization commits in transition flow (`AARTransitionGameMode`) for `Shop -> Invader` context; no shop-mode direct fallback path.

Docs: `Documentation/README_FactionSubsystem.md`

### Content Lookup

- `UTagKeySubsystem` resolves gameplay tags to authored content through registry routes.
- Project Settings are the default registry source.
- `UARItemDefinitionSubsystem` is the shared resolver facade for item/energy-drink definitions and shared item physics metadata; it delegates to `UTagKeySubsystem` and is consumed by both Shop and Scrapyard runtime paths.

### Invader

- `UARInvaderDirectorSubsystem` is the server-only invader run authority.
- `AARInvaderGameState` owns replicated invader shared runtime state.
- `AARInvaderPickupBase` is invader-pickup-only runtime base (not used by shop carryables).
- Invader combat runtime should remain GAS-driven and server-authoritative.
- Director exposes replicated/read-model state rather than relying on client simulation.
- Spicy track / full blast is GameState-owned shared replicated runtime state.
- Run-earned scrap/meat accrues in GameState run-ledger fields (not persistent storage) until shop re-entry deposit.
- Director owns all-players-downed loss end and unanimous early-bail vote resolution.

Docs: `Documentation/CppOverview/InvaderSpicyTrack.md`

### Transition

- `AARGameModeBase` owns optional transition-map travel routing for mode exits via `bRouteModeTravelThroughTransitionMap` + `TransitionTravelMapURL` + transition context (`TransitionSourceMode`, `TransitionReason`).
- `AARGameModeBase::TryStartTravel` accepts per-call `EARTravelRoutePolicy` override (`ModeDefault`, `ForceTransitionMap`, `ForceDirect`) so runtime can choose transition-map vs same-mode direct travel without mutating class defaults.
- `AARGameModeBase` provides `EndModeAndTravel(...)` and `TravelDirectInMode(...)` Blueprint helpers for explicit routing intent.
- `AARTransitionGameMode` owns transition-map continue gating and destination travel start.
- `AARTransitionGameState` owns replicated transition context (`FARTransitionContext`) for transition/result UI.
- `AARTransitionPlayerController` is the controller entrypoint for continue votes.
- `UARTransitionBlueprintLibrary` is the BP-safe builder/parser for transition travel URLs and context payloads.
- Transition mode is no-pawn by design; it should not spawn gameplay pawns.

Docs: `Documentation/README_TransitionMode.md`

### Scrapyard

- `AARScrapyardGameState` owns server-authoritative scrapyard timer/state, reserve/refund accounting, deterministic overspend trim, and reward grant finalization.
- `AARScrapyardGameState` also owns the replicated Scrapyard run-buff snapshot read model consumed by HUD/widgets.
- `AARScrapyardExitZoneActor` owns deposited-item + in-zone player tracking, replicated per-exit reserved scrap value, and reports reserve/refund deltas to Scrapyard GameState.
- `AARScrapyardHUD` is the local UI binding owner for Scrapyard runtime delegates (timer/summary/run-active + run-buff snapshot).
- `UARScrapyardHUDWidgetBase` and `UARScrapyardExitZoneWidgetBase` are reusable Blueprint-facing widget bridges for Scrapyard HUD state and per-exit reserved scrap state.
- `AARScrapyardCarryItemBase` overrides `ForwardUseToController` to route BI_Interactable-style use into `AARScrapyardPlayerController::RequestScrapyardPickupCarryItem` (not shop pickup).
- `AARScrapyardCarryItemBase` replicates item identity fields (`ScrapyardItemTag`, `FallbackScrapCost`) for remote inspect/UI paths.
- `AARScrapyardPlayerController::RequestUseSecondaryOnHeldCarryItem()` mirrors shop held-secondary dispatch and delegates behavior to held `AARShopCarryItemBase::UseSecondaryByController(...)` (default throw unless item override).
- Scrapyard hold-style interactions should use `AARPlayerController` active interaction tracking and optional `IARInteractableRangeListener` callbacks for server-authoritative out-of-range interruption.
- Negative scrap is allowed only for Scrapyard extraction accounting; finalization sets shared scrap to `0` before travel.
- Scrapyard budget starts as `ShopScrapStorage + RunLedgerScrap`; leftover finalized scrap is returned through run ledger for shop deposit.
- Scrapyard item definitions are TagKey-driven under `Scrapyard.Item`; energy-drink payload definitions are under `Scrapyard.EnergyDrink`.
- Scrapyard finalization defaults to `Scrapyard -> Transition -> Shop` using travel option context (`ARTrSource/ARTrReason/ARTrDest/ARTrFresh`).
- When `SpawnRuleSet` is set on `AARScrapyardGameMode`, scrapyard item spawn orchestration is GameMode-owned (Perlin noise + spawner weight + rarity budgets + `bAlwaysSpawn`). Managed flow only runs when the rule asset is set; leave it unset only for maps that should intentionally have no scrapyard spawns. Set spawner `bSpawnOnBeginPlay=false` when relying on managed flow. Docs: `Documentation/README_ScrapyardMode.md` and `Documentation/Assets/README_ScrapyardSpawnRules.md`.

Docs: `Documentation/README_ScrapyardMode.md`

### Temp Run Buffs

- `UARRunBuffSubsystem` owns save-backed temp-buff storage/queue/active state and authority mutation APIs.
- Run-buff ownership is keyed by loadout character gameplay tag; active payloads are per-character.
- Run-buff rotation happens at Invader initialization: remove previous active runtime payload, consume queued drinks into new active payload, apply once to player ASC state.
- Active payload re-application for late-join/respawn flows is handled from Invader + Scrapyard GameMode integration.
- Lifecycle split is fixed: queued save buffers clear at invader end, active payload survives through scrapyard, and queued+active clear on first shop entry.

---

## Cross-System Contracts

- Server owns authoritative gameplay-state mutation.
- Clients request actions through controller/server RPC entry points.
- Replicated `GameState` / `PlayerState` data is the canonical shared runtime model.
- World pause is resolved centrally by `AARGameStateBase`, not by ad hoc direct pause calls.
- Systems must not introduce client-authoritative shortcuts for gameplay state.
- Runtime caches that matter per world/instance must be subsystem-instance-owned, not translation-unit static state.
- `IStructSerializable` is the shared interface for extracting/applying state structs.
- Struct-based extraction/application is authority-only.
- Systems built on top of dialogue must use dialogue runtime APIs instead of directly mutating dialogue-owned save/emotion state.
- Editor tooling should author directly against runtime contracts, not shadow them with alternate schemas.

---

## Blueprint-Only Contract Section

Only keep items here when they are **required for correctness** and **not visible from C++**.

Valid examples:

- required BP-only struct fields
- required widget/controller lifecycle coupling
- required asset/tag/schema expectations
- required authored asset paths/settings that runtime assumes

Remove items from this section once they become native or are documented elsewhere clearly enough to stop being handoff-critical.

- Shop carryable actor Blueprints (for example bowl/meat) may keep `DefaultSceneRoot`, but must include at least one world-colliding `UPrimitiveComponent` so pickup/drop/throw physics can resolve correctly.

---

## Logging

- Global log category: `ARLog`
- Keep logs concise and decision-focused.
- Avoid spammy per-frame logging.

---

## Open Questions

- Keep this section to **max 5 bullets**.
- Remove resolved items immediately.
- Do not leave speculative notes elsewhere in the file.
