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
- `AARScrapyardCarryItemBase`
  - Scrapyard world-carryable base actor for extraction items.
  - `ForwardUseToController` routes BI_Interactable-style use to scrapyard pickup (`AARScrapyardPlayerController`) instead of shop pickup.
  - Inherits `ForwardSecondaryUseToController` / `UseSecondaryInWorldByController` default behavior from `AARShopCarryItemBase` for secondary world-item kick handling.
  - Replicates item identity fields (`ScrapyardItemTag`, `FallbackScrapCost`) for remote inspect/UI resolution.
- `AARScrapyardPlayerController`
  - `RequestUseSecondaryOnHeldCarryItem()` provides generic held-secondary dispatch through held `AARShopCarryItemBase::UseSecondaryByController(...)` (default throw unless item override).
- `AARPlayerController` (shared interaction runtime used by `AARScrapyardPlayerController`)
  - Tracks active hold targets (`ActiveInteractable`, `ActiveSecondaryInteractable`) plus latch state (`bIsInteracting`) for hold-style interaction input.
  - Performs server-side periodic range re-validation (`ActiveInteractionRangeCheckInterval`) for active targets and calls `IARInteractableRangeListener::OnPlayerOutOfRange(...)` on opted-in interactables before clearing target state.
- `UARInvaderDirectorSubsystem`
  - End-of-run authority (loss, unanimous early-bail, stop reasons).
  - Death-penalty application to run ledger.
- `UARRunBuffSubsystem`
  - Save-backed per-character stored/queued/active run-buff state.
  - Consume/apply/clear authority for energy-drink buffs.
  - Extracted energy drinks route to stored inventory only when `Unlock.Shop.Storage.EnergyDrink` is active; otherwise they route to queued next-run stacks.
- `UARItemDefinitionSubsystem`
  - Shared item-definition resolver facade used by Scrapyard + Shop.
  - Delegates row resolution to `UTagKeySubsystem`.
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

## Transition Handoff

- Scrapyard finalization now defaults to transition-map routing:
  - `Scrapyard -> Transition -> Shop`
- `AARScrapyardGameState` resolves final travel URL through `AARGameModeBase::BuildModeTravelURL` so it shares the same transition routing/config contract as Shop/Invader.
- Travel routing can be overridden per call with `EARTravelRoutePolicy` (`ModeDefault`, `ForceTransitionMap`, `ForceDirect`) for same-mode multi-map flows.
- Context is passed in URL options (`ARTrSource/ARTrReason/ARTrDest/ARTrFresh`) and hydrated into `AARTransitionGameState`.
- Transition-map routing is configured per mode on `AARGameModeBase` (`bRouteModeTravelThroughTransitionMap`, `TransitionTravelMapURL`, `TransitionSourceMode`, `TransitionReason`).

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
  - `AAREnergyDrinkCarryItem` replicates `EnergyDrinkItemTag` so remote UI can resolve drink content from spawned world actors.
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
  - `bAlwaysSpawn` bypasses noise/quotas but still respects runtime tag gates and rarity cap.
  - `SpawnerWeight` biases managed selection when GameMode orchestrates spawns.

## Managed Scrapyard Spawns (GameMode-owned)

- `AARScrapyardGameMode` orchestrates scrapyard spawns when `SpawnRuleSet` is set.
- Rule asset: `UARScrapyardSpawnRuleSet` ➜ `FARScrapyardSpawnRules`
  - `MinTotalSpawns`, `MaxTotalSpawns`
  - `NoiseScale`, `NoiseThreshold`, `NoiseJitter`
  - `RarityBudgets` map (`EARScrapyardItemRarity` → `{MinCount, MaxCount}`)
- Algorithm (authority, deterministic):
  1. Collect all `AARScrapyardItemSpawner` actors that have not already spawned.
  2. Build each spawner’s eligible items (using item definitions + spawn condition tags).
  3. Compute seeded Perlin noise per spawner; apply `NoiseThreshold` gate unless `bAlwaysSpawn` is true.
  4. Phase 0: fire all `bAlwaysSpawn` spawners (counting toward budgets).
  5. Phase 1: satisfy per-rarity `MinCount`.
  6. Phase 2: fill until `MaxTotalSpawns` and per-rarity `MaxCount`.
  7. Each spawn uses a deterministic seed from run seed + spawner identity.
- Authoring guidance:
  - Set `bSpawnOnBeginPlay=false` on spawners when using managed mode to avoid double spawns.
  - Use `bAlwaysSpawn` for guaranteed hero pieces; adjust `SpawnerWeight` to bias selection.

## UI Read Model Surface

- `AARScrapyardGameState::FARScrapyardExtractionSummary` exposes:
  - kept/trimmed counts
  - leftover/trimmed/wasted scrap
  - purchased/discarded counts
  - converted money
  - granted reward list
- `AARScrapyardHUD`, `UARScrapyardHUDWidgetBase`, `UARScrapyardExitZoneWidgetBase` bind to replicated summary/timer/run-active/run-buff snapshot delegates.
- Shared resolve path:
  - systems should resolve item/energy-drink rows through `UARItemDefinitionSubsystem` instead of duplicating direct `UTagKeySubsystem` calls.
