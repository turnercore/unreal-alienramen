# Session Subsystem Guide (`UARSessionSubsystem`)

This document describes the runtime session orchestration contract and how to expand from Steam to additional online backends (for example EOS or console services) without Blueprint rewrites.

## Runtime Ownership

- Primary runtime API: `UARSessionSubsystem` (`Source/AlienRamen/Public/ARSessionSubsystem.h`)
- Settings source: `UARNetworkUserSettings` (`Config=GameUserSettings`)
- Current policy:
  - Global seat cap is 2 total players (local + online combined)
  - `Stay Offline` blocks host/find/join/advertise and online PreLogin
  - LAN flow prefers null subsystem
  - Online flow prefers configured default subsystem when non-null, then Steam fallback, then default/null fallback

The subsystem is a `UGameInstanceSubsystem`, so in Blueprint:
- `Get Game Instance Subsystem` -> `ARSessionSubsystem`

Per-user setting persistence:
- Seed defaults: `Config/DefaultGameUserSettings.ini`
- Runtime writes: `Saved/Config/<Platform>/GameUserSettings.ini`

## Blueprint API Surface

- `EnsureSessionForCurrentFlow(bPreferLAN, OutResult)`
- `FindSessions(bLANQuery, MaxResults, OutResult)`
- `JoinSessionByIndex(ResultIndex, OutResult)`
- `DestroySession(OutResult)`
- `RefreshJoinability(OutResult)`
- `RequestLocalPlayerJoin(OutResult)`
- `IsStayOfflineEnabled()`
- `SetStayOfflineEnabled(bEnabled, bOutRestartRecommended)`

Events:
- `OnEnsureSessionCompleted`
- `OnFindSessionsCompleted`
- `OnJoinSessionCompleted`
- `OnDestroySessionCompleted`
- `OnRefreshJoinabilityCompleted`
- `OnLocalJoinCompleted`

## Authority Enforcement

- `AARGameModeBase::PreLogin(...)` rejects online joins when:
  - player cap is already reached (2)
  - `Stay Offline` is enabled for online identities
- `RequestLocalPlayerJoin(...)` enforces the same 2-seat cap for couch-join.
- `AARGameModeBase` refreshes session joinability on player join/leave.

## Backend Expansion (EOS/Xbox/Other OSS)

## What stays the same (no BP change expected)

- UI/menu Blueprint calls continue using `UARSessionSubsystem`.
- Save/hydration flow remains unchanged.
- Seat cap and offline policy remain authoritative in GameMode/SessionSubsystem.

## What changes per backend

1. `DefaultEngine.ini` backend config:
   - set/override `DefaultPlatformService`
   - add backend-specific subsystem sections and net driver settings
2. Plugins/modules:
   - enable the backend OSS plugin(s)
   - keep/remove Steam plugin depending on target platforms
3. Platform auth/session policy:
   - required login flow, permissions, and service compliance for that backend

No gameplay Blueprint should need to switch from Steam-specific nodes when using this subsystem contract.

## Identity + Save Compatibility Across Backends

- Player save identity is provider-aware (`UniqueNetIdString` + `UniqueNetIdType`).
- Strict online identities require exact provider/id match for hydration.
- Local-only/null-style identities can use slot fallback.
- If two local players share one online identity, hydration prefers matching `PlayerSlot`.

This means moving from Steam to another backend does not require a new Blueprint save schema; identity matching remains backend-aware in C++.

## Operational Notes

- Call `RefreshJoinability` after major state transitions if needed (player joins/leaves already trigger server-side refresh).
- Use `RequestLocalPlayerJoin` for couch-join input handling; it enforces seat cap and updates joinability.
- `Stay Offline` is runtime best-effort; full backend deactivation may still require restart depending on subsystem startup behavior.
- Session lifetime is intended to survive normal map travel (listen-server flow) unless explicitly destroyed.
- Leave behavior:
  - remote client leave returns only that client to menu (host session continues)
  - host/standalone leave runs best-effort autosave-if-dirty except in `Mode.Invader`, then returns to menu

## Minimum Test Matrix When Adding a New Backend

1. Host flow creates a discoverable joinable session.
2. Join flow resolves connect string and travels correctly.
3. Seat cap: third player always rejected.
4. Offline toggle blocks host/find/join/remote joins.
5. Save hydration matches correct identity after rejoin.
