# Alien Ramen GAS Reference

This document summarizes the current Gameplay Ability System (GAS) ownership model in Alien Ramen.

GAS is documented under shared systems because multiple modes consume it, even though Invader is the primary gameplay surface.

## Where GAS Is Used

- Invader player runtime:
  - `AARCharacterStateRuntime` owns the ASC and character-owned attribute sets.
  - `AARPlayerStateBase` owns player identity/runtime pointers and exposes convenience read/write APIs.
  - Invader pawn/avatar initializes actor info with `OwnerActor = AARCharacterStateRuntime` and `AvatarActor = Pawn`.
- Invader enemy runtime:
  - `AAREnemyBase` owns enemy ASC + enemy/core attribute sets.
- Shop/Scrapyard integration:
  - shop throw/kick strength and other shared gameplay reads now resolve through player-owned attributes.
  - ship/UI surfaces read replicated attributes through `AARPlayerStateBase`.

## Attribute Set Ownership (Hard Split)

### Core (`UARAttributeSetCore`)

Shared cross-actor combat/runtime attributes:

- `Health`
- `MaxHealth`
- `IncomingDamage`
- `Shield`
- `MaxShield`
- `HealthRegenRate`
- `HealthRegenDelay`
- `ShieldRegenRate`
- `ShieldRegenDelay`
- `DamageTakenMultiplier`
- `HealingReceivedMultiplier`
- `MoveSpeed`
- `Damage`
- `FireRate`

### Player (`UARAttributeSetPlayer`)

Player-only progression/combat/verb attributes:

- support and movement: `HealingDealtMultiplier`, `RepairRate`, `Strength`, `DodgeDistance`, `DodgeDuration`, `JumpDistance`, `JetpackFuel`, `MaxJetpackFuel`, `JetpackFuelRegenRate`, `JetpackFuelDrainRate`, `ReviveSpeed`, `PickupRadius`
- primary weapon lane: `ProjectileSpeed`, `Range`, `LockOnTime`, `SpreadMultiplier`, `CritChance`, `CritMultiplier`, `Ammo`, `MaxAmmo`
- secondary lane: `SecondaryDamage`, `SecondaryFireRate`, `SecondaryProjectileSpeed`, `SecondaryRange`, `SecondaryAmmo`, `SecondaryMaxAmmo`
- special lane: `SpecialDamage`, `SpecialFireRate`, `SpecialProjectileSpeed`, `SpecialRange`, `SpecialAmmo`, `SpecialMaxAmmo`
- spice/hat/rewards: `Spice`, `MaxSpice`, `SpiceGainMultiplier`, `SpiceDrainRate`, `SpiceShareRatio`, `HatEnergy`, `MaxHatEnergy`, `HatEnergyRegenRate`, `HatPower`, `MeatDropMultiplier`, `ScrapDropMultiplier`

### Enemy (`UAREnemyAttributeSet`)

Enemy-only combat/drop attributes:

- `CollisionDamage`
- `DropChance`
- `DropAmount`

## Runtime API Surface

`AARPlayerStateBase` and `AARCharacterStateRuntime` now expose split read helpers:

- Core:
  - `GetCoreAttributeValue(EARCoreAttributeType)`
  - `GetCoreAttributeSnapshot()`
- Player:
  - `GetPlayerAttributeValue(EARPlayerAttributeType)`
  - `GetPlayerAttributeSnapshot()`

`EARCoreAttributeType` now contains only:

- `Health`
- `MaxHealth`
- `MoveSpeed`

`EARPlayerAttributeType` contains:

- `Spice`
- `MaxSpice`
- `Strength`

## Attributes That Are Commonly Missing (Candidates)

Not all games need these, but these are the highest-value candidates for this project:

### Damage Pipeline

- `OutgoingDamageMultiplier`
- `Armor` or `DamageReductionFlat`
- `ArmorPenetration` (or `DefensePenetration`)
- `StatusEffectPower` / `StatusEffectResistance`

Why: You already have `DamageTakenMultiplier` and core damage stats. These fill the offensive/defensive loop for upgrades and enemy archetypes.

### Survivability/Recovery

- `ShieldBreakDelayMultiplier`
- `LifeSteal` (if wanted for specific loadouts)
- `HealingReceivedFlatBonus`

Why: Useful for ship identities and rogue-like affixes.

### Mobility/Handling

- `AccelerationMultiplier`
- `TurnRate` or `AimTurnRate`
- `DashCooldownReduction`

Why: You have distance/duration but not handling feel knobs.

### Weapon Economy

- `ReloadSpeedMultiplier`
- `MagazineSizeMultiplier` (if reload model is added)
- `CooldownReduction`

Why: You currently have fire rate and ammo pools. These support richer weapon progression.

### Utility/Co-op

- `InteractSpeed`
- `ReviveRange`
- `ThreatGeneration` (if enemy aggro logic evolves)

## Related Docs

- [Invader Loadouts and Player Runtime](README_Invader_Loadouts.md)
- [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md)
- [Invader Pickups](README_InvaderDrops.md)
- [Shop Runtime Contract](README_ShopRamenSystem.md)
