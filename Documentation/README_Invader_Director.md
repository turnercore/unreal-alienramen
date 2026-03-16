# Invader Director

Invader run orchestration is owned by `UARInvaderDirectorSubsystem` and related settings/state contracts.

## What to read

- [Director subsystem](CppOverview/UARInvaderDirectorSubsystem.md)
- [Director settings](CppOverview/UARInvaderDirectorSettings.md)
- [Runtime state component](CppOverview/UARInvaderRuntimeStateComponent.md)
- [Invader data types](CppOverview/InvaderDataTypes.md)

These pages capture wave/stage orchestration, runtime state flow, and tunable settings authority.

## Run-End Integration Contract

- `UARInvaderDirectorSubsystem` remains the authority for deciding when a run ends (for example `ManualStop` via bail vote or loss conditions) and emits `OnRunEnded`.
- `AARInvaderGameMode` is the canonical runtime hook for post-run orchestration:
  - Binds to `OnRunEnded` on authority.
  - Notifies each owning `AARInvaderPlayerController` through `ClientHandleInvaderRunEnded(Reason)` so local end-sequence UI/animation can run per player.
  - Exposes BP event `OnInvaderRunEnded(Reason)` for mode-level sequencing.
  - Exposes `FinalizeInvaderRunAndTravel(...)` so BP/C++ can decide exactly when to leave Invader.
- Auto travel is optional and configurable in `AARInvaderGameMode` defaults:
  - `bAutoTravelAfterRunEnd` (off by default)
  - `AutoTravelAfterRunEndDelaySeconds`
  - `DefaultPostRunTravelURL` (defaults to `/Game/Maps/Lvl_Scrapyard`)
