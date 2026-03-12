# Transition Mode Contract

This document defines the server-authoritative contract for the dedicated transition map flow (`Lvl_Loading` or equivalent).

## Ownership

- `AARGameModeBase`
  - Shared mode-travel router for transition-map handoff.
  - `TryStartTravel` routes destination URLs through transition map when mode opts in.
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
  - Resets player travel-ready flags on transition entry (configurable).
  - Auto-advances to destination when all active players are ready.
  - Spawns no gameplay pawn in transition mode.
- `AARTransitionGameState`
  - Replicated read model for transition context (`FARTransitionContext`).
  - Exposes source mode, transition reason, destination URL, and fresh-load flag to UI/widgets.
- `AARTransitionPlayerController`
  - BP/UI entrypoint for continue voting (`RequestTransitionContinue`).

## Travel Context

Transition context is passed by travel URL options:

- `ARTrSource` (`EARTransitionSourceMode`)
- `ARTrReason` (`EARTransitionReason`)
- `ARTrDest` (destination map URL/path)
- `ARTrFresh` (`0/1`)

Helpers live in `ARTransitionTypes`:

- `ARTransition::BuildTransitionTravelURL`
- `ARTransition::ApplyTransitionContextFromTravelOptions`

Blueprint wrappers:

- `UARTransitionBlueprintLibrary::MakeTransitionContext`
- `UARTransitionBlueprintLibrary::BuildTransitionTravelURL`
- `UARTransitionBlueprintLibrary::ApplyTransitionContextFromTravelOptions`

## Expected Flow

1. Source mode finalizes authoritative runtime state (economy/rewards/etc).
2. Source mode calls `TryStartTravel` / `EndModeAndTravel`; router emits transition-map URL + context options.
3. Transition map displays results/loading UI.
4. Players submit continue-ready votes.
5. Transition mode auto-travels to `TransitionContext.DestinationURL` when all are ready.

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
- From controller/UI during gameplay:
  - Call `AARPlayerController::TryStartTravel(..., RoutePolicy)`; server authority resolves final URL and performs save/travel gate checks.
- From main menu / non-`AARGameModeBase` map:
  - Build URL with `UARTransitionBlueprintLibrary`:
    1. `MakeTransitionContext(SourceMode, Reason, DestinationURL, bFreshLoadEntry)`
    2. `BuildTransitionTravelURL(TransitionMapURL, Context)`
  - Host/listen server then opens that URL.
