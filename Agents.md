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
- Compile after meaningful changes before updating docs.

---

## Documentation Rule

- Detailed subsystem behavior belongs in `Documentation/`.
- MkDocs docs are the main detailed context source and must be kept current.
- Prefer linking to docs instead of restating detail here.
- When Unreal documentation is needed, prefer **UE 5.7** docs and use MCP / Context7 when available.

Key docs:

- `Documentation/README_SessionSubsystem.md`
- `Documentation/README_FactionSubsystem.md`
- `Documentation/README_DialogueNPC.md`
- `Documentation/README_ProgressionUnlocks.md`
- `Documentation/README_ShopRamenSystem.md`
- `Documentation/README_ScrapyardMode.md`
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
- Save files persist canonical progression/player/world state, not every transient runtime detail.
- Hydration and state application are authority-only.
- Pending travel overlay state may carry between maps when not persisting to disk.
- Canonical saves are blocked during active dialogue sessions.
- Shop loose carryables (energy drinks/meat) persist as transient save snapshots (`ShopTransientCarryables`) only for reload-before-run continuity.
- Run-start and post-run shop-entry flows clear transient loose-carryable snapshots via save-backed one-shot clear gate.

### Dialogue / Speaker

- `UARDialogueSubsystem` is server-authoritative for dialogue offer selection, execution, mutation, completion, and choice memory.
- `UARSpeakerSubsystem` owns speaker talkable-state resolution/cache.
- `UARSpeakerComponent` owns speaker-side dialogue interaction and replicated talkable-state fields.
- `UAREmotionComponent` owns replicated overhead emotion display state.
- Runtime overhead emotion rendering is owned directly by `AARHUDBase` (`DrawHUD` + emotion projection/occlusion helpers), not by a separate HUD component.
- `AARNPCCharacterBase` is a lean shell; speaker/emotion/customer behavior is component-driven and each component is optional per actor.
- `AARNPCCharacterBase::ForwardUseToController(AActor*)` is the optional BP forwarding helper for BI_Interactable-style flows; it resolves pawn/controller sources to `AARPlayerController` and routes to controller RPC interaction.
- `UAREmotionResolverSubsystem` owns shared emotion icon lookup/cache via TagContentResolver route root `Dialogue.Emotion`.
- Speaker talkable refresh targets must come from dialogue runtime registered speaker tags (not synthesized speaker DataTable row-name tags).
- Dialogue-related settings pages are grouped under `Project Settings -> Dialogue` (`Dialogue`, `Dialogue Tooling`, `Emotion`, `Factions`).
- Speaker actors do not own dialogue authority.
- Seen state is transient only; completion and recorded choice results are persistent.

Docs: `Documentation/README_DialogueNPC.md`

### Shop

- `UARCustomerComponent` is the authoritative customer/order runtime.
- Customer speaker identity is component-owned (`SpeakerTagOverride` or owning `UARSpeakerComponent` tag); customer DataTable rows are keyed by route tag/row name and do not store a separate identity tag field.
- `AARShopDispenserActor` is the generic server-authoritative item dispenser surface.
- `AARShopPlayerController` owns shop-only interaction requests for carryables and stations (including `Pickup`/`Drop`/`Throw` plus station place/pickup/process/fill routes).
- `AARShopPlayerController::RequestShopUseOrDrop(AActor*)` is the preferred one-shot input entrypoint: forward-use valid targets, fallback drop when target is null.
- `AARShopPlayerController::RequestShopPickupCarryItem(nullptr)` is the no-hit fallback path and drops the currently held carry item when one exists.
- `AARShopPlayerController::RequestShopStationInteract(AARShopStationActor*)` is the smart station one-shot entrypoint: held bowl -> fill, held meat + empty slot -> place, empty hands + slotted meat -> pickup.
- Station processing supports both hold (`RequestShopStationStartProcessing`/`RequestShopStationStopProcessing`) and tap (`RequestShopStationTapProcessing`) input models.
- `AARShopStationActor` is server-authoritative for station state, processing progress, stock, and slot contents.
- `AARShopStationActor` also auto-slots loose world meat on station contact when the station can accept meat and its slot is empty.
- Manual/debug station authoring rule: if `Resolve Config from Data` is disabled and `RequiredUpgradeTags` is empty, station is treated as upgraded (no unlock dependency).
- Station processing input mode is station-configurable (`Hold`/`Tap`): in tap mode, each press consumes one pulse and release is required before the next pulse.
- `AARShopCarryItemBase` is the shared lifecycle base for shop carryables (for example `AARRamenBowlActor` and `AARRamenMeatActor`).
- Shop carryable actors replicate movement so held/drop/throw transforms remain server-authoritative across local + remote players.
- `AARRamenBowlActor` enforces strict fill order: `Noodles -> Broth -> Toppings`.
- `AARMeatStorageBoxActor` handles smart meat storage interaction: held meat + interact stores back to `GameState::Meat`; empty hands + interact dispenses from reserve.
- `AARRamenMeatActor` can auto-return to matching meat storage on world hit/overlap, but only after it has moved beyond storage-return arm distance (prevents instant re-store on spawn).
- Station processing progress is replicated runtime-only state and is intentionally **not** save-persistent.
- `AAREnergyDrinkCarryItem` is a shop carryable consumed through `AARShopPlayerController::RequestConsumeHeldEnergyDrink` and is valid only in `Mode.Shop`.
- Stored energy-drink inventory is authoritative before shop spawn; once spawned at shop anchors, drink instances are world-owned carryables until consumed/stored/sold by shop systems.

Docs: `Documentation/README_ShopRamenSystem.md`

### Faction

- `UARFactionSubsystem` owns election/voting runtime.
- Faction election is built on top of dialogue-owned faction/relationship surfaces and remains outside dialogue plugin ownership.

Docs: `Documentation/README_FactionSubsystem.md`

### Content Lookup

- `UTagContentResolverSubsystem` resolves gameplay tags to authored content through registry routes.
- Project Settings are the default registry source.
- `UARItemDefinitionSubsystem` is the shared resolver facade for item/energy-drink definitions and shared item physics metadata; it delegates to `UTagContentResolverSubsystem` and is consumed by both Shop and Scrapyard runtime paths.

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

### Scrapyard

- `AARScrapyardGameState` owns server-authoritative scrapyard timer/state, reserve/refund accounting, deterministic overspend trim, and reward grant finalization.
- `AARScrapyardGameState` also owns the replicated Scrapyard run-buff snapshot read model consumed by HUD/widgets.
- `AARScrapyardExitZoneActor` owns deposited-item + in-zone player tracking, replicated per-exit reserved scrap value, and reports reserve/refund deltas to Scrapyard GameState.
- `AARScrapyardHUD` is the local UI binding owner for Scrapyard runtime delegates (timer/summary/run-active + run-buff snapshot).
- `UARScrapyardHUDWidgetBase` and `UARScrapyardExitZoneWidgetBase` are reusable Blueprint-facing widget bridges for Scrapyard HUD state and per-exit reserved scrap state.
- Negative scrap is allowed only for Scrapyard extraction accounting; finalization sets shared scrap to `0` before travel.
- Scrapyard budget starts as `ShopScrapStorage + RunLedgerScrap`; leftover finalized scrap is returned through run ledger for shop deposit.
- Scrapyard item definitions are TagContentResolver-driven under `Scrapyard.Item`; energy-drink payload definitions are under `Scrapyard.EnergyDrink`.
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
