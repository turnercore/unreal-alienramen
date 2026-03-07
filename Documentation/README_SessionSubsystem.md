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
  - Steam invite acceptance is handled natively via OSS session invite delegates and routes into the same join path

The subsystem is a `UGameInstanceSubsystem`, so in Blueprint:
- `Get Game Instance Subsystem` -> `ARSessionSubsystem`

Per-user setting persistence:
- Seed defaults: `Config/DefaultGameUserSettings.ini`
- Runtime writes: `Saved/Config/<Platform>/GameUserSettings.ini`

Platform config note:
- `Config/Windows/WindowsEngine.ini` sets `DefaultPlatformService=Steam` for Windows builds.

## Blueprint API Surface

- `CreateSession(bUseLAN, OutResult)`
- `CreateSessionNamed(bUseLAN, SessionDisplayName, OutResult)`
- `FindSessions(bLANQuery, MaxResults, OutResult)`
- `CancelFindSessions(OutResult)`
- `JoinSessionByIndex(ResultIndex, OutResult)`
- `FindFriendSession(FriendUniqueNetId, OutResult)`
- `InviteFriendToSession(FriendUniqueNetId, OutResult)`
- `DestroySession(OutResult)`
- `RefreshJoinability(OutResult)`
- `AddLocalPlayer(OutResult)`
- `IsStayOfflineEnabled()`
- `SetStayOfflineEnabled(bEnabled, bOutRestartRecommended)`

Events:
- `OnCreateSessionCompleted`
- `OnFindSessionsCompleted`
- `OnFindFriendSessionCompleted`
- `OnInviteFriendCompleted`
- `OnCancelFindSessionsCompleted`
- `OnJoinSessionCompleted`
- `OnDestroySessionCompleted`
- `OnRefreshJoinabilityCompleted`
- `OnLocalJoinCompleted`

Async Blueprint exec-flow nodes (`OnSuccess` / `OnFailure`):
- `CreateSessionAsync(WorldContext, bUseLAN, SessionDisplayName)`
- `FindSessionsAsync(WorldContext, bLANQuery, MaxResults)`
- `JoinSessionByIndexAsync(WorldContext, ResultIndex)`
- `DestroySessionAsync(WorldContext)`

Session display-name policy:
- host path writes `ARLobbyName` and `SETTING_SESSIONKEY` into advertised session settings
- if `SessionDisplayName` is empty, runtime default is:
  1) current save slot base name (when a save is loaded)
  2) generated random slot-style name (save subsystem helper)
  3) fallback literal `AlienRamenLobby`
- find results expose this as `FARSessionSearchResultData.SessionDisplayName`

## Authority Enforcement

- `AARGameModeBase::PreLogin(...)` rejects online joins when:
  - player cap is already reached (2)
  - `Stay Offline` is enabled for online identities
- `AddLocalPlayer(...)` enforces the same 2-seat cap for couch-join.
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
- Use `AddLocalPlayer` for couch-join input handling; it enforces seat cap and updates joinability.
- Steam overlay friend-invite accept flow is handled by subsystem callback (`OnSessionUserInviteAccepted`) and attempts direct join; if an existing named session is active, subsystem destroys it first and then joins invite target.
- Manual `JoinSessionByIndex` also performs destroy-then-join if the local runtime is already in a different named session.
- Online (non-LAN) find now performs a retry without strict query filters when the first filtered search returns zero rows.
- Find completion semantics: transport/query success returns `bSuccess=true`; empty result sets use `ResultCode=SessionNotFound` with `Error="No sessions found."`.
- Session create/find execution path now uses AdvancedSessions C++ proxies when a local player controller is available (`UCreateSessionCallbackProxyAdvanced`, `UFindSessionsCallbackProxyAdvanced`) while preserving `UARSessionSubsystem` Blueprint API.
- Project plugin requirement for that proxy path: `AdvancedSessions` and `AdvancedSteamSessions` are enabled in `AlienRamen.uproject`.
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
