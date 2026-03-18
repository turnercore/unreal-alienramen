# Invader Loadouts and Player Runtime

Loadouts are a shared persistence + GAS-backed system consumed most heavily by Invader runtime.

For day-to-day work, this is the main entry point for the Invader player pawn / ship / PlayerState ability flow.

## Current Runtime Shape

- Loadouts are canonical character-owned state in `FARCharacterSaveData` and runtime-owned on `AARCharacterStateRuntime`.
- `AARCharacterStateRuntime` owns ASC + `UARAttributeSetCore` and per-character runtime state (loadout, downed/dead, invader runtime).
- `AARPlayerStateBase` is player-owned only (identity, readiness/preferences, current selected character pointer).
- `UARCharacterSubsystem` coordinates runtime lookup, swap execution, spawn/rebind, and runtime-to-pawn/controller resolution.
- The Invader pawn/avatar initializes ASC actor info with `OwnerActor = AARCharacterStateRuntime`, `AvatarActor = Pawn`.
- Server possession now treats loadout init as complete only after ASC is valid and ship baseline applies successfully; otherwise it keeps retrying and logs the blocking reason.
- Common controller ability-set grants are tied to that same retryable init path so they are not lost when possession order races happen in-editor.
- Possession by non-`AARPlayerController` no longer hard-aborts ship loadout initialization; pawn-side ship baseline abilities/effects still initialize while controller-common ability set grant is skipped until/if a gameplay controller is present.
- Inactive player-character pawns remain spawned and unpossessed by default; swap flow re-possesses existing pawns when available.
- UI should read replicated attributes through `AARPlayerStateBase` convenience accessors, which resolve the active character runtime.

## Data Table Row Contracts

- Canonical loadout row structs live in `Source/AlienRamen/Public/ARLoadoutTypes.h`.
- Hat rows use `FARHatDefRow` (`Unlock.Hat.*`) and currently expose:
  - `DisplayName`
  - `Description`
- Ship rows use `FARShipDefRow` (`Unlock.Ship.*`) and expose:
  - identity: `DisplayName`, `Description`
  - baseline gameplay: `Stats`, `PrimaryWeapon`, `StartupAbilities`, `StartupEffects`, `ShipTags`, `MovementType`
  - mode-specific pawn classes: `ScrapyardPawnClass`, `DummyPawnClass`, `InvaderPawnClass`
- Runtime consumers resolve ship fields by property name (reflection), so these names are contract-critical and should remain stable.
- Ship baseline hydrate logs now explicitly warn when `StartupAbilities` is missing on the resolved row struct and print that struct's property list to speed up `DT_Ships` contract debugging after row/schema rebuilds.

## Gravity Frame Notes

- `AARPlayerCharacterInvader` applies invader gravity using `UCharacterMovementComponent::SetGravityDirection`.
- Runtime now also rotates actor up-vector to match `UARInvaderDirectorSettings::InvaderDesiredUpDirection` during BeginPlay/possess/PlayerState replication updates.
- If players spawn with wrong orientation, check `[InvaderGravity]` logs first to verify desired up, gravity direction, and final actor up vector.

## What to read

- [Progression and unlocks](README_ProgressionUnlocks.md)
- [GAS overview](README_GAS.md)
- [GAS Blueprint attributes](README_GAS_Blueprint_Attributes.md)
- [Persistence overview](README_Persistence.md)

These pages define unlock tags, attribute-driven runtime behavior, persistence ownership, and the Blueprint/UI surfaces used by the Invader player runtime.
