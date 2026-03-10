# Alien Ramen Project Notes (Condensed)

## Purpose

This file is the **agent handoff contract** for the current architecture only.

It should contain:

- current invariants
- authority / replication ownership
- config sources
- major subsystem boundaries
- cross-system integration rules
- critical Blueprint-only contracts not visible in C++

If a detail is already true in code comments, Doxygen, or a dedicated doc, do not restate it here unless it is a cross-system invariant.

It should **not** become a changelog, implementation diary, or field dump.

---

## Anti-Bloat Rules

### Keep this file small

- Target **under 250 lines**.
- Prefer **short bullets** over paragraphs.
- Each section should describe **what must stay true**, not every implementation detail.
- If a topic needs more than ~6 bullets, move it to a dedicated doc and link it here.

### Only add information if it is one of these

- a rule that would cause bugs if forgotten
- an ownership / authority contract
- a save / replication / lifecycle contract
- a config source other systems depend on
- a Blueprint-only dependency C++ readers cannot see
- a replacement / deprecation that affects future work

### Do **not** add

- temporary debugging notes
- obvious Unreal behavior
- per-function summaries
- long API lists
- repetitive type/enum/property listings unless they are the contract
- “also remember” details that belong in code comments or docs pages
- historical notes unless a legacy path still exists and matters

### When updating this file

- Update it **in the same pass** as code changes.
- **Delete or rewrite** stale bullets immediately.
- Prefer replacing old bullets over appending new ones.
- If a detail already exists in a dedicated doc, link that doc instead of restating it here.
- If uncertain, verify in code first. If still uncertain, add a single `Open Question` bullet rather than guessing.

### Escalation rule

If a section starts growing again:

1. reduce it to invariants only
2. move mechanics/examples/API detail into a dedicated doc under `Documentation/`
3. leave only a one-line summary plus link here

---

## Core Project Contracts

- Engine version is **Unreal Engine 5.7**. Use UE 5.7 include order and tooling.
- Architecture is **server-authoritative multiplayer** with **listen server** as primary host model.
- **Local couch co-op** is a first-class requirement and must continue to work alongside LAN/online play.
- Default implementation bias is **multiplayer-safe** even for features that look single-player.
- Prefer **lean current-state code** over preserving obsolete pre-production paths unless explicitly requested.

---

## Critical Engineering Rules

- **Never** resolve Unreal/engine-dependent values in namespace/global static initialization. Resolve at runtime only. This avoids packaged `CrashDuringStaticInit` / `777006` failures.
- Prefer **forward declarations in headers** and move concrete includes to `.cpp` where possible.
- Shared enums/structs used across systems should live in focused shared `*Types.h` headers.
- Public headers belong in `Source/AlienRamen/Public`; implementations in `Private`.
- Blueprint-facing gameplay utilities should default to being exposed unless there is a clear reason not to.
- Blueprint categories should use the `Alien Ramen|...` prefix.
- Compile after meaningful changes before updating docs/notes.

---

## Documentation & Tooling

- Main docs live in `Documentation/` and are published with **MkDocs + Material**.
- API docs use **Doxygen** with config in `Doxyfile`.
- Keep code comments and docs updated in the same pass as runtime/editor changes.
- Prefer linking to detailed docs instead of duplicating detail here.
- When Unreal documentation is needed, prefer **UE 5.7** docs and use MCP/Context7 when available.

---

## High-Level Module Layout

- Runtime module: `Source/AlienRamen`
- Editor module: `Source/AlienRamenEditor`
- Core native GameInstance base: `UARGameInstance`
- Shared typed surfaces live in focused public headers such as player/color/pause/faction/dialogue/save/invader type headers

---

## Major Runtime Ownership

### GameInstance / Session

- `UARGameInstance` is the central runtime entry point for save/session subsystem access.
- `UARSessionSubsystem` owns native session lifecycle and should remain the Blueprint-facing networking surface.
- Do not couple gameplay/menu flows directly to backend-specific Blueprint session nodes.

### Save

- `UARSaveSubsystem` is the authoritative save/load/travel entry point.
- Save versioning is controlled in native code from `UARSaveGame`.
- Save system is **current-state only**; no pre-production backward-compat burden unless explicitly requested.
- Save and travel logic are subsystem-owned, not scattered across Blueprint flows.
- Canonical saves are blocked during active dialogue sessions.

### Dialogue Plugin Boundary

- Dialogue plugin ownership includes: dialogue runtime, speakers, dialogue editor tooling, conversations/lines, emotions, NPC dialogue/emotion components, dialogue-facing faction surfaces, and NPC relationship progression surfaces.
- Dialogue plugin ownership excludes built-on-top systems: faction voting/election orchestration and ordering/customer-serving loops.

### Dialogue / NPC

- `UARDialogueSubsystem` is server-authoritative for dialogue offer selection, execution, mutation, completion, and choice memory.
- `UARNPCSubsystem` owns NPC talkable-state resolution/cache.
- `UARNPCTalkComponent` owns NPC-side dialogue interaction and replicated dialogue talkable-state fields.
- `UAREmotionComponent` owns replicated overhead emotion display state (base state + dialogue override state), including per-player-slot variants and gameplay-tag-to-icon resolution.
- `UAREmotionResolverSubsystem` owns shared emotion icon lookup/cache from the settings-configured emotion DataTable.
- Emotion icon lookup for dialogue no longer depends on TagContentResolver routes; source table is configured directly in `UAREmotionSettings`.
- `AARNPCCharacterBase` hosts `UARNPCTalkComponent` and owns non-dialogue local NPC gates (for example customer/serving state); NPC actors still do not own dialogue authority.
- `AARNPCCharacterBase` and `AARPlayerCharacterBase` host `UAREmotionComponent`.
- Seen state is transient only; completion and recorded choice results are persistent.

