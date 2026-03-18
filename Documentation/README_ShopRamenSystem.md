# Shop Game Mode Runtime Contract

This document captures the runtime ownership and integration contract for the shop ramen ordering/serving loop.

## Mode Exit Travel

- `AARShopGameState::FinalizeShopRunAndTravelToInvader(InInvaderTravelURL)` is the explicit authority Blueprint helper for ending shop mode and starting run travel.
- Client/UI entrypoint: `AARShopPlayerController::RequestFinalizeShopRunAndTravelToInvader(...)` routes through server RPC and invokes the authoritative game-state helper.
- Destination input should be the final gameplay map URL (for example `/Game/Maps/Lvl_Invader`), not the transition map.
- Empty destination input falls back to `AARShopGameState::DefaultInvaderTravelURL`.

## Ownership Model

- **Server-authoritative runtime**:
  - `UARCustomerComponent` owns customer order state and serving evaluation.
  - `AARShopDispenserActor` owns generic item dispense flow (spawn + optional carry handoff + source consumption policy).
  - `AARShopStationActor` owns station slot/processing/stock runtime.
  - `AARShopCarryItemBase` owns shared shop carry-item lifecycle (`ReleaseCarryItem`) for bowl/meat actors.
  - `AARRamenBowlActor` owns bowl fill progression (strict sequence).
  - `AARMeatStorageBoxActor` is the meat-reserve specialization of `AARShopDispenserActor`.
- **Dialogue-owned outcomes**:
  - relationship mutation and emotion output are applied through `UParleyDialogueSubsystem::ApplyRamenServeOutcome(...)`.
  - shop code does not own dialogue/emotion/relationship authority.

## Configuration Sources

- `UARCustomerSettings` (`Project Settings -> Alien Ramen -> Shop Settings`) provides:
  - customer/station root tags for TagKey
  - relationship point curve (`Hate/Ok/Like/Love`)
  - default reaction emotion tags
  - default station processing duration and stock cap
  - fallback-order policy
- TagKey routes are expected for:
  - `Shop.Customer` -> `FARCustomerDefinitionRow`
  - `Unlock.Shop.Station` -> `FARShopStationConfigRow`
- `Item.Meat` -> `FARMeatDefinitionRow`
- Character table rows for shop/runtime character spawning can use `FARShopCharacterDefRow` (`Source/AlienRamen/Public/ARLoadoutTypes.h`) with:
  - `CharacterTag` (`Shop.Character.*`)
  - `CustomerTag` (`Shop.Customer.*`)
  - `SpeakerTag` (`Parley.Speaker.*`)
  - `CharacterClass` (soft pawn Blueprint/class; shown as `Blueprint` in row editor)
  - `Shop.Character`, `Shop.Customer`, and `Parley.Speaker` tags should mirror by the first segment under each root (for example `Brother`), while deeper subleafs may differ.
  - Speaker/customer behavior wiring still stays component-driven on the spawned pawn Blueprint.

## Speaker + Customer Flow

- `AARNPCCharacterBase` is a lean shell; `UARCustomerComponent`, `UParleySpeakerComponent`, and `UEmoComponent` are optional and can be authored independently per actor.
- Customer runtime speaker identity comes from `UARCustomerComponent::GetSpeakerTag()`:
  - `SpeakerTagOverride` when explicitly authored on the customer component
  - otherwise the owning `UParleySpeakerComponent` speaker tag
- Customer order UI can be authored per-NPC via `UARCustomerComponent::OrderWidgetClass` (`UARCustomerOrderWidgetBase` subclass). Runtime helpers:
  - `CreateAndInitializeOrderWidget(APlayerController*)`
  - `InitializeOrderWidget(UARCustomerOrderWidgetBase*)`
  - Widget base binds to customer delegates (`OnCustomerOrderChanged`/`GeneratedDetailed`/`Resolved`/`DoneOrdering`) and exposes BP events for styling.
- `FARCustomerDefinitionRow` is keyed by TagKey row tag/row name route mapping and does not carry a separate identity/speaker field.
- StateTree-facing gate helpers:
  - `UARCustomerComponent::HasOrderForInteraction()` reports active order availability for serve-first interaction branches.
  - `UParleySpeakerComponent::HasDialogueToSay()` reports dialogue talkability.
