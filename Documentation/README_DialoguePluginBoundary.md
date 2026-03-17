# Dialogue Plugin Ownership Boundary

This page defines what is owned by the Parley dialogue plugin boundary (`Plugins/Parley`) versus systems that build on top.

## Owned by Dialogue Plugin

- Dialogue runtime system (`UParleyDialogueSubsystem` and dialogue session execution).
- Speaker definitions and speaker-facing authored data.
- Dialogue editor tooling for speakers and conversations.
- Conversations and line execution model.
- Speaker dialogue component/runtime (`UParleySpeakerComponent`, `UParleySpeakerSubsystem`).
- Dialogue-facing faction/relationship state used by dialogue mutations and gating.
- Speaker relationship progression surfaces used by dialogue flow.
- Dialogue graph `Signal` node contract and `OnDialogueSignalFired` broadcast surface.
- Dialogue audio resolution contract (`NativeAudio` vs `AudioSignals`) and `OnDialogueAudioRequested` payload emission.

## Not Owned by Dialogue Plugin (Built On Top)

- Faction voting/election orchestration and travel finalization flow.
- Ordering/customer-serving gameplay loops.
  - Current built-on-top runtime surfaces: `UARCustomerComponent`, shop stations/bowls/meat storage.
- Emotion display/resolution runtime (`UEmoComponent`, `UEmoResolverSubsystem`, `UEmoSettings`, HUD rendering).
- FMOD event playback and cue-tag-to-FMOD mapping tables.

## Integration Rule

- Built-on-top systems may read and write dialogue-owned state only through stable runtime APIs/contracts; they should not become ownership authorities for dialogue, speaker, conversation, or relationship data.
- Shop/customer-serving systems should use `UParleyDialogueSubsystem::ApplyRamenServeOutcome(...)` for relationship + emotion output instead of mutating save/emotion state directly.
- Systems reacting to dialogue `Signal` nodes should bind `OnDialogueSignalFired` in game layer and keep gameplay effects out of dialogue graph execution code.
- Systems reacting to dialogue audio should consume controller-local requests in game layer; Parley should not depend on FMOD modules.

## Debugging

- `parley.debug.log` defaults `ParleyLog` to `VeryVerbose`.
- `parley.debug.log verbose|log|warning|error|off|reset` supports the same verbosity shorthands as `ar.debug.log`.
- `parley.debug.log` requires an active PIE/Game world context; it will no-op with a warning from editor-only/non-game contexts.
- Native console command `log parleylog <level>` still works for direct category control.
