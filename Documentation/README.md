# Alien Ramen Docs (UE 5.7)

This site is built with MkDocs Material + Doxygen. Everything under `Documentation/` is rendered as-is; C++ API reference is generated from `Source/` via `Doxyfile`.

## Quick links

- Gameplay Ability System: [GAS overview](README_GAS.md) - [GAS Blueprint attributes](README_GAS_Blueprint_Attributes.md)
- Networking/session system: [Session subsystem](README_SessionSubsystem.md)
- Save system: [Save subsystem](README_SaveSubsystem.md)
- Invader drops: [Invader drops runtime](README_InvaderDrops.md)
- Progression + unlocks: [Progression + Unlocks](README_ProgressionUnlocks.md)
- Dialogue plugin boundary: [Dialogue plugin ownership](README_DialoguePluginBoundary.md)
- Dialogue/NPC system (includes Signal node usage): [Dialogue + NPC runtime](README_DialogueNPC.md)
- Shop ramen ordering/serving (built on top): [Shop ramen system](README_ShopRamenSystem.md)
- Scrapyard extraction + temp buffs: [Scrapyard mode + temp buffs](README_ScrapyardMode.md)
- Faction election system (built on top): [Faction subsystem](README_FactionSubsystem.md)
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
    - `ar.invader.debug.set_spice [p1|p2] <value>`
    - `ar.invader.debug.add_spice [p1|p2] <delta>`
    - `ar.invader.debug.add_scrap <delta>`
    - `ar.invader.debug.add_money <delta>`
    - `ar.debug.add_meat [delta] [red|blue|white|none]`
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
14. **`ar.debug.add_meat [delta] [red|blue|white|none]`** - adds/subtracts replicated meat in the selected bucket (`none` default; can also pass color first without delta).
15. **`ar.debug.log <veryverbose|verbose|log|warning|error|off|reset>`** - sets runtime `ARLog` verbosity via a lowercase shortcut.