- `AARNPCCharacterBase` exposes cached actor-level bools for direct StateTree condition binding (safe when optional components are missing):
  - `bST_HasActiveOrder`
  - `bST_HasDialogueToSay`
- Interact priority is **delivery-first**:
  1. try serving held completed bowl via customer component
  2. if serving fails, fallback to dialogue via `UParleySpeakerComponent` (when present)
- `AARNPCCharacterBase::ForwardUseToController(AActor*)` is the preferred optional BP convenience entrypoint for BI_Interactable forwarding because it accepts either pawn or controller source references and routes to controller RPCs.
- Speaker talkable queries stay true while an active customer order exists so interaction prompts can still route ramen delivery when dialogue is locally gated.
- Customer evaluation rules:
  - unordered color matching
  - `None` request matches served `None` only
  - `Colorless` request matches any served non-`None` color
  - picky mode uses strict 3-slot matching with wildcard semantics (`Colorless` matches any non-`None`)
  - reaction mapping remains `0/1/2/3 matches => Hate/Ok/Like/Love`
- Customer lifecycle controls:
  - customers can be configured with finite order budgets (`MaxOrdersToGenerate`; `0` = unlimited).
  - optional auto-generation at spawn and after serve is preserved.
  - when finite budget is exhausted, customer marks done ordering and emits done signal.
  - runtime emits detailed signals for order generated, order served, and done-ordering states (counts + remaining budget).
  - customer runtime still drives the local speaker gate while orders are active; convenience interact paths can still attempt speaker fallback after delivery attempt.
  - ordering emotion now routes through generic emotion-system overrides (state + timed reaction), so fallback returns to dialogue/base emotion automatically.

## Shop Economy + Vending

- Runtime payout authority is `AARShopGameMode`.
- Base bowl payout:
  - authored on `AARShopGameMode::BaseBowlPayout` (default `10`)
  - mirrored to `AARShopGameState::BaseBowlPayout` for UI/readability
- Serve payout formula (`UARCustomerComponent::TryServeBowl`):
  - `Total = BaseBowlPayout + RoundToInt(CombinedMeatValue * SampledReactionMultiplier)`
  - `CombinedMeatValue` resolves from bowl slot `Item.Meat` tags -> meat row `ItemTag` -> shared item `SellMoneyValue`
  - sampled reaction multiplier range source is `AARShopGameMode` (`Hate/Ok/Like/Love` ranges)
- Vending settlement:
  - queued bowls persist to `UARSaveGame::PendingVendingStockedBowls`
  - shop-entry finalization awards per-bowl `RoundToInt(1 + CombinedMeatValue * QualityMultiplier)` and clears ledger
  - quality multipliers are authored on `AARShopGameMode` (`Low/Standard/High/Premium`)

## Station Runtime Contract

- Station states: `Idle`, `MeatReady`, `Processing`, `Processed`.
- Base vs upgraded behavior:
  - unupgraded station output is direct `None` for bowl fill (no meat/process/stock required).
  - upgraded station uses the slot + processing + stock model.
  - upgrade state is unlock-tag driven (`RequiredUpgradeTags`).
  - manual/debug authoring override: when `Resolve Config from Data` is disabled and `RequiredUpgradeTags` is empty, station is treated as upgraded.
- Meat slot behavior:
  - meat is physically slotted on station (`SlottedMeatActor`) and can be picked back up in `MeatReady`.
  - slot replacement is blocked while occupied.
  - loose world meat (dropped/thrown) that contacts an eligible empty station auto-slots through the same authoritative placement path as held-meat placement.
  - slotted meat presentation is anchored with physics/collision disabled regardless of whether the meat came from held placement or loose world contact.
