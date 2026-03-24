# Invader Docs

Invader is the combat run mode between Shop and Scrapyard.

Use this section when you are working on player combat runtime, loadouts, waves, pickups, scoring, or run orchestration.

## Start Here By Concern

| Concern | Start here | Then read |
| --- | --- | --- |
| player pawn, loadouts, abilities, attributes | [Loadouts and Player Runtime](README_Invader_Loadouts.md) | [GAS Overview](README_GAS.md) |
| pickups and reward collection | [Pickups](README_InvaderDrops.md) | [Scoring](README_Invader_Scoring.md) |
| wave flow, stages, director settings | [Invader Director](README_Invader_Director.md) | [Wave Designer](README_Invader_WaveDesigner.md) |
| shared full-blast and spicy-track runtime | [Invader Spicy Track](CppOverview/InvaderSpicyTrack.md) | [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md) |
| class-level runtime inventory | [Invader C++ Overview](CppOverview/README.md) | [Doxygen HTML](/unreal-alienramen/doxygen/index.html) |

## Core Pages

- [Loadouts and Player Runtime](README_Invader_Loadouts.md)
- [Pickups](README_InvaderDrops.md)
- [Scoring](README_Invader_Scoring.md)
- [Invader Director](README_Invader_Director.md)
- [Wave Designer](README_Invader_WaveDesigner.md)
- [Invader Spicy Track](CppOverview/InvaderSpicyTrack.md)

## Shared Systems Used By Invader

- [GAS Overview](README_GAS.md)
- [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md)
- [Progression and Unlocks](README_ProgressionUnlocks.md)
- [Networking Overview](README_Networking.md)
- [Persistence Overview](README_Persistence.md)
- [Transition Flow](README_TransitionMode.md)

## Controller Lifecycle Note

- Invader controller runtime should not rely on `BeginPlay` as the only setup hook after transition-map seamless travel.
- `AARInvaderPlayerController::SetPawn` now rebinds invader GameState wiring when controller instances persist across travel.
- Camera ownership should remain in the Invader mode/Blueprint shared-camera flow; native `SetPawn` should not force view target to pawn.

## Character Runtime Ownership Note

- Character combat/loadout runtime state is authoritative on `AARCharacterStateRuntime`, not on `AARPlayerStateBase`.
- Attribute ownership is split by domain:
  - `UARAttributeSetCore` for shared combat/runtime attributes (`Health`, `MaxHealth`, `MoveSpeed`, etc.)
  - `UARAttributeSetPlayer` for player-owned attributes (`Spice`, `MaxSpice`, `Strength`, pickup/drop multipliers, weapon lane stats)
  - `UAREnemyAttributeSet` for enemy-only attributes (`CollisionDamage`, `DropChance`, `DropAmount`)
- `UARCharacterSubsystem` is orchestration/lookup only (runtime registry, spawn/rebind/swap routing) and is not a replicated data owner.
- `AARPlayerStateBase` remains player-owned: identity, slot/profile, readiness, dialogue preference, and current selected character pointer.
- Picked/activated Invader upgrades remain character-owned state on `AARCharacterStateRuntime` / `FARCharacterSaveData::InvaderRuntime`.
- Shared spicy-track offers and slotted team-availability remain `AARInvaderGameState` state and may be cached/restored for Invader/Scrapyard flow, but they are not part of a character/player interaction identity and must not be injected into combined interaction tags.
- Invader pawns must resolve gameplay state from the runtime bound to that pawn in `UARCharacterSubsystem`, not by falling back to the player state's currently selected runtime.
- This is what allows cooldowns, active gameplay effects, and loadout-owned GAS state to keep advancing correctly on unpossessed ships.
- Ship/hat loadout baseline application is part of runtime-pawn initialization. Possession may grant controller-scoped input abilities, but it must not clear and rebuild the character-owned loadout baseline on every swap.
- Invader now opts into the shared `AARGameModeBase` gameplay bootstrap helper for `BeginPlay`, join/restart repair, and seamless-travel repair.
- Invader mode startup should preserve this sequence: hydrate character runtime data, materialize both character pawns, bind runtime-to-pawn and player-state-to-runtime references, then perform the final possession handoff.
- Invader-specific work that remains outside the shared helper: director binding/run-end flow, run-buff application wrappers, and post-bootstrap inactive-pawn damage-state repair.
