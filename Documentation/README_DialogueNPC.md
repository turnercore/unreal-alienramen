# Dialogue + NPC Runtime (`UARDialogueSubsystem`, `UARNPCSubsystem`)

## Overview

Alien Ramen now uses a conversation-asset, compiled-graph dialogue runtime:

- `UARDialogueSubsystem` is server-authoritative for offer selection, session execution, branching, eavesdrop, completion persistence, and choice-memory persistence.
- `UARNPCSubsystem` is now talkable-cache-focused and derives NPC talkable state from dialogue unlock availability.
- `AARNPCCharacterBase` remains the world interaction entrypoint and replicates `bIsTalkable`.

## Runtime Entry Points

Player/UI entrypoints route through `AARPlayerController` RPC wrappers:

- `RequestStartDialogue(FGameplayTag NpcTag)`
- `RequestInteractWithNpc(AARNPCCharacterBase* NpcActor)`
- `RequestAdvanceDialogue()`
- `RequestSubmitDialogueChoice(FGuid ChoiceBranchId)`
- `RequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot)`
- `RequestSetDialogueEavesdropOtherPlayer(bool bEnable)` (slot convenience wrapper)

Server runtime now pushes authoritative view snapshots back to client controllers via:

- `ClientDialogueSessionUpdated(const FDialogueClientView& View)`
- `ClientDialogueSessionEnded(const FString& SessionId)`

Core subsystem API:

- `GetAvailableConversationForNPC(...)`
- `StartConversation(...)`
- `AdvanceConversation(...)`
- `SubmitChoice(...)`
- `ForceEavesdrop(...)`
- `ValidateConversation(...)`
- `ValidateSpeaker(...)`
- `PreviewConversation(...)`
- `PreviewConversationTrace(...)` (tooling-oriented multi-step trace simulation)

Compatibility wrappers still exist for gameplay BPs:

- `TryStartDialogueWithNpc(...)`
- `SubmitDialogueChoice(...)` (routes to `SubmitChoice`)
- `SetShopEavesdropTarget(...)` (routes to `ForceEavesdrop`)

## Runtime UI Bridge

Runtime UI is intentionally separate from editor preview tooling.

- `UARDialogueWidgetBase` (`Source/AlienRamen/Public/ARDialogueWidgetBase.h`) is the shared Blueprint-facing widget base for dialogue presentation and input forwarding.
- `AARPlayerController` now exposes a local runtime UI bridge:
  - dialogue delegates: `OnDialogueViewUpdated`, `OnDialogueSessionEndedSignal`
  - cached view helpers: `GetCachedDialogueView(...)`, `QueryLocalDialogueView(...)`
  - widget lifecycle: `EnsureDialogueWidget()`, `RemoveDialogueWidget()`, `GetDialogueWidget()`
  - auto-widget config: `bAutoCreateDialogueWidget`, `DialogueWidgetClass`, `DialogueWidgetZOrder`
- `UARDialogueWidgetBase` forwards core interaction calls (`AdvanceDialogue`, `SubmitChoice`, `SetEavesdrop`, `SetEavesdropOtherPlayer`, `StartDialogueWithNpcTag`, `InteractWithNpc`) back to the bound controller.
- Default widget behavior can auto-toggle visibility from dialogue state (visible when view updates arrive, collapsed on session end/deinit).

## Content Model

Shared dialogue types live in [`Source/AlienRamen/Public/ARDialogueTypes.h`](/c:/Projects/Unreal/AlienRamen/Source/AlienRamen/Public/ARDialogueTypes.h).

- Speakers: `FARDialogueSpeakerRow` rows (content lookup compatible).
- Conversations: `UARDialogueConversationAsset` with:
  - `Header` (`FDialogueConversationHeader`)
  - `CompiledData` (`FDialogueCompiledConversationData`)
- Lines are embedded per conversation (`FDialogueConversationLine`), not global rows.
- Conversation registry source:
  - TagContentResolver DataTable rows (`FARDialogueConversationAssetRow`) routed by `ConversationDefinitionRootTag` (row tag or built tag from root+row name).

Settings live in [`Source/AlienRamen/Public/ARDialogueSettings.h`](/c:/Projects/Unreal/AlienRamen/Source/AlienRamen/Public/ARDialogueSettings.h):

- `SpeakerDefinitionRootTag` (speaker row lookup root)
- `ConversationDefinitionRootTag` (conversation lookup row root)
- shared/per-player mode tag containers
- execution guard `MaxExecutionStepsPerAdvance`

Default config now uses `SpeakerDefinitionRootTag=Dialogue.Speaker` and `ConversationDefinitionRootTag=Dialogue.Conversation`.

## Offer + Execution Rules (Current Runtime)

- Offer selection is server-side and bucketed:
  - unseen
  - game-seen/player-unseen catch-up
  - repeatable/other