- Processing behavior:
  - station-controlled input mode (`ProcessingInputMode`): `Hold` or `Tap`
  - hold-to-process (`StartProcessingByController`/`StopProcessingByController`)
  - tap-to-process (`TapProcessByController`) advances progress by `TapProcessingSecondsPerPress / EffectiveProcessingDuration` per press.
  - in `Tap` mode, `StartProcessingByController` consumes at most one pulse per press and requires `StopProcessingByController` (release) before the next pulse.
  - processing progress pauses/resumes and replicates to all players
  - slotted meat is consumed immediately when processing starts
  - processed stock carries both color + `Item.Meat` tag
  - when stock already exists, processing is blocked only for identical output type (same color + same meat tag)
  - processing without slotted meat is gated by `bAllowProcessingWithoutMeat` (from station row/runtime setting)
  - processing `None` is blocked whenever the station already has any buffered stock (colored or `None`); it is only allowed when stock is fully empty
- Bowl draw behavior:
  - bowl consumes one processed stock unit per fill
  - bowl fill records both slot color and slot `Item.Meat` tag (`NoodlesMeatTag`, `BrothMeatTag`, `ToppingsMeatTag`)
  - bowl sequence is strict: `Noodles -> Broth -> Toppings`

## World Carry Item Interaction

- World carryables (`AARShopCarryItemBase`, including bowl/meat actors) expose `ForwardUseToController(AActor* UsingActor)` for BI_Interactable forwarding.
- World carryables expose `ForwardSecondaryUseToController(AActor* UsingActor)` for BI-style held-secondary forwarding (consume/throw/etc via held-item secondary behavior).
- World carryables expose `ForwardKickToController(AActor* UsingActor)` for BI-style world-item kick forwarding to controller kick requests (`AARPlayerController::RequestKickActor`).
- `ForwardUseToController(...)` resolves `AARShopPlayerController` (direct controller or pawn owner controller) and routes to `RequestShopPickupCarryItem(...)`.
- Shop station request APIs are intentionally owned by `AARShopPlayerController` (not `AARPlayerController`).
- Shop-only interaction requests live on `AARShopPlayerController`:
  - `RequestShopUseOrDrop(AActor*)` for one-shot input routing (`ForwardUseToController` when target exists, fallback drop when null)
  - `RequestShopPickupCarryItem(AARShopCarryItemBase*)`
  - `RequestShopDropHeldCarryItem()`
  - `RequestShopThrowHeldCarryItem(float ThrowStrength)`
    - when `ThrowStrength <= 0`, server resolves throw power from thrower GAS `Strength` (`Strength * 100`, so default Strength `10` => throw strength `1000`)
  - `RequestUseSecondaryOnHeldCarryItem()` generic held-secondary dispatch (routes to held item `AARShopCarryItemBase::UseSecondaryByController(...)`)
  - `RequestShopStationPlaceHeldMeat(AARShopStationActor*)`
  - `RequestShopStationPickupMeat(AARShopStationActor*)`
  - `RequestShopStationStartProcessing(AARShopStationActor*)`
  - `RequestShopStationTapProcessing(AARShopStationActor*)`
  - `RequestShopStationStopProcessing(AARShopStationActor*)`
  - `RequestShopFillHeldBowlFromStation(AARShopStationActor*)`
  - `RequestShopStationInteract(AARShopStationActor*)` smart station route:
    - held bowl -> `RequestShopFillHeldBowlFromStation(...)`
    - held meat and station slot empty -> `RequestShopStationPlaceHeldMeat(...)`
    - empty hands and station has slotted meat -> `RequestShopStationPickupMeat(...)`
