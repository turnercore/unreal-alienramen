# Invader Data Types (`ARInvaderTypes.h`)
Path: `Source/AlienRamen/Public/ARInvaderTypes.h`

## Core Enums
- `EAREnemyColor`: Red, White, Blue
- `EARWavePhase`: Active, Berserk
- `EARInvaderFlowState`: Combat, AwaitStageClear, StageChoice, Transition, Stopped
- `EARSpawnEdge`: Top, Left, Right

## Authoring Rows
- `FARWaveEnemySpawnDef`
- Enemy class, color, edge, delay, authored offset, per-spawn effects
- `FARWaveDefRow`
- `bEnabled`
- Threat/player/selection fields
- Formation lock + flip flags
- `WaveDuration`
- Berserk profile
- Spawn array, wave tags, wave-level enemy effects
- `FARStageDefRow`
- `bEnabled`
- Selection/duration/threat/health multipliers
- Required/blocked/banned tags
- Stage-level enemy effects
- Reward descriptor

## Runtime Snapshot Types
- `FARWaveInstanceState`
- live wave info (id, row, phase, counts, stage, timestamps, color-swap)
- `FARInvaderRuntimeSnapshot`
- flow state, threat, run/stage timing
- stage choice + reward fields
- leak values and soft caps
- active wave array

## Blueprint Exposure
- Rows/snapshots are `BlueprintType` and fields are `EditAnywhere, BlueprintReadWrite`.

