# Alien Ramen GAS Reference

This document summarizes the current Gameplay Ability System (GAS) ownership model in Alien Ramen and the attribute model in `UARAttributeSetCore`.

GAS is documented under the shared plugin/runtime section because it supports more than one mode, even though Invader is the main gameplay consumer.

## Where GAS Is Used

- Invader player runtime:
  - `AARPlayerStateBase` owns the ASC
  - the Invader pawn/avatar initializes actor info and applies loadout-driven abilities/effects/tags
- Shop integration:
  - shop throw strength falls back to thrower GAS `Strength`
- Scrapyard integration:
  - ship/UI surfaces read replicated attributes through `PlayerState`

## Start Here If

- you are changing player loadouts, abilities, or attribute-driven combat behavior in Invader
- you need the replicated attribute read model for UI or Blueprint
- a non-Invader mode is consuming shared player attributes and you want the authoritative contract

## Current GAS Setup (Quick Context)

- Ability System Component (ASC) is owned by PlayerState (`AARPlayerStateBase`).
- Pawn (`AARPlayerCharacterInvader`) initializes ASC actor info and applies loadout-driven abilities/effects/tags.
- A single shared attribute set is used today: `UARAttributeSetCore`.
- Loadout terminology uses `Hat` (`Unlock.Hat`).

## Related Docs

- [Invader Loadouts and Player Runtime](README_Invader_Loadouts.md)
- [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md)
- [Shop Runtime Contract](README_ShopRamenSystem.md)
- [Scrapyard Ships](README_Scrapyard_Ships.md)

## Current Attributes In `UARAttributeSetCore`

### Survivability

- `Health`
- `MaxHealth`
- `Shield`
- `MaxShield`
- `HealthRegenRate`
- `HealthRegenDelay`
- `ShieldRegenRate`
- `ShieldRegenDelay`
- `DamageTakenMultiplier`
- `HealingReceivedMultiplier`

### Support

- `HealingDealtMultiplier`
- `RepairRate`
- `ReviveSpeed`
- `PickupRadius`

### Movement

- `MoveSpeed`
- `DodgeDistance`
- `DodgeDuration`
- `JumpDistance`
- `JetpackFuel`
- `MaxJetpackFuel`
- `JetpackFuelRegenRate`
- `JetpackFuelDrainRate`

### Combat - Primary

- `Damage`
- `FireRate`
- `ProjectileSpeed`
- `Range`
- `LockOnTime`
- `SpreadMultiplier`
- `CritChance`
- `CritMultiplier`
- `Ammo`
- `MaxAmmo`

### Combat - Secondary

- `SecondaryDamage`
- `SecondaryFireRate`
- `SecondaryProjectileSpeed`
- `SecondaryRange`
- `SecondaryAmmo`
- `SecondaryMaxAmmo`

### Combat - Special

- `SpecialDamage`
- `SpecialFireRate`
- `SpecialProjectileSpeed`
- `SpecialRange`
- `SpecialAmmo`
- `SpecialMaxAmmo`

### Spice System

- `Spice`
- `MaxSpice`
- `SpiceGainMultiplier`
- `SpiceDrainRate`
- `SpiceShareRatio`

### Hat System

- `HatEnergy`
- `MaxHatEnergy`
- `HatEnergyRegenRate`
- `HatPower`

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
