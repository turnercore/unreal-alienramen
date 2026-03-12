# Scrapyard + Invader Economy Contract

This document captures the server-authoritative runtime contract for:

- Scrapyard extraction/resolution
- Invader run-ledger flow into Scrapyard
- Shop re-entry cleanup/deposit
- Temp run-buff ownership and energy-drink behavior

## Ownership

- `AARScrapyardGameState`
  - Scrapyard timer authority (including pause + additive time).
  - Reserve/refund accounting for extraction items.
  - Deterministic overspend trim and reward finalization.
  - Replicated extraction summary and run-buff snapshot for HUD/widgets.
- `AARScrapyardExitZoneActor`
  - Deposited-item tracking + in-zone player tracking.
  - Per-zone replicated reserved-scrap read model.
  - Reserve/refund mutation entrypoint into Scrapyard GameState.
- `UARInvaderDirectorSubsystem`
  - End-of-run authority (loss, unanimous early-bail, stop reasons).
  - Death-penalty application to run ledger.
- `UARRunBuffSubsystem`
  - Save-backed per-character stored/queued/active run-buff state.
  - Consume/apply/clear authority for energy-drink buffs.
- `UARItemDefinitionSubsystem`
  - Shared item-definition resolver facade used by Scrapyard + Shop.
  - Delegates row resolution to `UTagContentResolverSubsystem`.
- `AARShopGameMode`
  - Run-ledger deposit to storage with clamps.
  - First-shop-entry run-buff cleanup.
  - Shop loose-carryable restore/spawn policy for energy drinks/meat.

## Data + Resolver Routes

- Scrapyard item definitions: route root `Scrapyard.Item` (`FARScrapyardItemDefRow`).
  - Includes item type/rarity, main/alt text, knowledge gates, spawn conditions, rewards, sell value, stack/weight/model metadata.
- Energy drink definitions: route root `Scrapyard.EnergyDrink` (`FAREnergyDrinkDefRow`).
  - Includes icon + per-run GE/tag payload + stack rules.
- Economy tuning: `UAREconomySettings`.
  - `InvaderDeathPenaltyPercent`
  - `MaxScrapStorage`
  - `MaxMeatStorage`
  - `DefaultScrapyardDurationSeconds`
  - `ScrapyardSpawnSeedSalt`

## Run Ledger Flow

- Invader drops write to run ledger only (`AARGameStateBase::RunLedgerScrap/RunLedgerMeat`).
- Loss handling:
  - all-players-downed/dead ends run with loss reason.
  - configurable percent death penalty is applied to run ledger (not persistent storage).
- Early-bail:
  - vote is tracked per active player slot.
  - unanimous yes ends run early without death-loss penalty path.
- Scrapyard budget start:
  - `ScrapyardSharedScrap = ShopStoredScrap + RunLedgerScrap`.
- Scrapyard finalization:
  - deterministic trim remains seed-based.
  - shared scrap is set to `0` before travel.
  - leftover scrap is preserved via run ledger for shop deposit.
- Shop entry:
  - deposit leftover scrap + run-ledger meat into storage.
  - clamp by economy max storage settings.
  - clear run ledger after deposit.

## Scrapyard Finalization Rules

- Candidate set:
  - deposited items in exit zones.
  - held items carried by players currently inside exit zones.
- Trim:
  - when total kept cost exceeds budget, random removal uses deterministic run seed.
- Reward support:
  - unlock tag rewards
  - progression tag rewards
  - energy drink rewards (to run-buff storage inventory)
- Text fallback safety:
  - knowledge-gated rows missing alt-name/alt-description fall back to primary text with warning logs.

## Temp Buff / Energy Drink Contract

- Character key:
  - loadout character gameplay tag is the ownership key for active payloads.
- Consume semantics:
  - duplicate consume of same drink type for same character is blocked.
  - shop-world drink consume path is separate from inventory consume path.
- Lifecycle:
  - invader end clears queued persisted stacks.
  - active payload stays runtime-valid through Scrapyard.
  - first shop entry clears queued + active payloads (runtime + save).
- Reapply:
  - active payload reapplies on join/respawn in Invader + Scrapyard.
- Save schema:
  - run-buff + shop transient fields are in `UARSaveGame` schema `v10`.

## Shop Energy Drink + Transient Carryables

- Shop consume scope:
  - `AAREnergyDrinkCarryItem` consume is accepted in `Mode.Shop` only.
- Spawn ownership:
  - stored drink inventory is authoritative pre-spawn.
  - when spawned into shop anchors, inventory count is decremented.
  - spawned drink actors are world-owned carryables.
- Loose carryable persistence:
  - `UARSaveGame::ShopTransientCarryables` stores loose shop energy-drink/meat world state.
  - shop saves capture loose carryables (excluding held/attached items).
  - shop load restores transient carryables before pre-run continuation.
  - run start clears transient loose list.
  - invader/scrapyard end marks next-shop-load clear gate (`bClearShopTransientCarryablesOnNextShopLoad`) so stale pre-run loose items do not survive completed-run transitions.

## Scrapyard Timer + Spawner

- Timer:
  - replicated paused state and accumulated pause time.
  - `SetScrapyardRunTimerPaused` + `AddScrapyardTime` are authority APIs.
  - game-state effective pause drives scrapyard timer pause.
- Spawner (`AARScrapyardItemSpawner`):
  - allowed item tags
  - required runtime tags (unlock/loadout)
  - spawn chance
  - rarity cap
  - weighted selection by item weight
  - deterministic RNG from run seed + economy seed salt

## UI Read Model Surface

- `AARScrapyardGameState::FARScrapyardExtractionSummary` exposes:
  - kept/trimmed counts
  - leftover/trimmed/wasted scrap
  - purchased/discarded counts
  - converted money
  - granted reward list
- `AARScrapyardHUD`, `UARScrapyardHUDWidgetBase`, `UARScrapyardExitZoneWidgetBase` bind to replicated summary/timer/run-active/run-buff snapshot delegates.
- Shared resolve path:
  - systems should resolve item/energy-drink rows through `UARItemDefinitionSubsystem` instead of duplicating direct `UTagContentResolverSubsystem` calls.
