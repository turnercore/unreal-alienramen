# AlienRamen

Developed with Unreal Engine 5.7

## Documentation

- GAS overview and attributes: `README_GAS.md`
- Save system/runtime flow: `README_SaveSubsystem.md`

## Debug Console Commands

Use Unreal in-game console (`~`) and run:

- `ar.invader.start [Seed]`
- `ar.invader.stop`
- `ar.invader.dump_state`
- `ar.invader.force_wave <WaveRowName>`
- `ar.invader.force_phase <WaveId> <Active|Berserk>`
- `ar.invader.force_threat <Value>`
- `ar.invader.force_stage <StageRowName>`
- `ar.invader.choose_stage <left|right>`
- `ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`

### Expected Behavior

1. `ar.invader.start [Seed]`

- Starts an invader run on authority/server.
- Rejected if a run is already active.
- If `Seed` is omitted, defaults to `1337`.

2. `ar.invader.stop`

- Stops the run and performs full director cleanup.
- Managed enemies are cleaned up and runtime counters/snapshots reset.

3. `ar.invader.dump_state`

- Logs a compact runtime snapshot to `ARLog`.

4. `ar.invader.force_wave <WaveRowName>`

- Attempts to spawn that wave immediately.
- Requires active run and valid wave row.

5. `ar.invader.force_phase <WaveId> <Active|Berserk>`

- Forces phase on an active wave instance.
- Invalid phase values are rejected.

6. `ar.invader.force_threat <Value>`

- Sets threat immediately (clamped to `>= 0`).
- Affects future weighted wave selection.

7. `ar.invader.force_stage <StageRowName>`

- Switches current stage definition immediately (if valid row exists).
- Does not automatically despawn currently alive enemies.

8. `ar.invader.choose_stage <left|right>`

- Submits stage choice when flow state is `StageChoice`.
- Outside `StageChoice`, this is a no-op.

9. `ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`

- Deprojects viewport corners onto a horizontal plane (default `SpawnOrigin.Z`).
- Logs suggested `GameplayBoundsMin/Max`.
- Pass `apply` to write+save into director settings.
- Optional numeric args: custom `PlaneZ` and XY margin.
