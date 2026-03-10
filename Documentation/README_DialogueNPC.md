# Dialogue + NPC Runtime (`UARDialogueSubsystem`, `UARNPCSubsystem`)

Plugin ownership boundary reference: [Dialogue Plugin Ownership Boundary](README_DialoguePluginBoundary.md)

## Overview

Alien Ramen now uses a conversation-asset, compiled-graph dialogue runtime:

- `UARDialogueSubsystem` is server-authoritative for offer selection, session execution, branching, eavesdrop, completion persistence, and choice-memory persistence.
- `UARNPCSubsystem` is now talkable-cache-focused and derives NPC talkable state from dialogue unlock availability.
- `AARNPCCharacterBase` remains the world interaction entrypoint; replicated dialogue talkable state is owned by `UARNPCTalkComponent` (`bIsTalkable` + per-slot mask).

Ownership reminder:

- This runtime is inside the Dialogue plugin ownership boundary.
- Faction voting/election orchestration and ordering loops are built-on-top systems, not dialogue-owned runtime.
- Shop/customer-serving built-on-top systems should route serving results through `ApplyRamenServeOutcome(...)` for relationship + emotion output.

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
- `ApplyRamenServeOutcome(...)` (built-on-top customer/order systems)

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

Shared dialogue types live in `Source/AlienRamen/Public/ARDialogueTypes.h`.

- Speakers: `FARDialogueSpeakerRow` rows resolved through TagContentResolver routes from `SpeakerDefinitionRootTag`.
- Conversations: `UARDialogueConversationAsset` with:
  - `Header` (`FDialogueConversationHeader`)
  - `CompiledData` (`FDialogueCompiledConversationData`)
- Lines are embedded per conversation (`FDialogueConversationLine`), not global rows.
- Conversation registry source:
  - TagContentResolver DataTable rows (`FARDialogueConversationAssetRow`) routed by `ConversationDefinitionRootTag` (row tag or built tag from root+row name).

Settings live in `Source/AlienRamen/Public/ARDialogueSettings.h`:

- `SpeakerDefinitionRootTag` (speaker row lookup root)
- `ConversationDefinitionRootTag` (conversation lookup row root)
- shared/per-player mode tag containers
- execution guard `MaxExecutionStepsPerAdvance`

Default config now uses `SpeakerDefinitionRootTag=Dialogue.Speaker` and `ConversationDefinitionRootTag=Dialogue.Conversation`.

## Emotion Runtime

- `UAREmotionComponent` provides replicated overhead-emotion display state for NPCs and player characters.
- Emotion state is server-authoritative with shared + per-slot variants (`P1` / `P2`).
- Dialogue applies session-scoped emotion overrides and clears them when the session ends, revealing base state again.
- Dialogue line `SpeakerTag` may include an emotion leaf (example: `Dialogue.Speaker.Fred.Angry`).
- Emotion icon lookup is resolved from a direct DataTable reference in `UAREmotionSettings` (row type `FAREmotionIconRow`) and cached by `UAREmotionResolverSubsystem`.
- Fallback order is: exact requested tag first, then generic fallback under `GenericEmotionRootTag` (default `Dialogue.Emotion`).
- `UAREmotionComponent` remains light-weight authoring: anchor placement + local icon size + optional local preview tag.

## Offer + Execution Rules (Current Runtime)

- Offer selection is server-side and bucketed:
  - unseen
  - game-seen/player-unseen catch-up
  - repeatable/other
- Highest numeric priority wins inside the first non-empty bucket.
- Equal-priority candidates resolve by weighted random using `OfferWeight` (default `1`).
- `ChanceOffered` (`0..1`) is rolled per candidate during offer evaluation; failed rolls mark that conversation skipped for the player this cycle.
- Per-cycle blockers are player-specific (`seen this cycle`, `skipped this cycle`), controlled by `bBlockOfferPerCycle` (default `true`).
- Even when per-cycle blocking is disabled, seen/skipped-this-cycle and repeatable+completed-by-player candidates are de-prioritized to effective priority `1`.
- Offer checks include:
  - mode enabled by `UARDialogueSettings` shared/per-player mode tags
  - primary speaker exact match
  - optional conversation-level active-character restriction (`Any` / `BrotherOnly` / `SisterOnly`)
  - relationship minimum
  - locked/blocked condition groups
  - seen/completed/repeatability suppression flags
- Runtime executes compiled nodes server-side with a step cap from settings.
- Implemented node execution: enter/completed/line/multiline/split-line/choice/bool/switch/route/route-by-character/tag-mutation/relationship-mutation/faction-mutation/random/sequence.
- Line-node auto-advance is now a per-player runtime preference on `AARPlayerStateBase` (`SetDialogueAutoAdvanceEnabled`), not authored per line node.
- Runtime line presentation supports token + style parsing at execute time (source `FText` is kept authored/localized, formatting is applied on the delivered view text):
  - lookup tokens: `[Some.Gameplay.Tag-displayname]` (or other field names), plus shortcuts `[Speaker]`, `[Brother]`, `[Sister]`
  - unknown/failed commands fail loudly: unresolved bracket commands are replaced with `UNKNOWN` and logged as runtime errors
  - simple style markers: `*bold*`, `**italic**`, `***bold+italic***`, `--strike--`
  - font wrappers: `[font:StyleTag]...[/font]` (auto-closes at line end if not explicitly closed)
