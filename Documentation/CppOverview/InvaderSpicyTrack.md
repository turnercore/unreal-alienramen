# Invader Spicy Track (`AARInvaderGameState`, `UARInvaderSpicyTrackSettings`)
Paths:
- `Source/AlienRamen/Public/ARInvaderGameState.h`
- `Source/AlienRamen/Public/ARInvaderSpicyTrackSettings.h`
- `Source/AlienRamen/Public/ARInvaderSpicyTrackTypes.h`

## Ownership and Authority
- Server-authoritative runtime in `AARInvaderGameState`.
- Shared track/full-blast state is replicated from GameState.
- Per-player runtime spicy metadata (color/combo/activated-upgrade ledger) lives on `AARPlayerStateBase`.
- System is runtime-only for Invader and is not persisted in save/hydration structs.

## Runtime Ownership Matrix
| State | Class | Authority | Replication | Notes |
| --- | --- | --- | --- | --- |
| `InvaderPlayerColor` | `AARPlayerStateBase` | Server | Replicated to all | Used for combo color matching. |
| `InvaderComboCount` | `AARPlayerStateBase` | Server | Replicated to all | Drives HUD combo display via `OnInvaderComboChanged`. |
| `LastInvaderKillCreditServerTime` | `AARPlayerStateBase` | Server | Not replicated | Timeout bookkeeping for combo reset. |
| `Spice` | `AARPlayerStateBase` (ASC `UARAttributeSetCore`) | Server | GAS attribute replication | Individual player meter value. |
| `MaxSpice` | `AARPlayerStateBase` (ASC `UARAttributeSetCore`) | Server (synced by Invader GS) | GAS attribute replication | Shared cap derived from `SharedFullBlastTier`. |
| `ActivatedInvaderUpgradeTags` | `AARPlayerStateBase` | Server | Replicated to all | "Has upgrade" ledger for claim/prereq logic. |
| `SharedTrackSlots` | `AARInvaderGameState` | Server | Replicated to all | Team-shared slotted upgrades. |
| `SharedFullBlastTier` | `AARInvaderGameState` | Server | Replicated to all | Tier progression (default max 5). |
| `FullBlastSession` (`bIsActive`, `Offers`, `RequestingPlayerSlot`, `ActivationTier`) | `AARInvaderGameState` | Server | Replicated to all | Authoritative offer session snapshot. |
| `OfferPresenceStates` (`PlayerSlot`, hovered offer, destination slot, cursor) | `AARInvaderGameState` | Server | Replicated to all | Live UI presence for both players during active offer session. |
| `ActiveSpiceSharers` | `AARInvaderGameState` | Server | Not replicated | Server tick loop membership for hold-to-share transfer. |
| `PredictedSpiceValue`, `bHasPredictedSpiceValue` | `AARPlayerStateBase` | Client local | Not replicated | Cosmetic HUD prediction overlay only. |
| Kill-credit FX event (`FARInvaderKillCreditFxEvent`) | `AARInvaderGameState` | Server emit | NetMulticast to all | Cosmetic hook for enemy->meter particles/cues with target player slot. |

## Offer Session Lifecycle (v1)
Server flow:
`Inactive -> FullBlastTriggered -> OfferSessionActive -> OfferChosen or OfferSkipped -> ResolveEffects -> Inactive`

Replicated offer-session payload (`FARInvaderFullBlastSessionState`):
- `bIsActive`
- `RequestingPlayerSlot`
- `ActivationTier`
- `Offers` (`UpgradeTag`, `OfferedLevel`)

Rules enforced in current runtime:
- Only one offer session may exist at a time (`RequestActivateFullBlast` fails when `bIsActive` is true).
- A new full blast cannot trigger while a session is active.
- Only the requesting player slot can resolve choose/skip.
- Both players receive replicated `FullBlastSession` and can render the same options while the non-requesting player observes.
- Selection must match one of the currently active replicated offers.
- Late-join clients reconstruct current offer session from replicated `FullBlastSession`.
- Offer presence is replicated for any player slot that publishes it during an active session.

Not yet implemented (open hardening work):
- Session id/nonce for stale-RPC rejection across sequential sessions.
- Offer timeout and auto-skip behavior.
- Chooser disconnect auto-skip behavior.

## Config Source
- Project settings class: `UARInvaderSpicyTrackSettings` (`Project Settings -> Alien Ramen -> Alien Ramen Invader Spicy Track`).
- Key knobs:
- combo timeout,
- spice-per-tier and max full-blast tier,
- skip scrap rewards per tier,
- level-roll offset weights (`-3..+3`),
- upgrade definition root tag (`Progression.InvaderUpgrade`) resolved through `UContentLookupSubsystem`,
- full-blast menu widget class (`FullBlastMenuWidgetClass`),
- enemy projectile clear tag,
- full-blast gameplay cue tag.

