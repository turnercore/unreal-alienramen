# Alien Ramen Docs (UE 5.7)

This site is built with MkDocs Material + Doxygen. Everything under `Documentation/` is rendered as-is; C++ API reference is generated from `Source/` via `Doxyfile`.

## Quick links

- Gameplay Ability System: [GAS overview](README_GAS.md) · [GAS Blueprint attributes](README_GAS_Blueprint_Attributes.md)
- Save system: [Save subsystem](README_SaveSubsystem.md)
- Dialogue/NPC system: [Dialogue + NPC runtime](README_DialogueNPC.md)
- C++ inventory: [Invader runtime/authoring overview](CppOverview/README.md)
- API reference: [Doxygen HTML](/unreal-alienramen/doxygen/index.html)

## Build / preview docs locally

```bash
source .venv-docs/bin/activate
mkdocs serve
```
(This runs Doxygen via the MkDocs plugin and serves on 127.0.0.1:8000 by default.)

## Debug console commands (Invader)

Use the in-game console (`~`).

??? note "Command list"
    - `ar.invader.start [Seed]`
    - `ar.invader.stop`
    - `ar.invader.dump_state`
    - `ar.invader.force_wave <WaveRowName>`
    - `ar.invader.force_phase <WaveId> <Active|Berserk>`
    - `ar.invader.force_threat <Value>`
    - `ar.invader.force_stage <StageRowName>`
    - `ar.invader.choose_stage <left|right>`
    - `ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`

### Expected behavior (authoritative server)

1. **`ar.invader.start [Seed]`** — starts a fresh run if none active; default seed `1337`.
2. **`ar.invader.stop`** — stops the run, cleans managed enemies, resets runtime counters/snapshots.
3. **`ar.invader.dump_state`** — logs a compact snapshot to `ARLog`.
4. **`ar.invader.force_wave <WaveRowName>`** — spawns wave immediately (requires active run + valid row).
5. **`ar.invader.force_phase <WaveId> <Active|Berserk>`** — forces phase on an active wave instance; invalid phases rejected.
6. **`ar.invader.force_threat <Value>`** — sets threat (>=0) affecting future weighted selection.
7. **`ar.invader.force_stage <StageRowName>`** — switches stage immediately if row exists; does not despawn current enemies.
8. **`ar.invader.choose_stage <left|right>`** — submits choice only during `StageChoice`; otherwise no-op.
9. **`ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`** — logs suggested bounds; `apply` writes+saves into director settings; optional PlaneZ and XY margin.