- Important conversation and important choice flow force passive players into participants/eavesdrop set before interaction.
- Line nodes (including multiline entries) support the same convenience active-character restriction (`Any` / `BrotherOnly` / `SisterOnly`) before skip-conditions are evaluated.
- Blocked-condition defaults now align to spec intent (`Any` by default on blocked groups); locked groups remain `All` by default.
- Logging: normal gating/selection outcomes are logged at `Verbose` level in `ARLog`; invalid graph/runtime corruption is logged as `Warning`/`Error` with conversation tag/session context for debugging.

## Seen vs Completed

- Per-cycle player offer blockers (`seen this cycle` / `skipped this cycle`) are persisted per player in save until explicitly cleared via `ClearConversationCycleOfferState(...)`.
- Runtime still keeps active-session transient containers for fast gating/evaluation.
- Completed state is persistent and save-backed.
- Completion is written only when a `Completed` node executes.
- Per-cycle offer blockers can be reset explicitly via `ClearConversationCycleOfferState(...)` (single slot or all slots).
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

NPC actor integration now routes through `UARNPCTalkComponent`:

- `UARNPCTalkComponent` owns NPC-side dialogue interaction + replicated dialogue talkable mask/state.
- `AARNPCCharacterBase` remains the owner of non-dialogue local NPC gates (for example serving/customer mode) and combines that with component talkability for public NPC talk checks.

## Emotion Resolver Runtime

- `UAREmotionResolverSubsystem` caches emotion tag->icon mappings from `UAREmotionSettings::EmotionDataTable`.
- Resolver cache invalidates/rebuilds when configured settings inputs change or when the bound emotion DataTable broadcasts `OnDataTableChanged`.
- Debug console commands:
  - `ar.emotion.LogCacheStats`
  - `ar.emotion.RebuildCache`

## Editor Tooling (Current)

Registered editor tabs:

- `Dialogue Speaker Editor` (`SDialogueSpeakerEditorPanel`)
- `Dialogue Conversation Graph Editor` (`SDialogueConversationGraphEditorPanel`)

Conversation graph tooling now provides:

- blueprint-style `SGraphEditor` canvas with right-click node creation
- right-click node creation actions are flat/top-level (no nested "Dialogue Nodes" submenu)
- graph node classes/schema (`UARDialogueEdGraph`, `UARDialogueEdGraphNode`, `UARDialogueEdGraphSchema`)
- line nodes now render with inline authoring UI: speaker portrait button (click to cycle speakers) + wrapped inline line-text edit
- custom graph nodes and add-node context actions expose explicit hover tooltips (Blueprint-style)
- drag-link execution wiring with:
  - one outgoing link per output pin
  - multiple incoming links allowed per node input
- full toolbar flow: Save / Validate / Compile Runtime Graph / Focus Enter / Auto Layout / Preview
- details-panel editing for selected node or conversation root
- dynamic branch-pin behavior for choice/switch/random/route-by-character nodes driven by stable branch GUIDs
- split-line node authoring uses multiline-style inline line rows, but runtime selects only the first row matching the active player character and otherwise skips to `Next`
- compile-from-editor-graph into `CompiledData` with node-level validation markers
- validation + preview execution through runtime dialogue subsystem even when PIE is not running
- no standalone in-tab global conversation list; graph tab edits a targeted conversation (speaker-hub handoff or explicit asset picker selection)
- preview trace output supports multi-step execution (line waits + auto-choice routing), plus preview-seen flags and typed injected variables
- speaker-tag editor fields are gameplay-tag-filtered to `Dialogue.Speaker.*` (header primary/participants, line speaker, relationship target, portrait-tag metadata surfaces)
- speaker rows include optional `LineFont` (`UFont` soft reference) for widget-level dialogue font styling; legacy style-tag wrapping remains a fallback path
- compile/create flow ensures `ParticipatingSpeakerTags` always includes the conversation primary speaker and `Dialogue.Speaker.Player`

Speaker hub currently provides:

- TagContentResolver-backed speaker table loading from `SpeakerDefinitionRootTag`
- searchable/sortable speaker list with columns (display name/tag/thresholds/conversation count)
- speaker CRUD (`New`, `Duplicate`, `Delete`) + `Save Speaker` + `Validate Speaker`
- reorderable threshold editing/reset (`5,15,30,50` defaults)
- inline portrait list with add/update/remove operations
- relationship-level grouped conversation map for selected primary speaker with structured gate/mutation summaries and unlock-chain hints
- conversation cards are draggable between level headers to change minimum-relationship level assignment (replaces level cycle toggle)
- conversation map `Locked by` is inline editable as speaker-scoped gameplay-tag locks (`Dialogue.Conversation.Id.<Speaker>.*`)
- conversation map `Required Tags` uses gameplay tag container editing (no CSV string entry)
- conversation map rows expose right-click context actions for `Open`, `Rename`, `Duplicate`, `Remove From Lookup`, and `Delete Asset + Remove From Lookup`
- speaker editor supports transaction-backed `Ctrl+Z` / `Ctrl+Y` (`Ctrl+Shift+Z`) undo/redo for conversation create/duplicate/delete flows
- stale lookup rows are cleaned when referenced conversation assets are deleted; removed conversation tags are stripped from lock/block condition references in remaining conversations
- generated conversation tag config cleanup is explicit via `Cleanup Tags` action (not implicit during delete), keeping undo/redo behavior predictable
- conversation create/open actions and broken-conversation scan using runtime validator
