# Dialogue + Speaker Runtime (`UARDialogueSubsystem`, `UARSpeakerSubsystem`)

Plugin ownership boundary reference: [Dialogue Plugin Ownership Boundary](README_DialoguePluginBoundary.md)

## Plugin Migration Notes (Current)

- Parley plugin owns dialogue/speaker/faction runtime (`UParleyDialogueSubsystem`, `UParleySpeakerSubsystem`, `UParleySpeakerComponent`, `UParleyFactionSubsystem`).
- Emo plugin owns emotion display/resolution/HUD base (`UEmoComponent`, `UEmoResolverSubsystem`, `AEmoHUDBase`).
- Parley is save-agnostic and Emo-agnostic. Game integration is done by AR-owned bridges (`UARParleySaveBridge`, `AARNPCCharacterBase`).
- `AARNPCCharacterBase` remains component-optional safe: speaker, emotion, and customer components may each be absent and runtime paths must early-out safely.
- `AARNPCCharacterBase` bridges Parley speaker emotion signals to Emo only when an emotion component exists.

## Overview

Alien Ramen now uses a conversation-asset, compiled-graph dialogue runtime:

- `UARDialogueSubsystem` is server-authoritative for offer selection, session execution, branching, eavesdrop, completion persistence, and choice-memory persistence.
- `UARSpeakerSubsystem` is now talkable-cache-focused and derives speaker talkable state from dialogue unlock availability.
- `AARNPCCharacterBase` remains the world interaction entrypoint; replicated dialogue talkable state is owned by `UARSpeakerComponent` (`bIsTalkable` + per-slot mask).

Ownership reminder:

- This runtime is inside the Dialogue plugin ownership boundary.
- Faction voting/election orchestration and ordering loops are built-on-top systems, not dialogue-owned runtime.
- Shop/customer-serving built-on-top systems should route serving results through `ApplyRamenServeOutcome(...)` for relationship + emotion output.

## Runtime Entry Points

Player/UI entrypoints route through `AARPlayerController` RPC wrappers:

- `RequestStartDialogue(FGameplayTag SpeakerTag)`
- `RequestInteractWithCharacter(AARNPCCharacterBase* CharacterActor)`
- `RequestAdvanceDialogue()`
- `RequestSubmitDialogueChoice(FGuid ChoiceBranchId)`
- `RequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot)`
- `RequestSetDialogueEavesdropOtherPlayer(bool bEnable)` (slot convenience wrapper)
- Actor-targeted interaction requests are server reachability-gated by controller pawn distance (`AARPlayerController::ServerInteractionMaxDistance`) before runtime mutation.

World actor convenience entrypoint:

- `AARNPCCharacterBase::ForwardUseToController(AActor* UsingActor)` accepts either controller or pawn references (for example BI_Interactable payloads), resolves the owning `AARPlayerController`, and routes through `RequestInteractWithCharacter(...)`.

Server runtime now pushes authoritative view snapshots back to client controllers via:

- `ClientDialogueSessionUpdated(const FDialogueClientView& View)`
- `ClientDialogueSessionEnded(const FString& SessionId)`

Core subsystem API:

- `GetAvailableConversationForSpeaker(...)`
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

- `TryStartDialogueWithSpeaker(...)`
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
- `UARDialogueWidgetBase` forwards core interaction calls (`AdvanceDialogue`, `SubmitChoice`, `SetEavesdrop`, `SetEavesdropOtherPlayer`, `StartDialogueWithSpeakerTag`, `InteractWithCharacter`) back to the bound controller.
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

- `UAREmotionComponent` provides replicated overhead-emotion display state for speaker actors and player characters.
- Emotion state is server-authoritative with shared + per-slot variants (`P1` / `P2`).
- Emotion display precedence is now: `System Override` (source+priority arbitration) -> `Dialogue Override` -> `Base`.
- Dialogue applies session-scoped emotion overrides and clears them when the session ends, revealing base state again.
- Dialogue line emotion is written as a system source (`DialogueLine`) with priority above `BusyEmotionPriority`; if line-tag resolve fails, no line source is written so lower-priority state (for example busy) remains visible.
- Dialogue clears prior line-emotion source entries when presenting the next line, so line emotion holds until next line or session end.
- Dialogue line `SpeakerTag` may include an emotion leaf (example: `Dialogue.Speaker.Fred.Angry`).
- Emotion icon lookup resolves through `UTagContentResolverSubsystem` route root `UAREmotionSettings::EmotionResolverRootTag` (row type `FAREmotionIconRow`) and is cached by `UAREmotionResolverSubsystem`.
- Fallback order is: exact requested tag first, then generic fallback under `GenericEmotionRootTag` (default `Dialogue.Emotion`) when a speaker tag includes an explicit emotion leaf (for example `Dialogue.Speaker.Fred.Angry` -> `Dialogue.Emotion.Angry`).
- `UAREmotionComponent` remains light-weight authoring: anchor placement + local icon size + optional local preview tag.
- Emotion anchor authoring is offset-only: `AnchorWorldOffset` is applied from owner actor top bounds fallback.
- Built-on-top systems can set/clear generic system overrides by source id and priority (`SetSystemEmotionTag*` / `ClearSystemEmotionTag*`), including timed auto-clear helpers (`SetSystemEmotionTagForDuration*`) with default duration from `UAREmotionSettings::DefaultTimedSystemOverrideDurationSeconds`.
- Runtime overhead emotion rendering is owned directly by `AARHUDBase` (no separate HUD emotion component).
- `AARHUDBase::DrawHUD` renders overhead emotions natively and applies projection/size/occlusion policy from HUD properties.
- Runtime suppression (for example cutscenes) should use `AARHUDBase::SetEmotionRenderingSuppressed(...)`.

