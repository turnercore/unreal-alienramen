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
- `HeadshotMultiplier` (if hit zones matter)
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

Why: Supports role differentiation in co-op.

## One Shared AttributeSet vs Separate Player/Enemy Sets

Short answer: keeping one shared set right now is fine.

### When One Shared Set Is Best

- Players and enemies share most combat math.
- You want fewer systems to maintain.
- You are still iterating quickly on game rules.

This appears to match the current project state.

### When To Split

Split only when one of these becomes true:

- Enemy-only stats significantly outnumber shared stats.
- You need strict authority/perf tuning per actor type.
- You keep adding player-only fields that enemies never use (or vice versa), creating noisy data and UI confusion.

### Recommended Path

- Keep `UARAttributeSetCore` as the shared base now.
- If divergence grows, add layered sets instead of a hard fork:
  - `UARAttributeSetCore` (shared)
  - `UARAttributeSetPlayer` (player-only)
  - `UARAttributeSetEnemy` (enemy-only)

This preserves current gameplay while scaling cleanly.

## Practical Notes

- Existing clamp/replication behavior in `UARAttributeSetCore` is strong and already production-friendly.
- If you add new attributes, mirror the current pattern:
  - Accessors in header
  - Clamp logic in `PreAttributeChange` / `PostGameplayEffectExecute`
  - `DOREPLIFETIME_CONDITION_NOTIFY` entry
  - Rep notify function

