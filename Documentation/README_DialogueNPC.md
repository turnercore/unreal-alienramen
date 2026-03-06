# Dialogue + NPC Runtime (`UARDialogueSubsystem`, `UARNPCSubsystem`)

## Overview

Alien Ramen dialogue/NPC runtime is server-authoritative and save-backed:

- `UARDialogueSubsystem` owns runtime dialogue sessions, node progression, choices, and eavesdrop policy.
- `UARNPCSubsystem` owns persistent NPC relationship/want state and talkable-state refresh.
- `AARNPCCharacterBase` is the world NPC entrypoint with replicated `bIsTalkable` and server interaction handoff.

## Mode Behavior

- `Mode.Invader`, `Mode.Scrapyard`: one shared dialogue session for the match.
- `Mode.Shop`: per-player dialogue sessions.
- Shop supports eavesdrop mirroring (`RequestSetDialogueEavesdrop(...)`).
- World pause on dialogue is controlled by `UARDialogueSettings::PauseOnDialogueModeTags` (default only `Mode.Invader`).

## Choice + Seen Policy

- Node participation mode: `InitiatorOnly` or `GroupChoice`.
- Canonical branch outcome is persisted once globally per node.
- Group-choice conflict tie-break is initiator-wins.
- Important node hook: `bForceEavesdropForImportantDecision` forces partner viewing in Shop and blocks choice submit until all slotted players are viewing.
- Seen history is per-player, but only active speaker gets seen credit.

## Persistence

Save schema `v5` adds:

- `NpcRelationshipStates`
- `DialogueCanonicalChoiceStates`
- `PlayerDialogueHistoryStates`

`UARSaveSubsystem::GatherRuntimeData(...)` carries these arrays forward from `CurrentSaveGame` so runtime dialogue/NPC mutations persist across normal save cycles.

## Content Authoring

Dialogue and NPC definitions are DataTable rows resolved via `UContentLookupSubsystem`.

- Dialogue root tag: `Dialogue.Node`
- NPC root tag: `Npc.Definition`

Dialogue row type: `FARDialogueNodeRow`
NPC row type: `FARNpcDefinitionRow`
