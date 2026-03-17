# Emo (Emotion Runtime)

`Emo` is the overhead emotion runtime used by dialogue/speaker and built-on-top systems.

## Runtime ownership

- Emotion state is server-authoritative.
- Runtime display state is carried by `UEmoComponent`.
- Icon resolve/cache is owned by `UEmoResolverSubsystem`.
- Rendering is owned by `AARHUDBase`.

## Integration rule

Systems should use emotion runtime APIs/overrides and should not directly mutate dialogue-owned emotion persistence state.

## Debugging

- `emo.debug.log` defaults `EmoLog` to `VeryVerbose`.
- `emo.debug.log verbose|log|warning|error|off|reset` supports the same verbosity shorthands as `ar.debug.log`.
- `emo.debug.log` requires an active PIE/Game world context; it will no-op with a warning from editor-only/non-game contexts.
- Native console command `log emolog <level>` still works when you want direct category control.

## Full contract

- [Parley runtime (Dialogue + Speaker)](README_DialogueNPC.md)
