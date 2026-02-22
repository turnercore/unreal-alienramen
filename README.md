# AlienRamen

Developed with Unreal Engine 5.7

## Documentation

- GAS overview and attributes: `README_GAS.md`

## Debug Console Commands

Use Unreal in-game console (`~`) and run:

- `ar.invader.start [Seed]`
- `ar.invader.stop`
- `ar.invader.dump_state`
- `ar.invader.force_wave <WaveRowName>`
- `ar.invader.force_phase <WaveId> <Entering|Active|Berserk|Expired>`
- `ar.invader.force_threat <Value>`
- `ar.invader.force_stage <StageRowName>`
- `ar.invader.choose_stage <left|right>`
- `ar.invader.force_intro <Seconds|clear>`

### Expected Behavior

1. `ar.invader.start [Seed]`

- Starts a fresh invader run on authority/server.
- Resets director runtime state (threat, timers, stage sequence, waves tracked, leaks, one-time usage history).
- Picks initial stage from settings (`InitialStageRow`) or weighted fallback.
- Enters stage intro before normal wave spawning.
- If `Seed` is omitted, defaults to `1337`.

2. `ar.invader.stop`

- Stops the invader run and sets flow state to `Stopped`.
- Stops further director-driven spawning/phase progression.
- Clears director runtime tracking/snapshot state.

3. `ar.invader.dump_state`

- Logs a compact runtime snapshot to `ARLog`.
- Includes flow state, stage, threat, leak counters, active waves, stage choice data, and intro remaining time.

4. `ar.invader.force_wave <WaveRowName>`

- Attempts to spawn that wave definition immediately.
- Works only while run is active and wave row exists.
- Uses repeat color-swap logic if forcing the same wave again and that wave allows swap.

5. `ar.invader.force_phase <WaveId> <Entering|Active|Berserk|Expired>`

- Forces the phase for one active wave instance ID.
- If the wave ID is missing/not active, nothing changes.
- Triggers enemy wave-phase update hooks for spawned enemies in that wave.

6. `ar.invader.force_threat <Value>`

- Sets threat immediately (clamped to `>= 0`).
- Affects future weighted wave selection.

7. `ar.invader.force_stage <StageRowName>`

- Switches current stage definition immediately (if row exists).
- Starts stage intro for that stage (or enters combat immediately if intro resolves to `0`).
- Clears pending/choice stage selection state.
- Does not automatically despawn already alive enemies.

8. `ar.invader.choose_stage <left|right>`

- Submits stage choice when flow state is `StageChoice`.
- Outside `StageChoice`, this is a no-op.
- Starts stage transition delay, then enters chosen stage intro.

9. `ar.invader.force_intro <Seconds|clear>`

- Sets runtime intro override seconds used when resolving stage intro duration.
- If currently in `StageIntro`, updates remaining intro time immediately.
- `clear` removes runtime override and returns intro resolution to normal policy:
  runtime override -> settings debug override -> stage row override -> settings default.
