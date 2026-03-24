# Emo (Emotion Runtime)

`Emo` is the overhead emotion runtime used by dialogue/speaker and other built-on-top systems.

## Runtime ownership

- Emotion state is server-authoritative.
- Runtime display registrations are carried by `UEmoComponent`.
- Icon resolve/cache is owned by `UEmoResolverSubsystem`.
- Active component projection is cached through a world-scoped registry instead of a global object scan.
- Rendering is owned by `AEmoHUDBase`.

## Editor preview

- `UEmoComponent::PreviewEmotionTag` is an authoring-only preview tag used by the component details panel and editor viewport.
- The editor preview billboard is editor-only and should be replaced in place when the preview tag changes; it must not accumulate duplicate billboard components on the owning actor.

## Integration rule

- Systems should write generic emotion registrations and should not hard-code project-specific viewer identity rules into the plugin.

## Tag and plugin contract

- Emo is Parley-agnostic at runtime. It resolves generic gameplay-tag-targeted registrations and does not interpret conversation, slot, or character ownership on its own.
- Emo provisions resolver/preview tags only: `Parley.Emotion`, `Parley.Emotion.Busy`, `Parley.Emotion.WantsToTalk`, and `Parley.Emotion.Preview`.
- Resolver cache invalidation compares the resolved emotion DataTable path so TagKey route changes cannot leave a stale icon map behind.

## HUD contract

- `MinimumIconScreenSizePixels` is a render-size clamp, not a cull threshold.
- `AEmoHUDBase` owns the local viewer context through `ViewedEmotionTags`.
- Game code is responsible for pushing the currently viewed gameplay tags into the HUD whenever local pawn or character context changes.
- `UEmoComponent` resolves registrations by:
  - highest priority first
  - targeted registrations before global registrations on equal priority
  - latest write when same-priority targeted registrations still tie
- Same-priority targeted conflicts log a warning and still resolve deterministically.
- `OnEmotionDisplayChanged` remains the empty-viewer/global signal; tag-scoped-only changes still raise `OnEmotionDisplayStateChanged`.

## Debugging

- `emo.debug.log` defaults `EmoLog` to `VeryVerbose`.
- `emo.debug.log verbose|log|warning|error|off|reset` supports the same verbosity shorthands as `ar.debug.log`.
- `emo.debug.log` requires an active PIE/Game world context; it will no-op with a warning from editor-only/non-game contexts.
- Native console command `log emolog <level>` still works when you want direct category control.

## Full contract

- [Parley runtime (Dialogue + Speaker)](README_DialogueNPC.md)
