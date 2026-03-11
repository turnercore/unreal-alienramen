# Shop Ramen System (Built On Top)

This document captures the runtime ownership and integration contract for the shop ramen ordering/serving loop.

## Ownership Model

- **Server-authoritative runtime**:
  - `UARCustomerComponent` owns customer order state and serving evaluation.
  - `AARShopDispenserActor` owns generic item dispense flow (spawn + optional carry handoff + source consumption policy).
  - `AARShopStationActor` owns station slot/processing/stock runtime.
  - `AARShopCarryItemBase` owns shared shop carry-item lifecycle (`ReleaseCarryItem`) for bowl/meat actors.
  - `AARRamenBowlActor` owns bowl fill progression (strict sequence).
  - `AARMeatStorageBoxActor` is the meat-reserve specialization of `AARShopDispenserActor`.
- **Dialogue-owned outcomes**:
  - relationship mutation and emotion output are applied through `UARDialogueSubsystem::ApplyRamenServeOutcome(...)`.
  - shop code does not own dialogue/emotion/relationship authority.

## Configuration Sources

- `UARCustomerSettings` (`Project Settings -> Alien Ramen -> Shop Settings`) provides:
  - customer/station root tags for TagContentResolver
  - relationship point curve (`Hate/Ok/Like/Love`)
  - default reaction emotion tags
  - default station processing duration and stock cap
  - fallback-order policy
- TagContentResolver routes are expected for:
  - `Shop.Customer` -> `FARCustomerDefinitionRow`
  - `Shop.Station` -> `FARShopStationConfigRow`

## Speaker + Customer Flow

- `AARNPCCharacterBase` is a lean shell; `UARCustomerComponent`, `UARSpeakerComponent`, and `UAREmotionComponent` are optional and can be authored independently per actor.
- Customer runtime speaker identity comes from `UARCustomerComponent::GetSpeakerTag()`:
  - `SpeakerTagOverride` when explicitly authored on the customer component
  - otherwise the owning `UARSpeakerComponent` speaker tag
- `FARCustomerDefinitionRow` is keyed by TagContentResolver row tag/row name route mapping and does not carry a separate identity/speaker field.
- Interact priority is **delivery-first**:
  1. try serving held completed bowl via customer component
  2. if serving fails, fallback to dialogue via `UARSpeakerComponent` (when present)
- `AARNPCCharacterBase::ForwardUseToController(AActor*)` is the preferred optional BP convenience entrypoint for BI_Interactable forwarding because it accepts either pawn or controller source references and routes to controller RPCs.
- Speaker talkable queries stay true while an active customer order exists so interaction prompts can still route ramen delivery when dialogue is locally gated.
- Customer evaluation rules:
  - unordered color matching
  - `None` ignored for non-picky scoring
  - picky mode requires strict unordered exact composition (with implied `None` fill)
  - reaction mapping remains `0/1/2/3 matches => Hate/Ok/Like/Love`
- Customer lifecycle controls:
  - customers can be configured with finite order budgets (`MaxOrdersToGenerate`; `0` = unlimited).
  - optional auto-generation at spawn and after serve is preserved.
  - when finite budget is exhausted, customer marks done ordering and emits done signal.
  - runtime emits detailed signals for order generated, order served, and done-ordering states (counts + remaining budget).
  - customer runtime still drives the local speaker gate while orders are active; convenience interact paths can still attempt speaker fallback after delivery attempt.
  - ordering emotion now routes through generic emotion-system overrides (state + timed reaction), so fallback returns to dialogue/base emotion automatically.

## Station Runtime Contract

- Station states: `Idle`, `MeatReady`, `Processing`, `Processed`.
- Base vs upgraded behavior:
  - unupgraded station output is direct `None` for bowl fill (no meat/process/stock required).
  - upgraded station uses the slot + processing + stock model.
  - upgrade state is unlock-tag driven (`RequiredUpgradeTags`); there is no starts-upgraded override.
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

## World Carry Item Interaction

- World carryables (`AARShopCarryItemBase`, including bowl/meat actors) expose `ForwardUseToController(AActor* UsingActor)` for BI_Interactable forwarding.
- `ForwardUseToController(...)` resolves `AARShopPlayerController` (direct controller or pawn owner controller) and routes to `RequestShopPickupCarryItem(...)`.
- Shop-only carry interaction requests live on `AARShopPlayerController`:
  - `RequestShopUseOrDrop(AActor*)` for one-shot input routing (`ForwardUseToController` when target exists, fallback drop when null)
  - `RequestShopPickupCarryItem(AARShopCarryItemBase*)`
  - `RequestShopDropHeldCarryItem()`
  - `RequestShopThrowHeldCarryItem(float ThrowStrength)`
- `RequestShopPickupCarryItem(nullptr)` is treated as a no-hit fallback: if the controller currently holds a carry item, it drops it.
- `AARPlayerCharacterShop` exposes BP helpers `IsCarryingShopItem()` and `GetHeldShopActor()` for pawn-side input/UI branching.
- Pickup is authority-validated and blocked when the item is already attached to another actor (for example station slot ownership).
- Carryables replicate movement so held/drop/throw transforms stay authoritative across listen-server + clients.
- Carry presentation/drop/throw physics resolve against a valid primitive component on the item (not strictly actor root), so carryable Blueprints can use `DefaultSceneRoot` as long as they include at least one world-colliding primitive component.
- Drop/throw restore world physics and gravity on the released carry item.

## Persistence + Replication

- Station processing progress is replicated runtime state only (not persisted in `UARSaveGame`).
- Meat inventory remains save-facing through `AARGameStateBase::Meat`.
- Meat-reserve dispenser entries decrement replicated GameState meat buckets and spawn world meat actors.

## StateTree Integration

- Shop AI now has StateTree scaffolding:
  - `UARShopStateTreeAIComponent`
  - `UARShopStateTreeAIComponentSchema`
  - `AARShopAIController` start/event helpers
- `AARShopAIController` maps active `State.ShopNPC.*` tags to speaker dialogue gating:
  - dialogue allowed when `State.ShopNPC.DialogueWindow` is active
  - otherwise dialogue is locally blocked while non-dialogue shop states are active
- Customer component emits order lifecycle events (`Event.ShopNPC.OrderGenerated` / `Event.ShopNPC.OrderServed`) for StateTree-driven speaker behavior.