### Faction

- `UARFactionSubsystem` election/voting runtime is built on top of dialogue-owned faction/relationship surfaces and remains outside dialogue plugin ownership.

### Tag Content Resolver

- `UTagContentResolverSubsystem` resolves gameplay tags to authored content through registry routes.
- Project Settings are the default registry source.

### Invader Runtime

- `UARInvaderDirectorSubsystem` is the server-only invader run authority.
- `AARInvaderGameState` owns replicated invader shared runtime state.
- Enemy/player combat runtime should remain GAS-driven and server-authoritative.

---

## Multiplayer / Authority Rules

- Server owns authoritative gameplay state mutation.
- Clients request actions through controller/server RPC entry points.
- Replicated GameState/PlayerState data is the canonical shared runtime model.
- Do not introduce client-authoritative shortcuts for gameplay state.
- World pause is resolved centrally by GameState, not ad hoc by random systems.

---

## Core Gameplay Contracts

### Player / Game State

- `AARPlayerStateBase` is the authoritative owner of player GAS state and replicated player runtime identity.
- `AARGameStateBase` is the authoritative shared world-state container for replicated run/save-facing state.
- Use built-in `PlayerArray` as authoritative player membership; do not create parallel replicated player lists.
- Slot, character, readiness, and other critical player runtime fields are native replicated contracts.

### Pause

- `AARGameStateBase` is the sole pause resolver.
- Effective pause is derived from player pause votes plus external reasons.
- Systems should set pause intent through GameState-owned paths, not direct `SetGamePaused` calls.

### Save-Facing World State

- Save-facing shared fields such as money, scrap, meat, unlocks, cycles, and active faction live as native GameState contracts.
- Save hydration and persistence are authority-only.

### Serialization

- `IStructSerializable` is the shared interface for state extraction/application.
- Struct-based state application is authority-only.

---

## Dialogue Contract Summary

- Dialogue is **server-authoritative**.
- Shop mode supports per-player sessions; Invader and Scrapyard use a shared session model.
- Line `SpeakerTag` may include an emotion leaf (for example `Dialogue.Speaker.Fred.Angry`) and is used for both speaker resolution and emotion icon fallback resolution.
- Important conversations/choices can force eavesdropping/participation rules.
- Offer selection uses primary speaker + gating + priority + repeatability policy.
- Completion persists only on completed-node execution.
- Choice-memory persists completed conversation results and can lock replay branches when authored to do so.
- Dialogue-applied emotion overrides are session-scoped and cleared when that session ends, revealing base emotion state again.

Detailed behavior belongs in:

- `Documentation/README_DialogueNPC.md`
- dialogue-specific runtime/editor docs

---

## Save Contract Summary

- Save files persist long-term progression and player/world canonical state, not every transient runtime detail.
- Revisions/backups are supported and pruned by configured retention rules.
- Travel can either persist to disk or carry one-shot pending runtime overlay state.
- Client save parity is synchronized from the server snapshot.

Detailed behavior belongs in:

- save subsystem docs
- save schema/type docs

---

## Invader Contract Summary

- Invader run flow is director-owned and server-authoritative.
- Enemies are `ACharacter`-based, GAS-driven, and resolved from authored definitions by identifier tag.
- Director state is exposed through replicated read models rather than client simulation.
- Spicy track / full blast is GameState-owned shared replicated runtime state.
- Drop spawning, kill credit, and shared-track progression are authoritative runtime flows.

Detailed behavior belongs in:

- `Documentation/CppOverview/InvaderSpicyTrack.md`
- invader runtime/editor docs

---

## Editor Tooling Summary

- Editor tooling lives in `AlienRamenEditor`.
- Important tools currently include:
    - Invader Authoring Tool
    - Enemy Authoring Tool
    - Dialogue Speaker Editor
    - Dialogue Conversation Graph Editor
    - Debug Save Tool
- Editor tools should author directly against current runtime contracts, not shadow them with alternate schemas.
- Dialogue Speaker Editor contract: no faction-based filter/sort surface; speaker/faction/portrait tags use gameplay-tag picker widgets, and portrait textures use an object asset picker (`UTexture2D` soft reference) instead of raw path text.
- Dialogue Speaker Editor conversation creation path is configured in Project Settings (`Alien Ramen -> Dialogue Tooling -> ConversationAssetsFolder`), defaulting to `/Game/Data/Conversations`.

---

## Blueprint-Only Contract Section

Only keep items here when they are **required for correctness** and are not visible from C++.
Examples:

- required BP-only struct fields
- required widget/controller lifecycle coupling
- required asset/tag/schema expectations

If a Blueprint-only fact becomes native, remove it from this section.

---

## Logging

- Global log category: `ARLog`
- Keep logs concise and decision-focused.
- Avoid spammy per-frame logging.
- Prefer warnings/errors only for actionable failure states.

---

## Required Additions For Any New Major System

When adding a new subsystem or major gameplay flow, document only:

- ownership / authority model
- config source
- runtime entry points / lifecycle
- replication/save implications
- integration points with existing systems
- replacement direction if it deprecates older behavior

If it needs more than that, write a dedicated doc and link it here.

---

## Open Questions

- Keep this section to **max 5 bullets**.
- Remove resolved items immediately.
- Do not leave speculative notes elsewhere in the file.
