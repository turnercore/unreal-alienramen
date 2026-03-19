# Alien Ramen Docs (UE 5.7)

This site is built with MkDocs Material + Doxygen. Everything under `Documentation/` is rendered as-is; C++ API reference is generated from `Source/` via `Doxyfile`.

## Start Here

- For mode flow and gameplay ownership across the main loop: [Game Modes Guide](README_GameModes.md)
- For networking, persistence, plugins, GAS, dialogue, and other shared runtime surfaces: [Shared Systems Guide](README_SharedSystems.md)
- For class-level Invader runtime references: [Invader C++ Overview](CppOverview/README.md)
- For generated API docs: [Doxygen HTML](/unreal-alienramen/doxygen/index.html)

## Core Loop Map

1. [Shop Mode](README_Shop.md)
2. [Invader Mode](README_Invader.md)
3. [Scrapyard Mode](README_Scrapyard.md)

Travel between those modes is coordinated through [Transition Flow](README_TransitionMode.md).

## Shared Runtime Map

- Networking and sessions: [Networking Overview](README_Networking.md)
- Save, travel, and hydration: [Persistence Overview](README_Persistence.md)
- Plugins and shared runtime foundations: [Plugins Overview](README_Plugins.md)
- Gameplay Ability System: [GAS Overview](README_GAS.md)
- Dialogue, speakers, emotion, and faction surfaces: [Parley Runtime](README_DialogueNPC.md) and [Faction Subsystem](README_FactionSubsystem.md)

## Build / preview docs locally

```powershell
py -m venv .venv-docs
.\.venv-docs\Scripts\Activate.ps1
pip install -r requirements-docs.txt
python -m mkdocs serve
```
(This runs Doxygen via the MkDocs plugin and serves on 127.0.0.1:8000 by default. `doxygen` must be available on `PATH`.)

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
    - `ar.invader.debug.set_spice [p1|p2] <value>`
    - `ar.invader.debug.add_spice [p1|p2] <delta>`
    - `ar.invader.debug.add_scrap <delta>`
    - `ar.invader.debug.add_money <delta>`
    - `ar.debug.add_meat [delta] [Item.Meat.*]`
    - `ar.debug.log <veryverbose|verbose|log|warning|error|off|reset>`

### Expected behavior (authoritative server)

1. **`ar.invader.start [Seed]`** - starts a fresh run if none active; default seed `1337`.
2. **`ar.invader.stop`** - stops the run, cleans managed enemies, resets runtime counters/snapshots.
3. **`ar.invader.dump_state`** - logs a compact snapshot to `ARLog`.
4. **`ar.invader.force_wave <WaveRowName>`** - spawns wave immediately (requires active run + valid row).
5. **`ar.invader.force_phase <WaveId> <Active|Berserk>`** - forces phase on an active wave instance; invalid phases rejected.
6. **`ar.invader.force_threat <Value>`** - sets threat (>=0) affecting future weighted selection.
7. **`ar.invader.force_stage <StageRowName>`** - switches stage immediately if row exists; does not despawn current enemies.
8. **`ar.invader.choose_stage <left|right>`** - submits choice only during `StageChoice`; otherwise no-op.
9. **`ar.invader.capture_bounds [apply] [PlaneZ] [Margin]`** - logs suggested bounds; `apply` writes+saves into director settings; optional PlaneZ and XY margin.
10. **`ar.invader.debug.set_spice [p1|p2] <value>`** - sets a player spice meter directly (`p1` default).
11. **`ar.invader.debug.add_spice [p1|p2] <delta>`** - adds/subtracts player spice (`p1` default).
12. **`ar.invader.debug.add_scrap <delta>`** - adds/subtracts replicated scrap on GameState.
13. **`ar.invader.debug.add_money <delta>`** - adds/subtracts replicated money on GameState.
14. **`ar.debug.add_meat [delta] [Item.Meat.*]`** - adds/subtracts replicated typed meat using an explicit meat definition tag (`delta` defaults to `1`; if tag omitted, first deterministic meat row is used).
15. **`ar.debug.log <veryverbose|verbose|log|warning|error|off|reset>`** - sets runtime `ARLog` verbosity via a lowercase shortcut.
