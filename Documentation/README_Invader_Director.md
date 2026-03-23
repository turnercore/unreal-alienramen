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
- Invader loss evaluation is signal-driven, not tick-polled:
  - the director refreshes loss state when tracked-player topology changes or when a tracked character runtime reports health/max-health/downed/dead changes
  - the director should evaluate canonical Invader character runtimes, not whichever runtime a player currently has selected on their `AARPlayerStateBase`
  - character switching must not by itself create a transient loss because control handoff does not change the underlying character runtimes being tracked
- `AARInvaderGameMode` is the canonical runtime hook for post-run orchestration:
  - Binds to `OnRunEnded` on authority.
  - Notifies each owning `AARInvaderPlayerController` through `ClientHandleInvaderRunEnded(Reason)` so local end-sequence UI/animation can run per player.
  - Exposes BP event `OnInvaderRunEnded(Reason)` for mode-level sequencing.
  - Exposes `FinalizeInvaderRunAndTravel(...)` so BP/C++ can decide exactly when to leave Invader.
- Auto travel is optional and configurable in `AARInvaderGameMode` defaults:
  - `bAutoTravelAfterRunEnd` (off by default)
  - `AutoTravelAfterRunEndDelaySeconds`
  - `DefaultPostRunTravelURL` (defaults to `/Game/Maps/Lvl_Scrapyard`)
