# Invader Drops Runtime Contract

Paths:
- `Source/AlienRamen/Public/ARInvaderDropBase.h`
- `Source/AlienRamen/Public/ARInvaderDropTypes.h`
- `Source/AlienRamen/Public/ARInvaderDirectorSettings.h`
- `Source/AlienRamen/Public/ARInvaderTypes.h`
- `Source/AlienRamen/Public/ARAttributeSetCore.h`
- `Source/AlienRamen/Public/ARPickupCollectorComponent.h`

## Authority / Ownership

- Enemy death ingestion remains server-authoritative in `AARInvaderGameState::NotifyEnemyKilled(...)`.
- Drop spawn, collection, and reward apply all run on authority.
- `AARInvaderDropBase` replicates and replicates movement for client visuals.

## Enemy Authoring Contract

- Enemy runtime row (`FARInvaderEnemyRuntimeInitData`) now authors:
- `DropType` (`None`, `Scrap`, `Meat`)
- `DropAmount` (baseline amount before variance/multipliers)
- collision toggles:
- `bCollideWithEnemies`
- `bCollideWithPlayers`
- `bCollideWithProjectiles`
- `bCollideWithDrops`
- Enemy authoring validation now enforces:
- `DropAmount >= 0`
- `DropAmount > 0` when `DropType != None`

## Attribute Contract

- `UARAttributeSetCore` now includes:
- `DropChance` (enemy-side, clamped `[0..1]`)
- `DropAmount` (enemy-side, non-negative)
- `MeatDropMultiplier` (killer-side, non-negative)
- `ScrapDropMultiplier` (killer-side, non-negative)
- Enemy runtime init writes:
- `DropAmount` from enemy definition
- `DropChance` from project settings default

## Project Settings Contract

- `UARInvaderDirectorSettings` now exposes:
- `DefaultEnemyDropChance` (default `0.5`)
- per-type base drop chance overrides:
- `DefaultEnemyScrapDropChance` (default `0.5`)
- `DefaultEnemyMeatDropChance` (default `0.2`)
- `DropAmountVarianceFraction` (default `0.25`)
- optional `DropAmountVarianceCurve` (sampled on `[0..1]`, output expected `[-1..1]`)
- per-type amount variance overrides:
- `ScrapDropAmountVarianceFraction` + optional `ScrapDropAmountVarianceCurve`
- `MeatDropAmountVarianceFraction` + optional `MeatDropAmountVarianceCurve`
- `ScrapDropStacks` / `MeatDropStacks` arrays (`Denomination` + `DropClass`) for stack-based spawn decomposition
- `DropInitialLinearSpeedMin/Max` for spawned drop XY scatter velocity
- `DropPawnCollisionMode` (`CollideWithPawns` or `IgnoreAllPawns`) for drift bumping behavior
- `bUseCapsuleDerivedPlayerPickupRadius` enables capsule-derived initialization (`Radius + Diameter`)
- `DefaultPlayerPickupRadius` is used when capsule-derived sizing is disabled

## Runtime Drop Resolution

- Drops only attempt when killer resolves to a player state.
- Enemy-owned/self/environment kills do not spawn drops.
- Roll flow:
1. Read enemy `DropType`.
2. Roll enemy `DropChance`.
3. Read enemy `DropAmount`.
4. Apply center-weighted variance using per-type settings (triangular fallback or optional curve).
5. Multiply by killer-side drop multiplier (`MeatDropMultiplier` or `ScrapDropMultiplier`).
6. Round to integer.
7. Build spawn plan:
- if stack definitions are configured, decompose amount into an optimal set (fewest pickups; ties prefer larger denominations)
- if exact decomposition is not possible, emit largest denomination pickups plus one remainder pickup using the lowest-denomination class
- if stack definitions are empty/invalid, no drop actor is spawned
8. Spawn one pickup actor per planned denomination amount.

## Pickup / Reward Contract

- `AARInvaderDropBase` physics:
- simulated physics enabled
- gravity disabled
- movement constrained to XY plane (Z translation locked)
- invader object channels are dedicated (`InvaderEnemy`, `InvaderPlayer`, `InvaderProjectile`, `InvaderDrop`) and used by enemy/player/projectile/drop runtime actors
- pawn collision during drift is controlled by `DropPawnCollisionMode`:
- `CollideWithPawns`: enemies/players can physically bump drifting drops
- `IgnoreAllPawns`: drifting drops ignore pawn collisions entirely
- player-side detector lives on `AARPlayerCharacterInvader` as `UARPickupCollectorComponent`:
- creates a query-only sphere detector and uses overlap events for nearby drops
- subscribes to owner ASC `PickupRadius` attribute-change delegate and updates detector radius immediately on change
- owning client starts predicted local collection visuals instantly
- owning client sends server collect requests via pawn RPC
- authority validates range/availability and starts authoritative collect
- uses a lightweight retry timer for currently overlapping drops (no per-frame world scan)
- player spawn/init writes a non-zero default into ASC `PickupRadius` on authority when current value is unset/non-positive
- On collection:
- physics/collision disabled
- fast lerp-to-player visual
- optional gameplay cue executes on collecting player's ASC
- predicted local visual rolls back automatically if server has not accepted collection within a short timeout
- authority applies reward to `AARGameStateBase`:
- `Scrap` increments `SetScrapFromSave(...)`
- `Meat` increments color bucket (`Red`, `Blue`, `White`, otherwise `Unspecified`) and commits via `SetMeatFromSave(...)`
