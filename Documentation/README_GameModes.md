# Game Modes Guide

Alien Ramen's primary gameplay loop is mode-driven:

1. [Shop Mode](README_Shop.md)
2. [Invader Mode](README_Invader.md)
3. [Scrapyard Mode](README_Scrapyard.md)

[Transition Flow](README_TransitionMode.md) is the handoff layer between those modes.

## Start Here By Task

| If you are working on... | Start here | Then read |
| --- | --- | --- |
| order serving, carryables, stations, shop NPC flow | [Shop Overview](README_Shop.md) | [Shop Runtime Contract](README_ShopRamenSystem.md) |
| combat run flow, players, waves, pickups, scoring | [Invader Overview](README_Invader.md) | [Loadouts and Player Runtime](README_Invader_Loadouts.md) |
| extraction, deposited scrap, exit zones, scrapyard rewards | [Scrapyard Overview](README_Scrapyard.md) | [Scrapyard Runtime Contract](README_ScrapyardMode.md) |
| map handoff, results screen, continue voting, travel routing | [Transition Flow](README_TransitionMode.md) | [Persistence Overview](README_Persistence.md) |

## Shop Mode

Shop is the social and preparation mode. It owns ramen service flow, carryables, stations, customer evaluation, and the pre-run economy surface.

- Entry point: [Shop Overview](README_Shop.md)
- Runtime authority: [Shop Runtime Contract](README_ShopRamenSystem.md)
- Supporting systems:
  - [Networking and Sessions](README_Networking.md)
  - [Persistence](README_Persistence.md)
  - [Parley Runtime](README_DialogueNPC.md)
  - [Plugins Overview](README_Plugins.md)

## Invader Mode

Invader is the combat run. It owns the director, waves, scoring, pickups, and the player combat/runtime loop.

- Entry point: [Invader Overview](README_Invader.md)
- Player runtime: [Loadouts and Player Runtime](README_Invader_Loadouts.md)
- Runtime ownership contract: character-owned combat/loadout state is authoritative on `AARCharacterStateRuntime`; player-owned identity/pointer state remains on `AARPlayerStateBase`.
- Supporting systems:
  - [GAS Overview](README_GAS.md)
  - [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md)
  - [Progression and Unlocks](README_ProgressionUnlocks.md)
  - [Networking and Sessions](README_Networking.md)
  - [Persistence](README_Persistence.md)

## Scrapyard Mode

Scrapyard is the post-run extraction and conversion mode. It reconciles run-earned resources, extraction budgeting, and return-to-shop payout flow.

- Entry point: [Scrapyard Overview](README_Scrapyard.md)
- Runtime authority: [Scrapyard Runtime Contract](README_ScrapyardMode.md)
- Supporting systems:
  - [Persistence](README_Persistence.md)
  - [Transition Flow](README_TransitionMode.md)
  - [Networking and Sessions](README_Networking.md)
  - [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md)

## Cross-Mode Systems

If your change touches more than one mode, start in [Shared Systems Overview](README_SharedSystems.md).

## Seamless Travel Controller Class

- `AARGameModeBase::HandleSeamlessTravelPlayer(...)` now enforces the destination mode's `PlayerControllerClass` (mode defaults/BP class settings) after seamless handoff.
- If the carried controller class does not match the destination mode default, game mode spawns/swaps to the desired controller class before gameplay reinitialization.
- Keep per-mode controller selection authored in GameMode defaults; do not hardcode mode-to-controller maps in travel callers.

## Runtime Character Switch Requests

- Authority entrypoint: `AARGameModeBase::SubmitCharacterSwitchHoldRequest(APlayerController*, bool bIsRequesting)`.
- Controller RPC bridge: `AARPlayerController::RequestSwitchToNextPlayableCharacter(...)` -> `ServerRequestSwitchToNextPlayableCharacter(...)`.
- Input contract is hold-style:
  - pressed/held sends `bIsRequesting=true`
  - released sends `bIsRequesting=false`
- Behavior:
  - if a free playable character exists, requester immediately cycles to the next free tag in `PlayableCharacterSwitchOrder`
  - if all playable characters are occupied, switch executes only when all eligible players are currently requesting; then everyone rotates to the next tag in order in one authoritative pass
  - requests are consumed by a release latch after any successful switch so holding input cannot instantly bounce players back
- Possession/update path is server-authoritative: GameMode applies final target character tags, respawns controllers with the updated identity, and refreshes speaker talkable state so dialogue/view-targeted systems stay aligned after the switch.
- Current limitation: canonical character normalization is still `Brother`/`Sister`-based in `ARPlayer` helpers; adding additional playable identities requires extending that canonicalization contract first.

## Scripted Dialogue Helper

- Authority entrypoint: `AARGameModeBase::StartParleyConversationByTagForCharacters(RequesterCharacterTag, OwnerCharacterTag, ConversationTag)`.
- Use this when mode logic needs deterministic conversation startup by exact conversation tag (for example Invader-mode scripted beats) without mode-specific subsystem plumbing.
- Helper forwards to Parley runtime scripted start API and returns `false` when authority/subsystem/runtime prerequisites are unavailable.
