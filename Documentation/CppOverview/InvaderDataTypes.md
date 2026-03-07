# Invader Data Types (`ARInvaderTypes.h`, `ARInvaderSpicyTrackTypes.h`)
Paths:
- `Source/AlienRamen/Public/ARInvaderTypes.h`
- `Source/AlienRamen/Public/ARInvaderSpicyTrackTypes.h`

## Core Enums
- `EAREnemyColor`: Red, White, Blue
- `EARInvaderDropType`: None, Scrap, Meat
- `EARWavePhase`: Active, Berserk
- `EARInvaderFlowState`: Combat, AwaitStageClear, StageChoice, Transition, Stopped
- `EARSpawnEdge`: Top, Left, Right
- `EARInvaderPlayerColor`: Unknown, Red, White, Blue
- `EARInvaderUpgradeClaimPolicy`: `SingleTeamClaim`, `PerPlayerClaim`, `Repeatable`

## Authoring Rows
- `FARWaveEnemySpawnDef`
- Enemy class, color, edge, delay, authored offset, per-spawn effects
- `FARWaveDefRow`
- `bEnabled`
- Threat/player/selection fields
- Formation lock + flip flags
- `WaveDuration`
- Spawn array, wave tags, wave-level enemy effects
- `FARStageDefRow`
- `bEnabled`
- Selection/duration/threat/health multipliers
- Required/blocked/banned tags
- Stage-level enemy effects
- Reward descriptor
- `FARInvaderEnemyDefRow`
- Enemy identifier, class/runtime init, `BaseSpiceKillValue`
- Runtime init includes invader drop authoring: `DropType` + `DropAmount`
- Runtime init also includes invader collision toggles: `bCollideWithEnemies`, `bCollideWithPlayers`, `bCollideWithProjectiles`, `bCollideWithDrops`
- `FARInvaderUpgradeDefRow`
- Upgrade identity tag + display fields
- On-activate and while-slotted gameplay effects
- Tier locks, unlock requirements, team claim policy
- Offer prerequisites (team activated upgrades)
- Activation prerequisites (activating-player upgrade history)

## Runtime Snapshot Types
- `FARWaveInstanceState`
- live wave info (id, row, phase, counts, stage, timestamps, color-swap)
- `FARInvaderRuntimeSnapshot`
- flow state, threat, run/stage timing
- stage choice + reward fields
- leak values and soft caps
- active wave array
- `FARInvaderTrackSlotState`
- Shared spicy-track slot state (`SlotIndex`, `UpgradeTag`, `UpgradeLevel`, activation flag)
- `FARInvaderUpgradeOffer`
- Offer item for a full-blast choice set (`UpgradeTag`, rolled level)
- `FARInvaderFullBlastSessionState`
- Active full-blast offering session (requesting slot, activation tier, 3 offers)
- `FARInvaderLevelOffsetWeight`
- Data-driven weight entry for level-roll offset (`Offset`, `Weight`)

## Blueprint Exposure
- Rows/snapshots are `BlueprintType` and fields are `EditAnywhere, BlueprintReadWrite`.
