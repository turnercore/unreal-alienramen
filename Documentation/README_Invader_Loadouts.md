# Invader Loadouts and Player Runtime

Loadouts are a shared persistence + GAS-backed system consumed most heavily by Invader runtime.

For day-to-day work, this is the main entry point for the Invader player pawn / ship / PlayerState ability flow.

## Current Runtime Shape

- Loadouts are canonical character-owned state in `FARCharacterSaveData` and runtime-owned on `AARCharacterStateRuntime`.
- `AARCharacterStateRuntime` owns ASC + `UARAttributeSetCore` + `UARAttributeSetPlayer` and per-character runtime state (loadout, downed/dead, invader runtime).
- `AARPlayerStateBase` is player-owned only (identity, readiness/preferences, current selected character pointer).
- `UARCharacterSubsystem` coordinates runtime lookup, swap execution, spawn/rebind, and runtime-to-pawn/controller resolution.
- The Invader pawn/avatar initializes ASC actor info with `OwnerActor = AARCharacterStateRuntime`, `AvatarActor = Pawn`.
- Server possession now treats loadout init as complete only after ASC is valid and ship baseline applies successfully; otherwise it keeps retrying and logs the blocking reason.
- `InitAbilityActorInfo()` is bind-only: it refreshes ASC owner/avatar wiring, local delegates, and movement/runtime mirrors, but it must not invoke the retryable server loadout application path.
- Retryable server loadout application may refresh ASC binding first, but it must remain a one-way flow and must not call back into the top-level pawn init entrypoints.
- Ship `Stats` gameplay effects are required runtime state for `FARShipDefRow`. A missing/failed/invalid ship stats apply must stay deferred instead of silently marking the runtime as initialized with zero combat/movement baseline.
- After ship baseline gameplay effects are applied, the pawn now explicitly re-syncs `CharacterMovement` from the runtime `MoveSpeed` attribute instead of relying only on an attribute-change delegate edge.
- Because the ASC now lives on `AARCharacterStateRuntime`, Invader bootstrap performs one more post-possession runtime-state check for the controlled pawn so controller-sensitive ship baseline effects can succeed after the final possession handoff.
- Common controller ability-set grants are tied to that same retryable init path so they are not lost when possession order races happen in-editor.
- Possession by non-`AARPlayerController` no longer hard-aborts ship loadout initialization; pawn-side ship baseline abilities/effects still initialize while controller-common ability set grant is skipped until/if a gameplay controller is present.
- Inactive player-character pawns remain spawned and unpossessed by default; swap flow re-possesses existing pawns when available.
- `AARInvaderGameMode` now materializes missing inactive character pawns for canonical playable identities during mode startup/restart using each character's own loadout state, falling back to `UARLoadoutSettings::DefaultPlayerLoadoutTags` when that character has no saved/runtime loadout yet.
- Invader pawn-class resolution is character-owned, not just active-player-owned: `GetDefaultPawnClassForController_Implementation(...)` and inactive-pawn materialization both resolve `Unlock.Ship.*` from the target character's runtime/save/default loadout before loading `FARShipDefRow::InvaderPawnClass`.
- Unpossessed invader ship pawns are explicitly damage-immune until they are possessed again.
- UI should read replicated attributes through `AARPlayerStateBase` convenience accessors, which resolve the active character runtime.
- `UARAttributeSetCore` no longer seeds an implicit `MaxHealth = 100` fallback in its constructor; zero/default health baselines must come from save hydration or authored gameplay effects.
- Max-attribute clamps in `PreAttributeChange(...)` should mutate `FGameplayAttributeData` directly instead of calling GAS attribute setters, or GAS can re-enter the same clamp path and overflow the stack.
- Attribute reads are split:
  - shared combat values via `GetCoreAttributeValue(EARCoreAttributeType)` (`Health`, `MaxHealth`, `MoveSpeed`)
  - player-owned values via `GetPlayerAttributeValue(EARPlayerAttributeType)` (`Spice`, `MaxSpice`, `Strength`)

## Data Table Row Contracts

- Canonical loadout row structs live in `Source/AlienRamen/Public/ARLoadoutTypes.h`.
- Hat rows use `FARHatDefRow` (`Unlock.Hat.*`) and currently expose:
  - `DisplayName`
  - `Description`
- Ship rows use `FARShipDefRow` (`Unlock.Ship.*`) and expose:
  - identity: `DisplayName`, `Description`
  - baseline gameplay: `Stats`, `PrimaryWeapon`, `StartupAbilities`, `StartupEffects`, `ShipTags`
  - mode-specific pawn classes: `ScrapyardPawnClass`, `InvaderPawnClass`
- Runtime consumers resolve ship rows as typed `FARShipDefRow` contracts (no reflective fallback prefixes).
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
