# GAS Attributes in Blueprints (UI + Gameplay)

This doc explains the practical Blueprint flow for reading and reacting to GAS attributes in Alien Ramen.

Use this page after [GAS Overview](README_GAS.md) when the question is specifically about replicated Blueprint/UI access.

## Common Entry Points

- Invader player HUD and teammate panels: [Invader Loadouts and Player Runtime](README_Invader_Loadouts.md)
- Scrapyard ship/UI surfaces: [Scrapyard Ships](README_Scrapyard_Ships.md)
- Shared system ownership: [GAS Overview](README_GAS.md)

## Source of Truth

- Player-character attributes live on `AARCharacterStateRuntime` (ASC owner).
- Active pawn (`AARPlayerCharacterInvader` in Invader) is the ASC avatar.
- `AARPlayerStateBase` is the player-owned pointer/projection surface and convenience read API for the active runtime.
- Attributes replicate through GAS/AttributeSet replication.
- UI should read from PlayerState, not from the pawn.
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

## What You Can Use in Blueprints

`AARPlayerStateBase` exposes:

- `GetCoreAttributeValue(EARCoreAttributeType)`
- `GetCoreAttributeSnapshot()`
- `GetSpiceNormalized()`
- `SetSpiceMeter(float)`
- `ClearSpiceMeter()`
- `GetInvaderPlayerColor()`
- `GetInvaderComboCount()`
- `HasActivatedInvaderUpgrade(Tag)`
- `GetActivatedInvaderUpgrades()`
- `SetPredictedSpiceValue(float)` (local UI prediction only)
- `ClearPredictedSpiceValue()`

Blueprint-assignable events:

- `OnCoreAttributeChanged(AttributeType, NewValue, OldValue)`
- `OnHealthChanged(NewValue, OldValue)`
- `OnMaxHealthChanged(NewValue, OldValue)`
- `OnSpiceChanged(NewValue, OldValue)`
- `OnMaxSpiceChanged(NewValue, OldValue)`
- `OnMoveSpeedChanged(NewValue, OldValue)`
- `OnInvaderPlayerColorChanged(NewColor, OldColor)`
- `OnInvaderComboChanged(SourcePlayerState, SourceCharacterTag, NewCombo, OldCombo)`
- `OnInvaderActivatedUpgradesChanged(...)`
- `OnPredictedSpiceChanged(PredictedSpice, AuthoritativeSpice, bHasPrediction)`

## Invader Spicy Track Runtime

- Shared track/full-blast runtime is owned by `AARInvaderGameState` (server-authoritative).
- UI/gameplay should treat PlayerState as per-player data and InvaderGameState as shared-track data.
- Core Invader GameState BP entry points:
- `RequestActivateFullBlast(PlayerState)`
- `ResolveFullBlastSelection(PlayerState, UpgradeTag, DestinationSlot)`
- `ResolveFullBlastSkip(PlayerState)`
- `ActivateTrackUpgrade(PlayerState, SlotIndex)`
- `StartSharingSpice(PlayerState)` / `StopSharingSpice(PlayerState)`
- Multiplayer client flow is available via `AARInvaderPlayerController` request RPC wrappers for the same actions.

Core enum:

- `EARCoreAttributeType::Health`
- `EARCoreAttributeType::MaxHealth`
- `EARCoreAttributeType::Spice`
- `EARCoreAttributeType::MaxSpice`
- `EARCoreAttributeType::MoveSpeed`

## UI Pattern (Recommended)

For each HUD widget (local player and teammate panels):

1. Resolve the target `AARPlayerStateBase`.
2. Bind to specific events you need (`OnHealthChanged`, `OnSpiceChanged`, etc.).
3. Immediately pull a snapshot using `GetCoreAttributeSnapshot()` to initialize the UI.
4. On widget destruct/remove, unbind from delegates.

This avoids polling and keeps HUD reactive.

## Showing Both Players

Use `GameState.PlayerArray` and cast entries to `AARPlayerStateBase`.

- For local panel: bind to local player's PlayerState.
- For teammate panel: bind to the other replicated PlayerState.

Because the attributes are replicated on runtime ASC/AttributeSet, both players can see each other's values through PlayerState reads.

## Writing Attribute Values

### Normal gameplay tuning

Prefer Gameplay Effects for buffs/debuffs and ongoing systems.

### Explicit Spice reset/use-case control

Use PlayerState helpers:

- `ClearSpiceMeter()` for hard reset (level transitions, full blast teardown, etc.).
- `SetSpiceMeter(Value)` for controlled assignment.

These are server-authoritative:

- If called on server, they apply immediately.
- If called on client, they route through server RPC internally.

## Move Speed and Character Movement

`MoveSpeed` is a GAS attribute and is now synced into pawn movement:

- `AARShipCharacterBase` updates `CharacterMovement.MaxWalkSpeed` and `MaxFlySpeed` whenever `MoveSpeed` changes.
- `AAREnemyBase` already does the same.

So GE-based slow/haste effects on `MoveSpeed` are reflected in actual movement for players and enemies.

## Practical Tips

- Do not shadow `LoadoutTags` or core GAS values in BP variables.
- Keep UI reads on PlayerState.
- Use delegate-driven updates over Tick polling.
- Use `OnCoreAttributeChanged` for generic widgets and specific events for simpler widgets.

## Quick BP Example (Health Bar)

1. In widget, store `TargetPlayerState` (`AARPlayerStateBase`).
2. Bind `TargetPlayerState.OnHealthChanged` and `OnMaxHealthChanged`.
3. In either event, compute `Percent = Health / MaxHealth`.
4. Set progress bar.
5. On construct, call `GetCoreAttributeSnapshot()` once and initialize.

## Quick BP Example (Spice Meter)

1. Bind `OnSpiceChanged` and `OnMaxSpiceChanged`.
2. Or call `GetSpiceNormalized()` directly for percent fill.
3. For reset flows, call `ClearSpiceMeter()` on the owning player state.
