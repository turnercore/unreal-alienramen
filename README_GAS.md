# Alien Ramen GAS Reference

This document summarizes the current Gameplay Ability System (GAS) attribute model in `UARAttributeSetCore` and gives practical guidance for next steps.

## Current GAS Setup (Quick Context)

- Ability System Component (ASC) is owned by PlayerState (`AARPlayerStateBase`).
- Pawn (`AARShipCharacterBase`) initializes ASC actor info and applies loadout-driven abilities/effects/tags.
- A single shared attribute set is used today: `UARAttributeSetCore`.

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

### Gadget System

- `GadgetEnergy`
- `MaxGadgetEnergy`
- `GadgetEnergyRegenRate`
- `GadgetPower`

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
