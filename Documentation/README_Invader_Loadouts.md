# Invader Loadouts and Player Runtime

Loadouts are a shared persistence + GAS-backed system consumed most heavily by Invader runtime.

For day-to-day work, this is the main entry point for the Invader player pawn / ship / PlayerState ability flow.

## Current Runtime Shape

- Loadouts are persisted as player-character-owned state and projected onto `AARPlayerStateBase`.
- The Ability System Component is owned by `AARPlayerStateBase`.
- The Invader pawn/avatar initializes ASC actor info and applies loadout-driven abilities, effects, and tags.
- UI should read replicated attributes from `PlayerState`, not from a local pawn copy.

## What to read

- [Progression and unlocks](README_ProgressionUnlocks.md)
- [GAS overview](README_GAS.md)
- [GAS Blueprint attributes](README_GAS_Blueprint_Attributes.md)
- [Persistence overview](README_Persistence.md)

These pages define unlock tags, attribute-driven runtime behavior, persistence ownership, and the Blueprint/UI surfaces used by the Invader player runtime.
