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

### Dialogue / NPC

- `UARDialogueSubsystem` is server-authoritative for dialogue offer selection, execution, mutation, completion, and choice memory.
- `UARNPCSubsystem` owns NPC talkable-state resolution/cache.
- `UARNPCTalkComponent` owns NPC-side dialogue interaction and replicated talkable-state fields.
- `UAREmotionComponent` owns replicated overhead emotion display state.
- `UAREmotionResolverSubsystem` owns shared emotion icon lookup/cache from configured emotion data.
- NPC actors do not own dialogue authority.
- Seen state is transient only; completion and recorded choice results are persistent.

Docs: `Documentation/README_DialogueNPC.md`

### Shop

- `UARCustomerComponent` is the authoritative NPC customer/order runtime.
- `AARShopDispenserActor` is the generic server-authoritative item dispenser surface.
- `AARShopStationActor` is server-authoritative for station state, processing progress, stock, and slot contents.
- `AARRamenBowlActor` enforces strict fill order: `Noodles -> Broth -> Toppings`.
- Station processing progress is replicated runtime-only state and is intentionally **not** save-persistent.

Docs: `Documentation/README_ShopRamenSystem.md`

### Faction

- `UARFactionSubsystem` owns election/voting runtime.
- Faction election is built on top of dialogue-owned faction/relationship surfaces and remains outside dialogue plugin ownership.

Docs: `Documentation/README_FactionSubsystem.md`

### Content Lookup

- `UTagContentResolverSubsystem` resolves gameplay tags to authored content through registry routes.
- Project Settings are the default registry source.

### Invader

- `UARInvaderDirectorSubsystem` is the server-only invader run authority.
- `AARInvaderGameState` owns replicated invader shared runtime state.
- Invader combat runtime should remain GAS-driven and server-authoritative.
- Director exposes replicated/read-model state rather than relying on client simulation.
- Spicy track / full blast is GameState-owned shared replicated runtime state.

Docs: `Documentation/CppOverview/InvaderSpicyTrack.md`

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