- Highest numeric priority wins inside the first non-empty bucket; equal priorities randomize.
- Offer checks include:
  - mode enabled by `UARDialogueSettings` shared/per-player mode tags
  - primary speaker exact match
  - relationship minimum
  - locked/blocked condition groups
  - seen/completed/repeatability suppression flags
- Runtime executes compiled nodes server-side with a step cap from settings.
- Implemented node execution: enter/completed/line/choice/bool/switch/tag-mutation/relationship-mutation/faction-mutation/random.
- Line-node auto-advance is now a per-player runtime preference on `AARPlayerStateBase` (`SetDialogueAutoAdvanceEnabled`), not authored per line node.
- Important conversation and important choice flow force passive players into participants/eavesdrop set before interaction.
- Blocked-condition defaults now align to spec intent (`Any` by default on blocked groups); locked groups remain `All` by default.
- Logging: normal gating/selection outcomes are logged at `Verbose` level in `ARLog`; invalid graph/runtime corruption is logged as `Warning`/`Error` with conversation tag/session context for debugging.

## Seen vs Completed

- Seen state is transient only (game + per-slot runtime containers in `UARDialogueSubsystem`).
- Completed state is persistent and save-backed.
- Completion is written only when a `Completed` node executes.
- Save system enforces no mid-conversation saves by checking active dialogue sessions before `SaveCurrentGame`.

## Choice Memory

- Runtime choice picks are tracked per session (`ChoiceNodeId -> BranchId`).
- On completed conversation:
  - game completion tag is persisted
  - initiating player completion tag is persisted
  - per-choice memory records are persisted (`FDialogueChoiceMemoryRecord`)
- Choice nodes in `LockedToRecordedChoice` mode auto-route to persisted branch when encountered after completion.
- If a completed conversation is replayed and a locked choice record is missing/unroutable, runtime now fails closed (ends non-completed) instead of reopening free choice.

## Persistence

Save schema is now `v7`:

- `DialogueRelationshipStates`
- `DialogueCompletedConversationTagsByGame`
- `DialoguePlayerPersistentStates`

Removed legacy dialogue save fields:

- `NpcRelationshipStates`
- `DialogueCanonicalChoiceStates`
- `PlayerDialogueHistoryStates`

## NPC Talkable Runtime

`UARNPCSubsystem` no longer owns ramen/want mutation logic. It now:

- caches `NpcTag -> bTalkable`
- refreshes from `UARDialogueSubsystem::HasUnlockedDialogueForNpcForAnyPlayer(...)`
- broadcasts `OnNpcTalkableChanged`
- combines subsystem talkable state with `AARNPCCharacterBase::bNpcLocalStateAllowsDialogue` (local NPC runtime gate, for example ordering-mode lockouts)

## Editor Tooling (Current)

Registered editor tabs:

- `Dialogue Speaker Editor` (`SDialogueSpeakerEditorPanel`)
- `Dialogue Conversation Graph Editor` (`SDialogueConversationGraphEditorPanel`)

Conversation graph tooling now provides:

- blueprint-style `SGraphEditor` canvas with right-click node creation
- right-click node creation actions are flat/top-level (no nested "Dialogue Nodes" submenu)
- graph node classes/schema (`UARDialogueEdGraph`, `UARDialogueEdGraphNode`, `UARDialogueEdGraphSchema`)
- line nodes now render with inline authoring UI: speaker portrait button (click to cycle speakers) + wrapped inline line-text edit
- drag-link execution wiring with:
  - one outgoing link per output pin
  - multiple incoming links allowed per node input
- full toolbar flow: Save / Validate / Compile Runtime Graph / Focus Enter / Auto Layout / Preview
- details-panel editing for selected node or conversation root
- dynamic branch-pin behavior for choice/switch/random nodes driven by stable branch GUIDs
- compile-from-editor-graph into `CompiledData` with node-level validation markers
- validation + preview execution through runtime dialogue subsystem even when PIE is not running
- no standalone in-tab global conversation list; graph tab edits a targeted conversation (speaker-hub handoff or explicit asset picker selection)
- preview trace output supports multi-step execution (line waits + auto-choice routing), plus preview-seen flags and typed injected variables
- speaker-tag editor fields are gameplay-tag-filtered to `Dialogue.Speaker.*` (header primary/participants, line speaker, relationship target, portrait-tag metadata surfaces)

Speaker hub currently provides:

- content-lookup-backed speaker table loading from `SpeakerDefinitionRootTag`
- searchable/sortable speaker list with columns (display name/tag/thresholds/conversation count)
- speaker CRUD (`New`, `Duplicate`, `Delete`) + `Save Speaker` + `Validate Speaker`
- reorderable threshold editing/reset (`50,150,300,500` defaults)
- inline portrait list with add/update/remove operations
- relationship-band grouped conversation map for selected primary speaker with structured gate/mutation summaries and unlock-chain hints
- conversation create/open actions and broken-conversation scan using runtime validator
