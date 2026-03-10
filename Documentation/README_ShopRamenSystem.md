# Shop Ramen System (Built On Top)

This document captures the runtime ownership and integration contract for the shop ramen ordering/serving loop.

## Ownership Model

- **Server-authoritative runtime**:
  - `UARCustomerComponent` owns NPC customer order state and serving evaluation.
  - `AARShopDispenserActor` owns generic item dispense flow (spawn + optional carry handoff + source consumption policy).
  - `AARShopStationActor` owns station slot/processing/stock runtime.
  - `AARRamenBowlActor` owns bowl fill progression (strict sequence).
  - `AARMeatStorageBoxActor` is the meat-reserve specialization of `AARShopDispenserActor`.
- **Dialogue-owned outcomes**:
  - relationship mutation and emotion output are applied through `UARDialogueSubsystem::ApplyRamenServeOutcome(...)`.
  - shop code does not own dialogue/emotion/relationship authority.

## Configuration Sources

- `UARCustomerSettings` (`Project Settings -> Alien Ramen|NPC -> Customer`) provides:
  - customer/station root tags for TagContentResolver
  - relationship point curve (`Hate/Ok/Like/Love`)
  - default reaction emotion tags
  - default station processing duration and stock cap
  - fallback-order policy
- TagContentResolver routes are expected for:
  - `NPC.Customer` -> `FARCustomerDefinitionRow`
  - `Shop.Station` -> `FARShopStationConfigRow`

## NPC Customer Flow

- `AARNPCCharacterBase` now hosts `UARCustomerComponent`.
- Interact priority is **delivery-first**:
  1. try serving held completed bowl via customer component
  2. fallback to dialogue via `UARNPCTalkComponent`
- NPC talkable queries stay true while an active customer order exists so interaction prompts can still route ramen delivery when dialogue is locally gated.
- Customer evaluation rules:
  - unordered color matching
  - `None` ignored for non-picky scoring
  - picky mode requires strict unordered exact composition (with implied `None` fill)
  - reaction mapping remains `0/1/2/3 matches => Hate/Ok/Like/Love`

## Station Runtime Contract

- Station states: `Idle`, `MeatReady`, `Processing`, `Processed`.
- Base vs upgraded behavior:
  - unupgraded station output is direct `None` for bowl fill (no meat/process/stock required).
  - upgraded station uses the slot + processing + stock model.
- Meat slot behavior:
  - meat is physically slotted on station (`SlottedMeatActor`) and can be picked back up in `MeatReady`.
  - slot replacement is blocked while occupied.
- Processing behavior:
  - hold-to-process (`StartProcessingByController`/`StopProcessingByController`)
  - processing progress pauses/resumes and replicates to all players
  - slotted meat is consumed immediately when processing starts
  - processing `None` is blocked if station currently has colored processed stock
- Bowl draw behavior:
  - bowl consumes one processed stock unit per fill
  - bowl sequence is strict: `Noodles -> Broth -> Toppings`

## Persistence + Replication

- Station processing progress is replicated runtime state only (not persisted in `UARSaveGame`).
- Meat inventory remains save-facing through `AARGameStateBase::Meat`.
- Meat-reserve dispenser entries decrement replicated GameState meat buckets and spawn world meat actors.

## StateTree Integration

- Shop AI now has StateTree scaffolding:
  - `UARShopStateTreeAIComponent`
  - `UARShopStateTreeAIComponentSchema`
  - `AARShopAIController` start/event helpers
- `AARShopAIController` maps active `State.ShopNPC.*` tags to NPC dialogue gating:
  - dialogue allowed when `State.ShopNPC.DialogueWindow` is active
  - otherwise dialogue is locally blocked while non-dialogue shop states are active
- Customer component emits order lifecycle events (`Event.ShopNPC.OrderGenerated` / `Event.ShopNPC.OrderServed`) for StateTree-driven NPC behavior.
