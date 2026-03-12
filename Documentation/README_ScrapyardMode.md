# Scrapyard Mode + Temp Buff Runtime Contract

This document captures the C++ runtime contracts for Scrapyard extraction economy and temporary run buffs.

## Ownership

- `AARScrapyardGameState`
  - Authoritative Scrapyard run timer/state.
  - Shared scrap reserve/refund accounting (negative allowed only here).
  - Exit-zone registration and extraction candidate aggregation.
  - Deterministic overspend trim + reward grant finalization.
- `AARScrapyardExitZoneActor`
  - Tracks deposited carry items.
  - Tracks player occupancy in exit volume.
  - Tracks replicated per-exit reserved scrap value for deposited items.
  - Reports reserve/refund mutations to `AARScrapyardGameState`.
- `UARRunBuffSubsystem`
  - Save-backed storage/queue/active temp-buff state.
  - Invader-init run-buff rotation.
  - Active payload apply/remove runtime integration on `AARPlayerStateBase` ASC.

## Scrapyard Scrap Rules

- Scrapyard reserve/refund uses `AARScrapyardGameState` authority APIs:
  - `ReserveScrapForItem`
  - `RefundScrapForItem`
- Reserve subtracts item cost from shared scrap immediately.
- Refund adds cost back immediately.
- Scrapyard allows negative shared scrap during extraction accounting.
- Invader/Shop remain clamped non-negative because they use `AARGameStateBase::SetScrapFromSave`.
- Scrapyard finalization always sets shared scrap to `0` before travel.

## Extraction Candidate Set

Finalization uses the global candidate set:

1. All deposited items in all registered exit zones.
2. Any carry item currently held by players standing inside an exit zone.

Candidates are de-duplicated by actor pointer and evaluated against available budget.

## Deterministic Overspend Trim

- Budget = `CurrentScrap + ReservedCostTotal` (pre-reserve equivalent).
- If total candidate cost exceeds budget:
  - candidates are trimmed using a seeded deterministic RNG (`ScrapyardRunSeed`) until affordable.
- Kept candidates grant rewards.
- Trimmed candidates are removed with no reward.

## Reward Routing

- `LicenseUnlock` reward:
  - Adds unlock tag through GameState unlock surface.
- `EnergyDrink` reward:
  - Routed into `UARRunBuffSubsystem::AddExtractedEnergyDrink`.
  - If `Unlock.Shop.Storage.EnergyDrink` is owned, reward goes to stored inventory.
  - Otherwise it is queued for next Invader run.
- Grant order is deterministic: `LicenseUnlock` rewards resolve before `EnergyDrink` rewards, so unlock-gated routing in the same finalization pass is stable.

Item definitions are resolved through TagContentResolver route root `Scrapyard.Item` with row type `FARScrapyardItemDefRow`.

## Temp Buff Persistence + Rotation

Save fields (`UARSaveGame`, schema v8):

- `StoredEnergyDrinkStacks`
- `QueuedEnergyDrinkStacks`
- `ActiveRunBuffPayloads`
- `ActiveRunBuffCycleId`

Rotation behavior (`UARRunBuffSubsystem::RotateRunBuffsAtInvaderInit`):

1. Remove previously active runtime-applied effects/tags.
2. Consume queued drinks into new active payload snapshot.
3. Increment `ActiveRunBuffCycleId`.
4. Apply new active payload to all current player states.

Idempotency guard:

- Rotation skips duplicate apply when same world + same active cycle is already processed.

## Invader Integration

- `AARInvaderGameMode::BeginPlay` triggers run-buff rotation (authority).
- `AARInvaderGameMode::HandleStartingNewPlayer_Implementation` reapplies active payload for late joins.
- `AARInvaderGameMode::RestartPlayer` reapplies active payload for respawn/restart paths.

## Scrapyard Buff Continuity

- Rotation still occurs only during Invader initialization.
- `AARScrapyardGameMode::BeginPlay` reapplies the current active payload snapshot to connected players.
- `AARScrapyardGameMode::HandleStartingNewPlayer_Implementation` and `RestartPlayer` reapply active payload for join/respawn continuity in Scrapyard.

## Scrapyard Pawn Resolution from Loadout

- `AARScrapyardGameMode::GetDefaultPawnClassForController_Implementation` resolves player ship tag from loadout.
- Ship row lookup is done through TagContentResolver (`Unlock.Ship.*`).
- Scrapyard pawn class is read from ship row field `ScrapyardPawnClass` (reflection-based).
- Safe fallback order:
  1. Ship row `ScrapyardPawnClass`
  2. `FallbackScrapyardPawnClass` on Scrapyard GameMode
  3. GameMode default pawn class path

## UI Hook Surface

- `AARScrapyardHUD`
  - Local HUD-side binding layer for Scrapyard runtime UI.
  - Binds to `AARScrapyardGameState` delegates:
    - `OnScrapyardExtractionSummaryChanged`
    - `OnScrapyardRunTimerChanged`
    - `OnScrapyardRunActiveChanged`
  - Binds to `UARRunBuffSubsystem::OnRunBuffStateChanged`.
  - Caches latest summary/timer/run-active/run-buff snapshot and rebroadcasts through Blueprint events + assignable delegates.
  - Entry points:
    - `InitializeScrapyardHUD`
    - `DeinitializeScrapyardHUD`
    - `GetCachedExtractionSummary`
    - `GetCachedRunRemainingSeconds`
    - `GetCachedRunActive`
    - `GetCachedRunBuffStateSnapshot`

- `UARScrapyardHUDWidgetBase`
  - Reusable widget bridge for Scrapyard HUD state.
  - Binds to `AARScrapyardHUD` delegate surface and exposes BP update hooks:
    - extraction summary
    - timer
    - run active state
    - run-buff snapshot
  - Supports auto-bind to owning Scrapyard HUD on construct (`bAutoBindOwningScrapyardHUDOnConstruct`).

- `UARScrapyardExitZoneWidgetBase`
  - Reusable widget bridge for a single `AARScrapyardExitZoneActor`.
  - Binds to `OnExitZoneChanged`.
  - Caches replicated `DepositedReservedScrapValue` and forwards updates to BP hooks/delegates.
  - Supports auto-observing an initial exit zone (`InitialObservedExitZone`, `bAutoObserveInitialExitZoneOnConstruct`).

- `AARScrapyardGameState`
  - `GetExtractionSummary`
  - `GetScrapyardRunRemainingSeconds`
  - `OnScrapyardExtractionSummaryChanged`
  - `OnScrapyardRunTimerChanged`
  - `OnScrapyardRunActiveChanged`
- `AARScrapyardExitZoneActor`
  - `GetDepositedReservedScrapValue`
  - `OnExitZoneChanged`
- `UARRunBuffSubsystem`
  - `GetRunBuffStateSnapshot`
  - `OnRunBuffStateChanged`
  - stored/queued count query helpers
