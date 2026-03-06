# Alien Ramen — Agent Guide

This file is the top-level operating contract for coding agents. Keep it short, current, and actionable.

## 1) Core Rules

- Treat this file as the primary handoff contract for future agents.
- Update this file immediately after meaningful architecture, authority/replication, ownership, lifecycle, API-surface, or tooling changes.
- Do not leave stale statements. If behavior changed, update or remove the old statement in the same change.
- Prefer durable contracts over implementation noise:
    - keep invariants, authority/replication rules, config sources, lifecycle entry points, and cross-system dependencies
    - avoid temporary details, obvious framework behavior, and verbose per-field dumps likely to rot
- Blueprint-only facts are first-class documentation requirements. If critical behavior only exists in BP assets, document the minimum required contract here or in linked docs.
- Record decisions, not guesses. If uncertain, verify in code before writing. If still uncertain, add a clearly labeled open question.
- Favor lean current-state code over backward compatibility unless explicitly requested. Remove obsolete paths instead of maintaining dual systems during pre-production.
- Always try to compile after meaningful changes. Compile before updating docs.
- When changing runtime/editor systems, update docs in the same pass. Delete or rewrite stale docs rather than leaving contradictions.

## 2) High-Priority Unreal / Code Hygiene Rules

- Never initialize Unreal/engine-dependent values at namespace/global static init time.
    - Do not do things like `FGameplayTag::RequestGameplayTag`, `FPaths::*`, subsystem access, or asset loads in global/static initialization.
    - Resolve these at runtime via functions, subsystem/component initialization, constructor body, or function-local static accessors.
    - Reason: avoid packaged `CrashDuringStaticInit` (`777006`) failures.
- Prefer forward declarations in headers and move concrete `#include` dependencies to `.cpp` files wherever UHT/type requirements allow.
- When multiple systems consume the same enums/structs, extract them into focused shared type headers (for example `*Types.h`) instead of dragging large owner headers across module boundaries.
- Header include hygiene for larger subsystems: prefer `CoreMinimal.h` -> required engine headers -> project headers -> generated.h.
- Avoid blanket includes. Keep dependency graphs thin for faster builds.
- API exposure default: prefer Blueprint exposure for gameplay-facing utilities unless told otherwise.
- Blueprint API categories should be under `Alien Ramen|...` or an existing subsystem category path using that prefix.
- Avoid parameter/local names `Tags` on `AActor`-derived classes; this can shadow `AActor::Tags`. Prefer `InTags` or `TagContainer`.

## 3) Engine / Tooling Baseline

- Unreal Engine version contract: **UE 5.7**.
- When needed, use the MCP/doc lookup path to check **Unreal Engine 5.7** documentation specifically.
- Core runtime module: `Source/AlienRamen`
- Editor tooling module: `Source/AlienRamenEditor`

## 4) Project Architecture Defaults

- Game model is cooperative multiplayer.
- Networking model is **server-authoritative**.
- Primary host model is **listen server**.
- Local coop is a first-class requirement.
- Internet multiplayer is also a first-class requirement; default to multiplayer-safe patterns even when a feature looks single-player at first.

### Canonical mode naming

- `Invader`
- `Scrapyard`
- `Shop`
- `Lobby`

### Native base classes

- `UARGameInstance`
- `AARGameModeBase`
- `AARGameStateBase`
- `AARPlayerController`
- `AARPlayerStateBase`
- `AARPlayerCharacterBase`

## 5) Ownership / Authority Defaults

Use these defaults unless a subsystem contract explicitly says otherwise.

### PlayerState

Use `AARPlayerStateBase` for:

- replicated per-player runtime state
- replicated player identity/setup state
- player-owned ASC / core attributes
- player HUD-facing replicated state
- per-player Invader runtime state

### GameState

Use `AARGameStateBase` / mode GameState for:

- replicated shared match state
- shared mode-level runtime state
- save-facing shared runtime fields
- team/shared Invader systems

### GameMode

Use GameMode for:

- authority-only orchestration
- joins/leaves/setup normalization
- travel gating / pre-travel hooks
- server-only mode rules

### GameInstance / Subsystems

Use `GameInstance` or subsystems for:

- cross-map systems
- save/load orchestration
- global registries / lookups / long-lived services

### Clients

- clients may request actions through controller/server RPC entrypoints
- clients must not directly own authoritative gameplay mutation

## 6) Key Runtime Contracts

### GAS

- `AARPlayerStateBase` owns the authoritative player ASC and `UARAttributeSetCore`.
- Player HUD-facing attribute state is PlayerState-owned.
- Use replication-aware loose gameplay-tag APIs when server-side ASC loose tags must be visible to clients.

### Save / Hydration

- Save/load/hydration entrypoint is `UARSaveSubsystem`.
- Hydration is **server-authoritative**.
- PlayerState and GameState state application must not mutate runtime state from non-authority paths.
- Travel/save flow is C++-owned in the save subsystem.

### Content Lookup

- `UContentLookupSubsystem` resolves gameplay tags to DataTable rows via registry routes.
- Registry source is project settings via `UARContentLookupSettings`.
- Prefer tag-driven lookup over hard-coded table references when the subsystem already uses Content Lookup.

### Invader

- Invader director authority brain is `UARInvaderDirectorSubsystem`.
- Shared Invader spicy-track runtime is owned by `AARInvaderGameState`.
- Per-player Invader spicy runtime metadata lives on `AARPlayerStateBase`.
- Enemy kill-credit and spicy-track mutation are server-authoritative.

## 7) Source-of-Truth Docs

Read the corresponding systems documentation for the current source-of-truth on each subsystem. If you see contradictions between those docs and this file, verify in code and update the docs to match the current code behavior. Do not leave contradictions. You can find documentation in /Documentation/CppOverview or linked from the relevant subsystem header.

## 8) What to Document When Adding or Changing a System

When adding a new subsystem or major gameplay flow, document:

1. ownership / authority model
2. replication model
3. configuration source
4. runtime entry points and lifecycle
5. integration touchpoints with other systems
6. current status if replacing or deprecating something

Keep this specific enough that a new agent can act without rediscovery.

## 9) Agent Workflow Expectations

Before making non-trivial changes:

- inspect existing patterns in nearby code first
- prefer extending current architecture over inventing a parallel pattern
- check whether a shared type header already exists or should exist
- check docs for the subsystem you are modifying

After making non-trivial changes:

- compile if possible
- update public API comments / Doxygen where relevant
- update the subsystem doc
- update this file only if the top-level contract changed

## 10) Current High-Value Invariants

- Do not reintroduce legacy `RamenShop` naming in C++; `Shop` is canonical.
- Do not create parallel replicated player arrays when built-in `PlayerArray` is the authoritative source.
- Do not bypass server authority for save hydration, shared match state, or Invader spicy-track mutation.
- Do not shadow native PlayerState/GameState fields with duplicate Blueprint variables.
- Do not use true engine pause casually for multiplayer gameplay flows; prefer documented gameplay suspension models where appropriate.