## Offer + Execution Rules (Current Runtime)

- Offer selection is server-side and bucketed:
  - unseen
  - game-seen/player-unseen catch-up
  - repeatable/other
- Highest numeric priority wins inside the first non-empty bucket.
- Equal-priority candidates resolve by weighted random using `OfferWeight` (default `1`).
- `ChanceOffered` (`0..1`) is rolled per candidate during offer evaluation; failed rolls mark that conversation skipped for the player this cycle.
- Per-cycle blockers are character-specific (`seen this cycle`, `skipped this cycle`), controlled by `bBlockOfferPerCycle` (default `true`).
- Even when per-cycle blocking is disabled, seen/skipped-this-cycle and repeatable+completed-by-player candidates are de-prioritized to effective priority `1`.
- Offer checks include:
  - mode enabled by `UARDialogueSettings` shared/per-player mode tags
  - primary speaker exact match
  - per-speaker cycle offer cap (`FARDialogueSpeakerRow::MaxOffersPerCycle`, `0` = unlimited) evaluated per character state
  - optional conversation-level active-character restriction (`Any` / `BrotherOnly` / `SisterOnly`)
  - relationship minimum
  - locked/blocked condition groups
  - seen/completed/repeatability suppression flags
- Runtime executes compiled nodes server-side with a step cap from settings.
- Implemented node execution: enter/completed/line/multiline/split-line/choice/bool/switch/route/route-by-character/tag-mutation/relationship-mutation/faction-mutation/random/sequence.
- Line-node auto-advance is now a per-player runtime preference on `AARPlayerStateBase` (`SetDialogueAutoAdvanceEnabled`), not authored per line node.
  - that preference is persisted as player-owned save data
- Runtime line presentation supports token + style parsing at execute time (source `FText` is kept authored/localized, formatting is applied on the delivered view text):
  - lookup tokens: `[Some.Gameplay.Tag-displayname]` (or other field names), plus shortcuts `[Speaker]`, `[Brother]`, `[Sister]`
  - unknown/failed commands fail loudly: unresolved bracket commands are replaced with `UNKNOWN` and logged as runtime errors
  - simple style markers: `*bold*`, `**italic**`, `***bold+italic***`, `--strike--`
  - font wrappers: `[font:StyleTag]...[/font]` (auto-closes at line end if not explicitly closed)
- Important conversation and important choice flow force passive players into participants/eavesdrop set before interaction.
- Shop eavesdrop requests are immediate-only: `ForceEavesdrop` rejects enable requests when the target slot has no active dialogue session (no queued eavesdrop registration).
- Conversations can be authored as private (`FDialogueConversationHeader::bPrivateConversation`): active private sessions reject eavesdrop requests by default.
- Important choice flow overrides private-session eavesdrop lock while the choice is actively forcing all viewers.
- Per-player mode supports optional busy-speaker lock (`UARDialogueSettings::bOnlyOneTalkerPerSpeakerInPerPlayerModes`): when enabled, offers/starts for a speaker already owned by another active session are blocked, and optional auto-eavesdrop fallback can be enabled (`bAutoEavesdropOnBusySpeakerByDefault`).
- Busy query helpers are exposed for gameplay/UI traces: `UARDialogueSubsystem::IsSpeakerBusyForController(...)` and `AARNPCCharacterBase::IsSpeakerBusyForController(...)`.
- Busy-speaker presentation routes through emotion-system source overrides (source `DialogueBusy`) using `UAREmotionSettings::BusyEmotionTag` and `BusyEmotionPriority`.
- Line nodes (including multiline entries) support the same convenience active-character restriction (`Any` / `BrotherOnly` / `SisterOnly`) before skip-conditions are evaluated.
- Blocked-condition defaults now align to spec intent (`Any` by default on blocked groups); locked groups remain `All` by default.
- Logging: normal gating/selection outcomes are logged at `Verbose` level in `ARLog`; invalid graph/runtime corruption is logged as `Warning`/`Error` with conversation tag/session context for debugging.

