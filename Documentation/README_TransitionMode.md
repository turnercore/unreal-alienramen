# Transition Mode Contract

This document defines the server-authoritative contract for the dedicated transition map flow (`Lvl_Loading` or equivalent).

Transition flow is the handoff layer between the three primary game modes:

1. Shop
2. Invader
3. Scrapyard

## Ownership

- `AARGameModeBase`
  - Shared mode-travel router for transition-map handoff.
  - `TryStartTravel` routes destination URLs through transition map when mode opts in.
  - Seamless-travel handoff resets carried spectator controller state back to playing via `HandleSeamlessTravelPlayer(...)` so gameplay modes repossess correctly after transition maps.
  - Seamless-travel handoff now swaps carried controllers to the destination mode's `PlayerControllerClass` when classes mismatch (for example transition/debug controllers entering gameplay modes), so mode-specific controller defaults and startup logic still apply.
  - Identity/slot normalization now considers only controller-owned active `PlayerState` instances during travel handoff to avoid stale seamless-copy remnants stealing `P1/P2` occupancy.
  - Per-call route override is supported via `EARTravelRoutePolicy`:
    - `ModeDefault`
    - `ForceTransitionMap`
    - `ForceDirect`
  - Blueprint convenience wrappers:
    - `EndModeAndTravel(...)` (forced transition-map route)
    - `TravelDirectInMode(...)` (forced direct map travel)
  - Route config is mode-owned via:
    - `bRouteModeTravelThroughTransitionMap`
    - `TransitionTravelMapURL`
    - `TransitionSourceMode`
    - `TransitionReason`
- `AARTransitionGameMode`
  - Authority owner of transition continue-gate behavior.
  - Native class is abstract; maps should use a Blueprint subclass.
  - Resets player travel-ready flags on transition entry (configurable).
  - Auto-advances to destination when all active players are ready.
  - Final destination hop now uses absolute server travel to avoid leaking prior map URL options into the destination mode (for example stale `game=` overrides).
  - Spawns no gameplay pawn in transition mode.
- `AARTransitionGameState`
  - Replicated read model for transition context (`FARTransitionContext`).
  - Exposes source mode, transition reason, destination URL, and fresh-load flag to UI/widgets.
- `AARTransitionPlayerController`
  - BP/UI entrypoint for continue voting (`RequestTransitionContinue`).
  - Native class is abstract; transition maps should use a Blueprint subclass via GameMode defaults.
  - BP/UI transition widget host:
    - `ShowTransitionWidgetFromContext`
    - `ShowTransitionWidget(WidgetClass)`
    - `HideTransitionWidget`
    - `ResolveTransitionWidgetClassFromContext`
  - Context-based widget resolution supports overrides by:
    - `bFreshLoadEntry` (`FreshLoadTransitionWidgetClass`)
    - `Reason` (`TransitionWidgetClassByReason`)
    - `SourceMode` (`TransitionWidgetClassBySourceMode`)
    - fallback (`DefaultTransitionWidgetClass`)

## Travel Context

Transition context is passed by travel URL options:

- `ARTrSource` (`EARTransitionSourceMode`)
- `ARTrReason` (`EARTransitionReason`)
- `ARTrDest` (destination map URL/path)
- `ARTrFresh` (`0/1`)

Helpers live in `ARTransitionTypes`:

- `ARTransition::AppendTravelOptions` (normalizes travel option separators to UE-style repeated `?`)
- `ARTransition::EnsureTravelOption` (token-safe option injection, for example `listen`)
- `ARTransition::BuildTransitionTravelURL`
- `ARTransition::AppendTransitionContextOptions`
- `ARTransition::ApplyTransitionContextFromTravelOptions`

Blueprint wrappers:

- `UARTransitionBlueprintLibrary::MakeTransitionContext`
- `UARTransitionBlueprintLibrary::BuildTransitionTravelURL`
- `UARTransitionBlueprintLibrary::ApplyTransitionContextFromTravelOptions`

## Expected Flow

1. Source mode finalizes authoritative runtime state (economy/rewards/etc).
2. Source mode calls `TryStartTravel` / `EndModeAndTravel`; router emits transition-map URL + context options.
   - Caller-supplied destination travel options are embedded into `ARTrDest` and preserved through the transition-map leg.
3. Transition map displays results/loading UI.
4. Players submit continue-ready votes.
5. Transition mode auto-travels to `TransitionContext.DestinationURL` when all are ready.
   - Travel is executed as absolute URL handoff so destination map defaults (GameMode/PlayerController) resolve deterministically.
6. The final gameplay map receives the same transition context in its own travel options.

## Direct Same-Mode Travel

For stage-to-stage travel where mode class should stay the same (for example Invader map A -> Invader map B), use direct routing:

- C++: `TryStartTravel(DestinationURL, ..., EARTravelRoutePolicy::ForceDirect)`
- BP (GameMode): `TravelDirectInMode(DestinationURL, ...)`
- BP (Controller): `TryStartTravel` with `RoutePolicy = ForceDirect`

## Current Wiring

- Shop mode defaults to `Shop -> Transition -> Invader` via `AARGameModeBase::TryStartTravel`.
- Invader mode defaults to `Invader -> Transition -> Scrapyard` via `AARGameModeBase::TryStartTravel`.
- Scrapyard finalization defaults to `Scrapyard -> Transition -> Shop` by resolving final URL through `AARGameModeBase::BuildModeTravelURL` before authority travel request.
- Any mode can disable transition-map routing by setting `bRouteModeTravelThroughTransitionMap=false` in that mode class/defaults.

## Blueprint Wiring Quick Guide

- From a gameplay mode (`AARGameModeBase` subclass):
  - End current mode and show transition: call `EndModeAndTravel(DestinationURL, ...)`.
  - Same-mode map hop (no transition map): call `TravelDirectInMode(DestinationURL, ...)`.
  - Generic path: call `TryStartTravel(..., RoutePolicy)` and pass `ModeDefault` / `ForceTransitionMap` / `ForceDirect`.
- From shop runtime state (`AARShopGameState`):
  - Explicit shop-exit helper: call `FinalizeShopRunAndTravelToInvader(InInvaderTravelURL)`.
  - Leave `InInvaderTravelURL` empty to use `DefaultInvaderTravelURL`.
  - Pass the final gameplay destination (for example `/Game/Maps/Lvl_Invader`), not the transition map URL.
- From controller/UI during gameplay:
  - Call `AARPlayerController::TryStartTravel(..., RoutePolicy)`; server authority resolves final URL and performs save/travel gate checks.
  - Shop-specific convenience: `AARShopPlayerController::RequestFinalizeShopRunAndTravelToInvader(...)` routes via server RPC to the authoritative shop finalization helper.
- From main menu / non-`AARGameModeBase` map:
  - Build URL with `UARTransitionBlueprintLibrary`:
    1. `MakeTransitionContext(SourceMode, Reason, DestinationURL, bFreshLoadEntry)`
    2. `BuildTransitionTravelURL(TransitionMapURL, Context)`
  - Host/listen server then opens that URL.

## Save-Load Entry

- `UARTravelSubsystem::TravelToLoadedSaveDestination(...)` is the standard gameplay-entry path after `LoadGame(...)`.
- It builds transition context with:
  - `SourceMode=SaveLoad`
  - `Reason=SaveLoadEntry`
  - `bFreshLoadEntry=true`
- Fresh-load-only gameplay restore logic should key off this context and the matching save-subsystem one-shot signal instead of inventing separate load-entry flags per mode.
