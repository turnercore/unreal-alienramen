# Alien Ramen Project Notes

## Non-Negotiable Agent Rules

- Treat this file as the handoff contract for future agents. Keep it accurate, concise, and actionable.
- Do not leave stale entries in this file. Delete or rewrite outdated rules rather than leaving contradictions or confusion.
- Record decisions, not guesses. If uncertain, verify in code before writing; if still uncertain, add a clearly labeled open question.
- Preserve intent and constraints for future work (what must stay true), not just what exists today.
- HIGH PRIORITY: never initialize Unreal/engine-dependent values at namespace/global static initialization time (for example `FGameplayTag::RequestGameplayTag`, `FPaths::*`, subsystem access, asset loads in static/global constructors). Resolve these at runtime via functions/instance lifecycle (`Initialize`, `BeginPlay`, constructor body, function-local static accessor) to avoid packaged `CrashDuringStaticInit` (`777006`) failures.
- Favor lean current-state code over backward compatibility unless explicitly requested; remove obsolete/legacy paths instead of maintaining dual systems during pre-production.
- Important: do not keep legacy/backward-compatibility code, tests, or data paths unless the user explicitly asks for them.
- Prefer forward declarations in headers and move concrete `#include` dependencies to `.cpp` files wherever UHT/type requirements allow.
- When multiple systems consume the same enums/structs, extract them into focused shared type headers (for example `*Types.h`) to avoid dragging large owner headers across module boundaries.
- Keep shared headers in `Source/AlienRamen/Public` and implementations in `Source/AlienRamen/Private`; `UHelperLibrary` now follows this (`Public/HelperLibrary.h`, `Private/HelperLibrary.cpp`) and callers should include `"HelperLibrary.h"` (no relative `../` includes).
- API exposure default: prefer Blueprint exposure for gameplay-facing utilities unless told otherwise.
- If exposure choice is unclear, ask before locking API surface.
- Non-plugin Blueprint API categories should be under `Alien Ramen|Category|...` to avoid conflicts with engine/editor categories and to make searching easier.
- Always try to compile after making changes. Compile before updating `Agents.md` and other documentation.
- When changing runtime/editor systems, update documentation in the `Documentation` folder in the same pass: add/refresh Doxygen comments on public APIs and keep MkDocs pages/nav accurate; delete or rewrite stale docs rather than leaving contradictions.
- Header include hygiene for large subsystems (TagKey, editor tools, etc.): prefer the order `CoreMinimal.h` -> needed engine headers -> project headers -> `generated.h`. Avoid blanket includes; keep dependency graphs thin for faster builds.
- When able and needed, use the MCP server to look up Unreal 5.7 documentation. Prefer Unreal 5.7 documentation specifically.
- GAS attribute ownership is hard-split by domain: shared attributes in `UARAttributeSetCore`, player-only attributes in `UARAttributeSetPlayer`, enemy-only drop/collision attributes in `UAREnemyAttributeSet`. Do not reintroduce compatibility aliases that expose player/enemy attributes through `Core`.

## Developer-Facing Annotation & Comment Guidelines

- Annotate anything developer-facing in editor or code, including variables, functions, Blueprint nodes, structs, classes, and config data, with clear comments describing intent, usage, and constraints.
- Assume the reader is a future version of yourself who may have forgotten details or context.
- If you had to stop and think, ask a question, or rediscover intent, add a comment.
- Prefer writing the contract before or alongside implementation: document the purpose, usage, and constraints of a function/variable/struct before the implementation details grow around it.
- Use Doxygen-style comments (`///` or `/** */`) for C++ APIs so they appear in generated API docs, and use `@param`, `@return`, and related tags where they improve clarity.
- Add clear editor-facing tooltips and metadata for Blueprint-exposed and config-facing fields so intent is visible in Unreal Editor without reading the source.
- Keep comments focused on intent, constraints, side effects, authority/replication expectations, lifecycle assumptions, and integration touchpoints. Do not clutter code with comments that restate the obvious.

## Documentation Reference

- `Documentation/README_GameModes.md`
- `Documentation/README_SharedSystems.md`
- `Documentation/README_SessionSubsystem.md`
- `Documentation/README_Persistence.md`
- `Documentation/README_FactionSubsystem.md`
- `Documentation/README_DialogueNPC.md`
- `Documentation/README_ProgressionUnlocks.md`
- `Documentation/README_ShopRamenSystem.md`
- `Documentation/README_ScrapyardMode.md`
- `Documentation/README_TransitionMode.md`
- `Documentation/README_Invader.md`

## Multiplayer Architecture Assumptions

- Game model is cooperative multiplayer.
- Networking model is server-authoritative.
- Primary host model is listen server.
- Local coop means couch coop (split screen and/or same screen) and should always be supported.
- LAN sessions and internet sessions are separate targets and both should be supported.
- Internet multiplayer should be treated as a first-class requirement: replication behavior and server-authoritative flow should be robust, predictable, and production-ready.
- Architecture decisions should default to multiplayer-safe patterns (authority checks, replication correctness, deterministic server-owned state flow) even for features that appear single-player at first.
- Prefer optimistic client-side flow with server reconciliation for responsive gameplay, but ensure server authority is the source of truth and edge cases are handled gracefully (for example, latency spikes, packet loss, and out-of-order messages).