## Seen vs Completed

- Per-cycle offer blockers (`seen this cycle` / `skipped this cycle`) are persisted per character in save until explicitly cleared via `ClearConversationCycleOfferState(...)`.
- Per-cycle speaker offer counts are also persisted per character (`SpeakerOfferCountsThisCycle`) and gate speaker offer/start paths when `MaxOffersPerCycle > 0`.
- Runtime still keeps active-session transient containers for fast gating/evaluation.
- Completed state is persistent and save-backed.
- Completion is written only when a `Completed` node executes.
- Per-cycle offer blockers can be reset explicitly via `ClearConversationCycleOfferState(...)` (single slot or all slots).
- Save system enforces no mid-conversation saves by checking active dialogue sessions before `SaveCurrentGame`.

## Choice Memory

- Runtime choice picks are tracked per session (`ChoiceNodeId -> BranchId`).
- On completed conversation:
  - game completion tag is persisted
  - initiating character completion tag is persisted
  - per-choice memory records are persisted on character-owned save rows (`FDialogueChoiceMemoryRecord`)
- Choice nodes in `LockedToRecordedChoice` mode auto-route to persisted branch when encountered after completion.
- If a completed conversation is replayed and a locked choice record is missing/unroutable, runtime now fails closed (ends non-completed) instead of reopening free choice.

## Persistence

Save schema is now `v11` with dialogue split by ownership:

- shared:
  - `DialogueRelationshipStates`
  - `DialogueCompletedConversationTagsByGame`
- character-owned:
  - `CharacterStates[].DialogueState`

Legacy `DialoguePlayerPersistentStates` rows migrate into `CharacterStates[]` by resolving the saved active character for the matching player identity/slot.

## Speaker Talkable Runtime

`UARSpeakerSubsystem` no longer owns ramen/want mutation logic. It now:

- caches `SpeakerTag -> bTalkable`
- refreshes from `UARDialogueSubsystem::HasUnlockedDialogueForSpeakerForAnyPlayer(...)`
- enumerates refresh targets from dialogue runtime registered speaker tags (conversation primaries + speaker registry), not synthesized DataTable row-name tags
- broadcasts `OnSpeakerTalkableChanged`
- combines subsystem talkable state with `AARNPCCharacterBase::bSpeakerLocalStateAllowsDialogue` (local speaker runtime gate, for example ordering-mode lockouts)

Speaker actor integration now routes through `UARSpeakerComponent`:

- `UARSpeakerComponent` owns speaker-side dialogue interaction + replicated dialogue talkable mask/state.
- `AARNPCCharacterBase` remains the owner of non-dialogue local speaker gates (for example serving/customer mode) and combines that with component talkability for public speaker talk checks.
- effective talkable state drives a persistent emotion-system override source `TalkableState` using `UAREmotionSettings::WantsToTalkEmotionTag` (set while talkable, cleared when not talkable).

## Emotion Resolver Runtime

- `UAREmotionResolverSubsystem` caches emotion tag->icon mappings from TagContentResolver route root `UAREmotionSettings::EmotionResolverRootTag` (default `Dialogue.Emotion`).
- Resolver cache invalidates/rebuilds when configured settings inputs change or when the bound emotion DataTable broadcasts `OnDataTableChanged`.
- Debug console commands:
  - `ar.emotion.log_cache_stats`
  - `ar.emotion.rebuild_cache`
- Optional diagnostics toggles in `UAREmotionSettings`:
  - `bEnableVerboseResolverLogs`
  - `bEnableVerboseRenderLogs`

## Editor Tooling (Current)

Registered editor tabs:

- `Dialogue Speaker Editor` (`SDialogueSpeakerEditorPanel`)
- `Dialogue Conversation Graph Editor` (`SDialogueConversationGraphEditorPanel`)

Conversation graph tooling now provides:

- blueprint-style `SGraphEditor` canvas with right-click node creation
- right-click node creation actions are flat/top-level (no nested "Dialogue Nodes" submenu)
- graph node classes/schema (`UARDialogueEdGraph`, `UARDialogueEdGraphNode`, `UARDialogueEdGraphSchema`)
- line nodes now render with inline authoring UI: speaker portrait button (left-click cycles base speakers from participants/graph usage, right-click opens emotion-tag picker under current speaker) + wrapped inline line-text edit
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
- compile/create flow ensures `ParticipatingSpeakerTags` always includes the conversation primary speaker and `Dialogue.Speaker.Player`; line-speaker edits also auto-add the selected base speaker so cycle convenience stays current during authoring
- Speaker details authoring categories for actor/talk/emotion properties use distinct roots (`Alien Ramen|Speaker`, `Alien Ramen|Talk`, `Alien Ramen|Emotion`) to avoid repeated same-name category buckets in Blueprint class-default details.

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
