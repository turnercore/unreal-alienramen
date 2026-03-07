# Dialogue + NPC Runtime (`UARDialogueSubsystem`, `UARNPCSubsystem`)

## Overview

Alien Ramen dialogue/NPC runtime is server-authoritative and save-backed:

- `UARDialogueSubsystem` owns runtime dialogue sessions, node progression, choices, and eavesdrop policy.
- `UARNPCSubsystem` owns persistent NPC relationship/want state and talkable-state refresh.
- `AARNPCCharacterBase` is the world NPC entrypoint with replicated `bIsTalkable` and server interaction handoff.

## Runtime Entry Points

- Player/UI entrypoints are routed through `AARPlayerController` RPC wrappers:
  - `RequestStartDialogue`
  - `RequestAdvanceDialogue`
  - `RequestSubmitDialogueChoice`
  - `RequestSetDialogueEavesdrop`
- World interaction entrypoint is `AARNPCCharacterBase::InteractByController(...)` (authority-only).
- Core subsystem calls:
  - `TryStartDialogueWithNpc(...)`
  - `AdvanceDialogue(...)`
  - `SubmitDialogueChoice(...)`
  - `SetShopEavesdropTarget(...)`

## Mode Behavior

- `Mode.Invader`, `Mode.Scrapyard`: one shared dialogue session for the match.
- `Mode.Shop`: per-player dialogue sessions.
- Shop supports eavesdrop mirroring (`RequestSetDialogueEavesdrop(...)`).
- World pause on dialogue is controlled by `UARDialogueSettings::PauseOnDialogueModeTags` (default only `Mode.Invader`).
- Shared mode policy: only one shared session can be active at a time.
- Shop policy: each owner slot can have at most one active per-player session.

## Unlock + Row Selection Policy

- Dialogue rows are discovered through `UContentLookupSubsystem` from root `Dialogue.Node`.
- Candidate rows are filtered by exact `NpcTag`, then ordered by priority/tag sort.
- Row unlock conditions are evaluated server-side against:
  - Save progression tags (`RequiredProgressionTags`, `BlockedProgressionTags`)
  - GameState unlock tags (`RequiredUnlockTags`, `BlockedUnlockTags`)
  - NPC relationship state (`MinLoveRating`, `bRequiresWantSatisfied`)
  - Seen-history repeat policy (`bAllowRepeatAfterSeen`)

## Choice + Seen + Progression Policy

- Node participation mode: `InitiatorOnly` or `GroupChoice`.
- Canonical branch outcome is persisted once globally per node.
- Group-choice conflict tie-break is initiator-wins.
- Important node hook: `bForceEavesdropForImportantDecision` forces partner viewing in Shop and blocks choice submit until all slotted players are viewing.
- Seen history is per-player, but only active speaker gets seen credit.
- Progression grants:
  - `GrantProgressionTagsOnEnter` are applied when entering a row.
  - `GrantProgressionTags` from the resolved choice are applied on choice commit.
- Canonical choice writes and seen-history writes mark save dirty (`UARSaveSubsystem::MarkSaveDirty()`).

## NPC Relationship + Talkable State

- `UARNPCSubsystem::SubmitNpcRamenDelivery(...)` accepts only exact want-tag matches.
- On accepted delivery:
  - `LoveRating` increases by at least `1` (or NPC row override)
  - `bCurrentWantSatisfied` is set
  - Save is marked dirty
- NPC talkable state is derived from dialogue unlock availability (`HasUnlockedDialogueForNpcForAnyPlayer`) and cached in `UARNPCSubsystem`.
- `AARNPCCharacterBase` listens for talkable cache changes and replicates `bIsTalkable` to clients.

## Persistence

Save schema `v6` includes:

- `NpcRelationshipStates`
- `DialogueCanonicalChoiceStates`
- `PlayerDialogueHistoryStates`

`UARSaveSubsystem::GatherRuntimeData(...)` carries these arrays forward from `CurrentSaveGame` so runtime dialogue/NPC mutations persist across normal save cycles.

Related saved-tag contracts:
- [Progression + Unlocks](README_ProgressionUnlocks.md)
- [Save subsystem](README_SaveSubsystem.md)

## Content Authoring

Dialogue and NPC definitions are DataTable rows resolved via `UContentLookupSubsystem`.

- Dialogue root tag: `Dialogue.Node`
- NPC root tag: `NPC.Identity`

Dialogue row type: `FARDialogueNodeRow`
NPC row type: `FARNpcDefinitionRow`
