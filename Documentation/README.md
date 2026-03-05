# Alien Ramen Docs (UE 5.7)

This site is built with MkDocs Material + Doxygen. Everything under `Documentation/` is rendered as-is; C++ API reference is generated from `Source/` via `Doxyfile`.

## Quick links

- Gameplay Ability System: [GAS overview](README_GAS.md) · [GAS Blueprint attributes](README_GAS_Blueprint_Attributes.md)
- Save system: [Save subsystem](README_SaveSubsystem.md)
- C++ inventory: [Invader runtime/authoring overview](CppOverview/README.md)
- API reference: [Doxygen output](doxygen/index.html)

## Build / preview docs locally

```bash
python3 -m pip install -r requirements-docs.txt
doxygen Doxyfile
mkdocs serve
```

## Debug console commands (Invader)

Use the in-game console (`~`).

??? note "Command list"
    - `ar.invader.start [Seed]`
    - `ar.invader.stop`
    - `ar.invader.dump_state`
    - `ar.invader.force_wave <WaveRowName>`
    - `ar.invader.force_phase <WaveId> <Entering|Active|Berserk|Expired>`
    - `ar.invader.force_threat <Value>`
    - `ar.invader.force_stage <StageRowName>`
    - `ar.invader.choose_stage <left|right>`
    - `ar.invader.force_intro <Seconds|clear>`
    - `ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`

### Expected behavior

1. **`ar.invader.start [Seed]`** — fresh run on authority, resets director state, seeds intro; default seed `1337`.
2. **`ar.invader.stop`** — ends run, halts spawning and phase progression, clears tracking.
3. **`ar.invader.dump_state`** — logs snapshot: flow state, stage, threat, leaks, waves, choices, intro time.
4. **`ar.invader.force_wave <WaveRowName>`** — spawns wave immediately if active + row exists; color-swap repeats respected.
5. **`ar.invader.force_phase <WaveId> <Entering|Active|Berserk|Expired>`** — forces phase for one active wave ID; no-op if missing.
6. **`ar.invader.force_threat <Value>`** — sets threat (>=0) for future weighted selection.
7. **`ar.invader.force_stage <StageRowName>`** — switches stage, enters intro, clears pending choices; does not despawn living enemies.
8. **`ar.invader.choose_stage <left|right>`** — submits choice only during `StageChoice`; then runs transition + intro.
9. **`ar.invader.force_intro <Seconds|clear>`** — overrides intro duration (or clears to normal resolution order: runtime > settings debug > stage row > default).
10. **`ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`** — logs suggested bounds; `apply` writes + saves into director settings; optional PlaneZ/Margin.
