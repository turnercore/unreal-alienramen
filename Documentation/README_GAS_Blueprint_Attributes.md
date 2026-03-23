# GAS Attributes in Blueprints (UI + Gameplay)

This doc covers Blueprint-facing attribute reads/writes after the Core/Player/Enemy split.

## Source of Truth

- Character-owned player runtime lives on `AARCharacterStateRuntime` (ASC owner).
- Active pawn (`AARPlayerCharacterInvader` in Invader) is ASC avatar.
- `AARPlayerStateBase` is the player-owned pointer/projection surface and convenience read API for the active runtime.
- Enemy runtime lives on `AAREnemyBase` with `UARAttributeSetCore` + `UAREnemyAttributeSet`.
- Attributes replicate through GAS/AttributeSet replication.
- UI should read from `AARPlayerStateBase`, not from the pawn.
- For ASC-driven widgets, use the ASC widget bases below and bind to the character/enemy ASC source directly.

## ASC Widget Bases (Event-Driven)

The project includes a reusable widget stack for direct ASC delegate binding:

- `UARASCAttributeWidgetBase`
  - Generic base for anything exposing an ASC.
  - Initialize via `InitializeFromASC` or `InitializeFromActor`.
  - Emits `OnASCAttributeWidgetTrackedAttributeChanged(AttributeName, NewValue, OldValue)`.
  - Cleans up delegates in `DeinitializeASCAttributeWidget` and `NativeDestruct`.
- `UARPlayerASCAttributeWidgetBase`
  - Tracks core attributes (`Health`, `MaxHealth`, `Spice`, `MaxSpice`, `MoveSpeed`, `Strength`).
  - Tracks player primary lane (`Damage`, `FireRate`, `Ammo`, `MaxAmmo`).
  - Tracks player hat lane (`HatEnergy`, `MaxHatEnergy`, `HatEnergyRegenRate`, `HatPower`).
- `UAREnemyASCAttributeWidgetBase`
  - Tracks core attributes plus enemy `CollisionDamage`.

## PlayerState Blueprint APIs

### Core reads

- `GetCoreAttributeValue(EARCoreAttributeType)`
- `GetCoreAttributeSnapshot()`

`EARCoreAttributeType`:

- `Health`
- `MaxHealth`
- `MoveSpeed`

### Player reads

- `GetPlayerAttributeValue(EARPlayerAttributeType)`
- `GetPlayerAttributeSnapshot()`
- `GetSpiceNormalized()`

`EARPlayerAttributeType`:

- `Spice`
- `MaxSpice`
- `Strength`

### Player writes

- `SetSpiceMeter(float)`
- `ClearSpiceMeter()`
- `SetStrength(float)`

## Blueprint Delegates

Generic:

- `OnCoreAttributeChanged(AttributeType, NewValue, OldValue)`
- `OnPlayerAttributeChanged(AttributeType, NewValue, OldValue)`

Specific convenience delegates:

- `OnHealthChanged`
- `OnMaxHealthChanged`
- `OnMoveSpeedChanged`
- `OnSpiceChanged`
- `OnMaxSpiceChanged`
- `OnStrengthChanged`

## Recommended UI Pattern

1. Resolve target `AARPlayerStateBase`.
2. Bind to specific delegates needed for that widget.
3. Pull both snapshots on construct:
   - `GetCoreAttributeSnapshot()`
   - `GetPlayerAttributeSnapshot()`
4. Unbind on destruct/remove.

Use delegate-driven updates instead of tick polling.

## Invader Spicy Track Notes

- Shared track/full-blast runtime remains on `AARInvaderGameState`.
- Player spice state remains per-player on `AARPlayerStateBase`/runtime player attributes.
- Multiplayer interaction APIs are unchanged (`RequestActivateFullBlast`, `ResolveFullBlastSelection`, `StartSharingSpice`, etc.).
