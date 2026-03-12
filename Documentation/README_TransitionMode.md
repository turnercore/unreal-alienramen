# Transition Mode Contract

This document defines the server-authoritative contract for the dedicated transition map flow (`Lvl_Loading` or equivalent).

## Ownership

- `AARGameModeBase`
  - Shared mode-travel router for transition-map handoff.
  - `TryStartTravel` routes destination URLs through transition map when mode opts in.
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
2. Source mode server-travels to transition map with context options.
3. Transition map displays results/loading UI.
4. Players submit continue-ready votes.
5. Transition mode auto-travels to `TransitionContext.DestinationURL` when all are ready.

## Current Wiring

- Shop mode defaults to `Shop -> Transition -> Invader` via `AARGameModeBase::TryStartTravel`.
- Invader mode defaults to `Invader -> Transition -> Scrapyard` via `AARGameModeBase::TryStartTravel`.
- Scrapyard finalization defaults to `Scrapyard -> Transition -> Shop` by resolving final URL through `AARGameModeBase::BuildModeTravelURL` before authority travel request.
- Any mode can disable transition-map routing by setting `bRouteModeTravelThroughTransitionMap=false` in that mode class/defaults.