- `RequestShopPickupCarryItem(nullptr)` is treated as a no-hit fallback: if the controller currently holds a carry item, it drops it.
- Actor-targeted shop RPC requests are server reachability-gated by controller pawn distance (`AARPlayerController::ServerInteractionMaxDistance`) before any station/dispenser/carry mutation runs.
- Hold-style interaction input should set/clear `AARPlayerController` active target fields (`ActiveInteractable`, `ActiveSecondaryInteractable`) and shared latch state (`bIsInteracting`) on press/release.
- While a hold-style target is active, `AARPlayerController` performs server-side periodic range validation (`ActiveInteractionRangeCheckInterval`) and triggers `IARInteractableRangeListener::OnPlayerOutOfRange(...)` on opted-in interactables before clearing active targets.
- Interaction outcome animation cues are emitted through `AARPlayerController::OnInteractionActionCue` (`NotifyInteractionActionCue(...)`), with current secondary defaults emitting `Throw`, `Consume`, and kick-style cues that classify as `Kick` vs `Slap` by target height delta above pawn (`SlapCueMinHeightDeltaCm`).
- `AARPlayerCharacterShop` exposes BP helpers `IsCarryingShopItem()` and `GetHeldShopActor()` for pawn-side input/UI branching.
- Shop player pawn selection is native via `AARShopGameMode::GetDefaultPawnClassForController_Implementation(...)`, resolving by canonical `Shop.Character.*` map keys (`ShopPawnClassByCharacterTag`) with `FallbackShopPawnClass` fallback so BP join-spawn/possess wiring is optional and should be removed when redundant.
- Spawn transform selection is GameMode-owned through `AARGameModeBase::ChoosePlayerStart_Implementation(...)` using `AARTaggedPlayerStart::SpawnIdentityTag` (editor display name: `Player Start Tag (Gameplay)`, gameplay-tag picker) with canonical character tags as the primary identity signal.
- `ChoosePlayerStart_Implementation(...)` performs an authority pre-pass to complete first-session setup (when needed) and normalize slot/character identity before evaluating spawn tags, so editor/raw-map starts do not pick a start from unresolved character state.
- `AARGameModeBase` caches the canonical character tag chosen in `ChoosePlayerStart_Implementation(...)` per controller for the current spawn attempt, and `AARShopGameMode` consumes that cache in pawn-class resolution so start selection and pawn class cannot diverge mid-spawn.
- First-join PlayerState hydration uses strict identity matching with canonical current-character tags.
- `AARGameModeBase::SpawnDefaultPawnAtTransform_Implementation(...)` performs a collision-adjusted fallback spawn (`AdjustIfPossibleButAlwaysSpawn`) when the engine default pawn spawn path fails at the chosen start, so blocked starts still produce a possessed pawn.
- `HandleStartingNewPlayer(...)` performs a one-time corrective respawn when character identity drifts across the initial spawn pass (for example pre-spawn `Brother` -> post-spawn `Sister`), ensuring final spawn transform aligns with final character identity.
- `HandleStartingNewPlayer(...)` performs a one-shot respawn retry only if the initial spawn path leaves the controller without a pawn, so direct-load/editor startup failures recover without requiring manual PIE restart.
- `AARShopCarryItemBase::UseSecondaryByController(...)` default behavior is throw; item subclasses can override for item-specific secondary behavior (for example `AAREnergyDrinkCarryItem` consumes instead of throwing).
- `AARShopCarryItemBase::UseSecondaryInWorldByController(...)` default behavior is a strength-scaled kick impulse for non-held world items (`Strength * 100`); subclasses can override if needed.
- Pickup is authority-validated and blocked when the item is already attached to another actor (for example station slot ownership).
- Carryables replicate movement so held/drop/throw transforms stay authoritative across listen-server + clients.
- Carry presentation/drop/throw physics resolve against a valid primitive component on the item (not strictly actor root), so carryable Blueprints can use `DefaultSceneRoot` as long as they include at least one world-colliding primitive component.
- Drop/throw restore world physics and gravity on the released carry item.
- Meat storage interaction contract:
  - `AARMeatStorageBoxActor::TryHandleStorageInteraction(...)` stores held meat when the interacting controller is holding `AARRamenMeatActor`; otherwise it dispenses from reserve.
  - reserve inventory is canonical by meat type tag (`FARMeatState::AdditionalAmountsByType`); legacy color buckets are compatibility mirrors.
  - `TryDispenseMeat(...)` uses random typed dispense by container color (`TryDispenseRandomMeatByContainerColor`), uniformly across eligible meat types.
  - `TryDispenseSpecificMeat(...)` supports explicit typed retrieval by `Item.Meat` tag.
  - color-only compatibility paths resolve to the first deterministic meat row for that color (sorted row-name order).
  - `AARRamenMeatActor` auto-attempts store on storage hit/overlap (`TryStoreWorldMeat`) against matching storage color; `None`/unspecified meat is accepted into a color-specific storage and stored under that storage color.
  - world auto-store is gated by travel-from-spawn distance (`MinWorldAutoStoreTravelDistance`) so freshly dispensed meat does not instantly return when spawned near/on storage.
  - intentional player pickup arms meat world-return (`AARRamenMeatActor::ArmStorageReturn` via carry component), allowing valid throw-back store even when travel-from-spawn gate would otherwise block.
