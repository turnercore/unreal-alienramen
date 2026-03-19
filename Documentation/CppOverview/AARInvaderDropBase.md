# AARInvaderDropBase (`ARInvaderDropBase.h`)
Path: `Source/AlienRamen/Public/ARInvaderDropBase.h`

## Purpose

- Replicated invader currency pickup base for scrap/meat drops.
- Designed for BP subclasses that provide visuals and optional custom cue behavior.

## Runtime Contract

- Server-authoritative for:
- proximity pickup checks
- collection state transition
- reward application into `AARGameStateBase` currencies
- Actor physics defaults:
- simulate physics enabled
- gravity disabled
- movement constrained to XY (locked Z translation)
- pawn collision response is project-settings-driven (`UARInvaderDirectorSettings::DropPawnCollisionMode`)
- collection switches to fast lerp-to-player visual and disables normal collision/physics.
- collection detection is driven by `UARPickupCollectorComponent` on `AARPlayerCharacterInvader`; server still validates and awards.
- collector is event-driven: overlap detector + ASC pickup-radius attribute-change subscription (not per-frame world scans).
- owning clients can start predicted local collection visuals immediately while waiting for server confirmation.

## Reward Routing

- `DropType = Scrap` -> increments run-ledger scrap (`AARGameStateBase::AddRunLedgerScrap`).
- `DropType = Meat` -> uses the drop actor's replicated `MeatDefinitionTag` (resolved at spawn) and writes run-ledger typed meat (`AARGameStateBase::AddRunLedgerTypedMeat`).
- `DropColor` is runtime-authored by the killing enemy state (enemy color at drop spawn time), so meat type identity and drop color remain decoupled.
- `SourceEnemyIdentifierTag` is retained for diagnostics/fallback validation; gameplay reward identity is `MeatDefinitionTag`.

## Blueprint Hooks

- `BP_OnCollectionStarted(CollectingPlayer)`
- `BP_OnRewardApplied(CollectingPlayer)`
- `ApplyDropReward` is `BlueprintNativeEvent` for custom reward routing override when needed.
