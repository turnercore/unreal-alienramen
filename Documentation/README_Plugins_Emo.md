# Emo (Emotion Runtime)

`Emo` is the overhead emotion runtime used by dialogue/speaker and built-on-top systems.

## Runtime ownership

- Emotion state is server-authoritative.
- Runtime display state is carried by `UEmoComponent`.
- Icon resolve/cache is owned by `UEmoResolverSubsystem`.
- Rendering is owned by `AARHUDBase`.

## Integration rule

Systems should use emotion runtime APIs/overrides and should not directly mutate dialogue-owned emotion persistence state.

## Full contract

- [Parley runtime (Dialogue + Speaker)](README_DialogueNPC.md)
