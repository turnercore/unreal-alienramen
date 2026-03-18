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