- `AARShopCarryItemBase` exposes shared weight tuning: `WeightKg` (`0` = native primitive mass/default behavior, `>0` = explicit mass override in kg) for bowl/meat physics tuning.

## Persistence + Replication

- Station processing progress is replicated runtime state only (not persisted in `UARSaveGame`).
- Meat inventory remains save-facing through `AARGameStateBase::Meat`.
- Meat debug command `ar.debug.add_meat <delta> <Item.Meat.*|red|white|blue|colorless|none>` supports explicit type-tag adds or color-token deterministic resolution.
- Returning meat to storage (held interact or world-hit auto-store) increments typed GameState inventory and releases the world meat actor.
- Loose shop carryables use save-backed transient snapshots (`UARSaveGame::ShopTransientCarryables`) for reload-before-run continuity.
- Transient snapshot capture/restore scope currently includes loose world `AAREnergyDrinkCarryItem` and `AARRamenMeatActor` instances (held/attached actors are excluded).
- Character-owned shop restore snapshots live in `UARSaveGame::CharacterStates[]` and currently capture:
  - shop character transform
  - held supported carryable snapshot (`AAREnergyDrinkCarryItem`, `AARRamenMeatActor`, `AARRamenBowlActor`)
- Character shop restore applies only when re-entering `Mode.Shop` from a fresh save load of a save that was itself made in `Mode.Shop`.
- Fresh-load character restore skips controllers that are not ready yet and retries on later `RestartPlayer(...)` calls until the pending fresh-load entry is fully consumed.
- Clean shop entry from new game / invader / scrapyard does not apply character transform or held-item restore snapshots.
- Starting a run clears transient loose-carryable snapshots; invader/scrapyard completion marks a one-shot clear on next shop entry.

## Energy Drinks in Shop

- `AAREnergyDrinkCarryItem` is a replicated shop carryable (pickup/drop/throw via existing carry pipeline).
- Secondary action consume path is routed by `AARShopPlayerController::RequestConsumeHeldEnergyDrink`.
- Consume authority is shop-mode only.
- Stored drink inventory is authoritative pre-spawn; drinks spawned into shop anchors become world-owned instances and are removed from stored inventory count.
- Consuming a spawned world drink applies run-buff payload through `UARRunBuffSubsystem` using per-character ownership rules (no per-character duplicate of the same drink type).
- Shared item metadata (for example spawn actor class/weight) should resolve through `UARItemDefinitionSubsystem` so Shop and Scrapyard consume the same authored item rows.

## StateTree Integration

- Shop AI now has StateTree scaffolding:
  - `UARShopStateTreeAIComponent`
  - `UARShopStateTreeAIComponentSchema`
  - `AARShopAIController` start/event helpers
- `AARShopAIController` maps active `State.ShopNPC.*` tags to speaker dialogue gating:
  - dialogue allowed when `State.ShopNPC.DialogueWindow` is active
  - otherwise dialogue is locally blocked while non-dialogue shop states are active
  - dialogue gate automatically reopens when `State.ShopNPC` is not active and on controller unpossess cleanup.
- Customer component emits order lifecycle events (`Event.ShopNPC.OrderGenerated` / `Event.ShopNPC.OrderServed`) for StateTree-driven speaker behavior.
- Shop AI controller also bridges dialogue lifecycle into ShopNPC StateTree tags for the possessed speaker:
  - `Event.ShopNPC.ConversationOffered` when talkable becomes true.
  - `Event.ShopNPC.DialogueStarted` when a session starts for that speaker.
  - `Event.ShopNPC.DialogueEnded` when the speaker no longer has an active session.
  - `Event.ShopNPC.ConversationCompleted` when a completed conversation belongs to that speaker.