## Main Runtime APIs
- `RequestActivateFullBlast(...)` creates an offer session when the requester is at cap.
- `ResolveFullBlastSelection(...)` applies picked upgrade to track and resolves full-blast effects.
- `ResolveFullBlastSkip(...)` drains meter and grants configured scrap without slot replacement.
- `ActivateTrackUpgrade(...)` activates a slotted upgrade, then performs one-tier track drop.
- `GetMaxSelectableTrackCursorTierForPlayer(...)` returns the highest selectable tier for one player.
- `GetMaxSelectableTrackCursorTierAcrossPlayers(...)` returns the highest selectable tier available to any tracked player (useful for shared UI lane affordance/coloring).
- `StartSharingSpice(...)` / `StopSharingSpice(...)` drive hold-to-share transfer loop.
- `AwardKillCredit(...)` supports explicit scripted credit.
- `NotifyEnemyKilled(...)` is the automatic ingestion entry called from enemy death.
- `SetOfferPresence(...)` / `ClearOfferPresence(...)` publish/clear replicated per-player offer UI presence.
- `OnInvaderKillCreditFxEvent` broadcasts on server + clients when kill credit awards spice (includes target slot, spice gained, combo, enemy metadata, optional origin).

## Debug Console Commands
- `AR.Invader.Debug.SetSpice [p1|p2] <value>`
- `AR.Invader.Debug.AddSpice [p1|p2] <delta>`
- `AR.Invader.Debug.SetCursor [p1|p2] <tier>`
- `AR.Invader.Debug.InjectUpgrade [UpgradeTagOrRowName] [Level] [Uses|-1 for infinite]`
- `InjectUpgrade` resolves token by:
- row name first (exact/case-insensitive) from upgrade definition table,
- then gameplay tag token,
- then fallback tag-string/leaf match against loaded upgrade definitions.
- `InjectUpgrade` executes through the same resolve-selection path as normal full-blast selection (synthetic one-off offer session), so track updates and tier progression mirror real gameplay flow.
- Destination behavior:
- below max tier: injects at current full-blast lane and pushes full blast up one tier,
- at max tier: replaces topmost slotted upgrade lane (never the full-blast lane).

## Full Blast UI Bridge
- Local invader UI owner is `AARInvaderPlayerController`; it binds to `AARInvaderGameState::OnInvaderFullBlastSessionChanged`.
- Controller resolves offer definition rows through content lookup and publishes `OnInvaderFullBlastMenuSessionUpdated(bIsActive, SessionState, OfferDefinitions)`.
- Optional auto-spawned menu widget is settings-driven via `UARInvaderSpicyTrackSettings::FullBlastMenuWidgetClass`.
- Native widget base is `UARInvaderFullBlastMenuWidget`:
- receives session payload via `BP_OnFullBlastMenuUpdated(...)`,
- and sends selection/skip/presence back through owning controller (`SubmitSelection`, `SubmitSkip`, `PublishOfferPresence`, `ClearOfferPresence`).
- Menu is auto-shown only for the requesting player slot and removed when session ends.

## Gameplay Rules Implemented
- Offer generation is unique and excludes currently slotted upgrades.
- Offer eligibility checks tier locks, unlock tags, claim policy, and team-level activation prerequisites.
- If no offers are eligible, full blast activation fails (no fallback/override) and logs a loud error with rejection diagnostics.
- Activation checks claim policy and activating-player prerequisites.
- Claim policy supports one-player lock, both-players lock, or repeatable upgrades.
- Any upgrade activation resets spicy meter to `0`.
- Full blast resolve path currently uses `SetGamePaused(true/false)`, executes gameplay cue, and clears tagged enemy projectiles.

## Gameplay Pause Model
Current implementation:
- Full blast activation calls engine pause (`UGameplayStatics::SetGamePaused`), and resolve unpauses.
- Invader spicy runtime adds hard gates while full-blast session is active:
- `AARInvaderGameState::Tick` skips combo/share processing,
- kill-credit ingestion paths ignore events (`AwardKillCreditInternal`, `NotifyEnemyKilled`).

Recommended multiplayer-safe direction:
- Replace true pause with replicated gameplay suspension state (for example `bInvaderMatchSuspended` on `AARInvaderGameState`).
- While suspended: gate enemy AI updates, wave spawning/progression, and player firing; keep UI and networking active.
- Keep replication/RPC processing active so offer selection remains responsive in multiplayer.
