# Emo (Emotion Runtime)

`Emo` is the overhead emotion runtime used by dialogue/speaker and built-on-top systems.

## Runtime ownership

- Emotion state is server-authoritative.
- Runtime display state is carried by `UEmoComponent`.
- Icon resolve/cache is owned by `UEmoResolverSubsystem`.
- Active component projection is cached through a world-scoped registry instead of a global object scan.
- Rendering is owned by `AEmoHUDBase`.

## Integration rule

Systems should use emotion runtime APIs/overrides and should not directly mutate dialogue-owned emotion persistence state.

## Tag and plugin contract

- Emo depends on the `Parley` plugin at runtime.
- Emo provisions its own required tags for preview and player-slot emotion handling, including `Parley.Emotion.Preview`, `Player.Slot.P1`, and `Player.Slot.P2`.
- Resolver cache invalidation compares the resolved emotion DataTable path so TagKey route changes cannot leave a stale icon map behind.

## HUD contract

- `MinimumIconScreenSizePixels` is a render-size clamp, not a cull threshold.
- `OnEmotionDisplayChanged` is shared-display only; slot-specific changes still raise `OnEmotionDisplayStateChanged`.

## Full contract

- [Parley runtime (Dialogue + Speaker)](README_DialogueNPC.md)
