# Dialogue Plugin Ownership Boundary

This page defines what is owned by the Dialogue plugin boundary (even if code currently lives in `Source/AlienRamen`) versus systems that build on top.

## Owned by Dialogue Plugin

- Dialogue runtime system (`UARDialogueSubsystem` and dialogue session execution).
- Speaker definitions and speaker-facing authored data.
- Dialogue editor tooling for speakers and conversations.
- Conversations and line execution model.
- Emotion runtime and content resolution (`UAREmotionComponent`, emotion tags/icons).
- Speaker dialogue/emotion components (`UARSpeakerComponent`, `UAREmotionComponent` integration).
- Dialogue-facing faction/relationship state used by dialogue mutations and gating.
- Speaker relationship progression surfaces used by dialogue flow.

## Not Owned by Dialogue Plugin (Built On Top)

- Faction voting/election orchestration and travel finalization flow.
- Ordering/customer-serving gameplay loops.
  - Current built-on-top runtime surfaces: `UARCustomerComponent`, shop stations/bowls/meat storage.

## Integration Rule

- Built-on-top systems may read and write Dialogue-owned state only through stable runtime APIs/contracts; they should not become ownership authorities for dialogue, speaker, conversation, emotion, or relationship data.
- Shop/customer-serving systems should use `UARDialogueSubsystem::ApplyRamenServeOutcome(...)` for relationship + emotion output instead of mutating save/emotion state directly.
