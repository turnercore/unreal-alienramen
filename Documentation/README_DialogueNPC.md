# Dialogue + Speaker Runtime (`UParleyDialogueSubsystem`, `UParleySpeakerSubsystem`)

Plugin ownership boundary reference: [Dialogue Plugin Ownership Boundary](README_DialoguePluginBoundary.md)

## Plugin Migration Notes (Current)

- Parley plugin owns dialogue/speaker/faction runtime (`UParleyDialogueSubsystem`, `UParleySpeakerSubsystem`, `UParleySpeakerComponent`, `UParleyFactionSubsystem`).
- Emo plugin owns emotion display/resolution/HUD base (`UEmoComponent`, `UEmoResolverSubsystem`, `AEmoHUDBase`).
- Parley is save-agnostic and Emo-agnostic. Game integration is done by AR-owned bridges (`UARParleySaveBridge`, `AARNPCCharacterBase`).
- Alien Ramen also owns the interaction-context assembly that Parley consumes at the plugin boundary. `AARPlayerStateBase` builds `FARInteractionContext`, and Parley only receives the resulting tag buckets.
- `AARNPCCharacterBase` remains component-optional safe: speaker, emotion, and customer components may each be absent and runtime paths must early-out safely.
- `AARNPCCharacterBase` bridges Parley speaker emotion signals to Emo only when an emotion component exists.

## Overview

Alien Ramen now uses a conversation-asset, compiled-graph dialogue runtime:

- `UParleyDialogueSubsystem` is server-authoritative for offer selection, session execution, branching, eavesdrop, completion persistence, and choice-memory persistence.
- `UParleySpeakerSubsystem` is now talkable-cache-focused and derives speaker talkable state from dialogue unlock availability.
- `AARNPCCharacterBase` remains the world interaction entrypoint; replicated dialogue talkable state is owned by `UParleySpeakerComponent` (`bIsTalkable` + per-slot mask).

Ownership reminder:

- This runtime is inside the Dialogue plugin ownership boundary.
- The plugin does not know about Alien Ramen persistence ownership buckets. It only receives generic `GameTags`, `PlayerTags`, and `CombinedTags` inputs from the game.
- Faction voting/election orchestration and ordering loops are built-on-top systems, not dialogue-owned runtime.
- Shop/customer-serving built-on-top systems should bridge customer outcomes into Parley relationship mutations and project-owned emotion presentation instead of relying on a plugin-owned helper.

## Runtime Entry Points

Player/UI entrypoints route through `AARPlayerController` RPC wrappers:

- `RequestStartDialogue(FGameplayTag SpeakerTag)`
- `RequestInteractWithCharacter(AARNPCCharacterBase* CharacterActor)`
- `RequestAdvanceDialogue()`
- `RequestSubmitDialogueChoice(FGuid ChoiceBranchId)`
- `RequestSetDialogueEavesdropByCharacter(bool bEnable, FGameplayTag TargetCharacterTag)` (character-native targeting)
- `RequestSetDialogueEavesdropOtherPlayer(bool bEnable)` (targets the opposite canonical character of the local controller)
- Actor-targeted interaction requests are server reachability-gated by controller pawn proximity to the target actor's collision bounds (`AARPlayerController::ServerInteractionMaxDistance`) before runtime mutation.

World actor convenience entrypoint:

- `AARNPCCharacterBase::ForwardUseToController(AActor* UsingActor)` accepts either controller or pawn references (for example BI_Interactable payloads), resolves the owning `AARPlayerController`, and routes through `RequestInteractWithCharacter(...)`.

Server runtime now pushes authoritative view snapshots back to client controllers via:

- `ClientDialogueSessionUpdated(const FDialogueClientView& View)`
- `ClientDialogueSessionEnded(const FString& SessionId)`

Core subsystem API:

- `GetAvailableConversationForSpeaker(...)`
- `StartConversation(...)`
- `StartConversationByTagForCharacters(...)` (scripted start by requester/owner character tags + exact conversation tag; bypasses standard offer/reoffer gate checks)
- `TryStartDialogueBetweenSpeakers(...)` (explicit source-speaker + target-speaker start; ownership resolves through runtime character/controller identity)
- `ClearConversationCycleOfferState(...)` (resets cycle offer blockers/counters; all-slot reset also clears transient speaker override maps)
- `GrantManualOfferOverrideForSpeaker(...)` (server/BP hook to grant per-speaker manual offer credits that bypass cycle-policy caps)
- `OfferConversationTagNowForSpeaker(...)` (server/BP hook to force the next/current offer for a speaker to a specific conversation tag)
- `ClearManualOfferOverrideForSpeaker(...)`
- `ClearForcedConversationOfferForSpeaker(...)`
- `AdvanceConversation(...)`
- `SubmitChoice(...)`
- `ForceEavesdrop(...)`
- `ValidateConversation(...)`
- `ValidateSpeaker(...)`
- `PreviewConversation(...)`
- `PreviewConversationTrace(...)` (tooling-oriented multi-step trace simulation)
- game-owned relationship bridge from customer/order systems
- `OnDialogueSignalFired` (broadcast from Signal nodes with signal/payload tags plus conversation/speaker/owner-character context)
- `OnDialogueAudioRequested` (broadcast when line audio resolves into either native sound payload or cue-tag signal payload)

## Interaction Context Boundary

Alien Ramen now builds the requester's current interaction identity on `AARPlayerStateBase` and feeds Parley from that context.

`FARInteractionContext::CombinedTags` includes:
- `GameProgressionTags`
- `PlayerProgressionTags`
- `CharacterProgressionTags`
- caller-supplied transient interaction tags
- current speaker tags
- canonical character + player-slot identity tags
- projected loadout tags
- live ASC owned tags
- character-owned activated `Invader.Upgrade.*` tags

`CombinedTags` intentionally excludes:
- shared `AARInvaderGameState` spicy-track offers
- shared slotted track upgrades that are merely available but not owned by the current character/player

Parley's authored condition sources stay the same:
- `GameTags` stays save-wide/game-only
- `PlayerTags` stays player-only
- `CombinedTags` now means "full current interaction identity as provided by Alien Ramen"

## Signal Node (How To Use)

`Signal` is a single-input/single-output passthrough node for game-layer hooks without embedding arbitrary gameplay code in dialogue graphs.

Authoring flow:

1. In the conversation graph, add a `Signal` node from the context menu.
2. Set `SignalTag` under `Dialogue.Signal.*` (example: `Dialogue.Signal.GiveReward`).
3. Optionally set `PayloadTags` for extra context (example: `Item.Weapon.Shotgun`).
4. Wire the `Next` output to continue dialogue flow.

Runtime behavior:

- On authoritative runtime execution, the node broadcasts `UParleyDialogueSubsystem::OnDialogueSignalFired`.
- Broadcast payload: `SignalTag`, `PayloadTags`, `ConversationTag`, `SpeakerTag`, `OwnerCharacterTag`.
- The node immediately advances to `NextNodeId`.
- Conversation preview/trace tooling treats `Signal` as passthrough and does not broadcast gameplay signals.
- Validation warns when `SignalTag` is unset and errors on payload type mismatch.

Binding patterns:

- Blueprint: bind to `OnDialogueSignalFired` on `UParleyDialogueSubsystem`, branch on `SignalTag`, then run game-specific logic.
- C++: bind in a game-owned listener and filter by tag.

```cpp
void UMySystem::Init(UParleyDialogueSubsystem* Dialogue)
{
	if (Dialogue)
	{
		Dialogue->OnDialogueSignalFired.AddDynamic(this, &UMySystem::HandleDialogueSignal);
	}
}

void UMySystem::HandleDialogueSignal(
	FGameplayTag SignalTag,
	FGameplayTagContainer PayloadTags,
	FGameplayTag ConversationTag,
	FGameplayTag SpeakerTag,
	FGameplayTag OwnerCharacterTag)
{
	if (SignalTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Dialogue.Signal.GiveReward"))))
	{
		// Game-owned reaction logic goes here.
	}
}
```

## Dialogue Audio Modes

Parley dialogue audio is mode-driven from `UParleyDialogueSettings::DialogueAudioMode`:

- `NativeAudio`
  - dialogue resolves native `USoundBase` payloads
  - line `Sound` has priority
  - if line `Sound` is empty, speaker emotion fallback sound can supply audio
- `AudioSignals`
  - native sounds are suppressed
  - dialogue resolves cue-tag payloads only
  - line `AudioCueTag` has priority
  - if line cue is empty, speaker emotion fallback cue can supply signal audio

Speaker emotion fallback audio is authored in speaker rows (`FParleySpeakerRow::EmotionAudioFallbacks`) and keyed by emotion tags.

Local delivery contract:

- server runtime resolves the request once per delivered line
- Parley forwards local audio requests to participating player controllers
- AR layer handles per-machine dedupe (`SessionId + LineGuid`) for couch co-op
- AR FMOD bridge resolves cue tags through `FARDialogueAudioCueFMODRow` DataTable mappings and plays 2D FMOD events locally

## Runtime UI Bridge

Runtime UI is intentionally separate from editor preview tooling.

- `UParleyDialogueWidgetBase` (`Plugins/Parley/Source/Parley/Public/ParleyDialogueWidgetBase.h`) is the shared Blueprint-facing widget base for dialogue presentation and input forwarding.
- `AARPlayerController` exposes the local runtime UI bridge for dialogue input and controller-bound state, but the HUD owns whether the widget exists on screen:
  - dialogue delegates: `OnDialogueViewUpdated`, `OnDialogueSessionEndedSignal`
  - cached view helpers: `GetCachedDialogueView(...)`, `QueryLocalDialogueView(...)`
  - widget lifecycle: `EnsureDialogueWidget()`, `RemoveDialogueWidget()`, `GetDialogueWidget()`
- `AARHUDBase` owns the viewport-side dialogue widget presentation:
  - auto-widget config: `bAutoCreateDialogueWidget`, `DialogueWidgetClass`, `DialogueWidgetZOrder`
  - widget instance management: `EnsureDialogueWidget(...)`, `RemoveDialogueWidget()`, `GetDialogueWidget()`
- `AARPlayerController::RequestHUDInitialization()` refreshes HUD presentation when the local controller or current character changes, so the existing widget instance can stay alive and just repush view/state when possible.
- `UParleyDialogueWidgetBase` forwards core interaction calls (`AdvanceDialogue`, `SubmitChoice`, `SetEavesdrop`, `SetEavesdropOtherPlayer`, `StartDialogueWithSpeakerTag`, `InteractWithCharacter`) back to the bound controller.
- The widget base now also mirrors the common Parley runtime signal surface into Blueprint events out of the box:
  - dialogue session/view lifecycle: initialized, session started, view updated, session ended, deinitialized
  - dialogue events: conversation started/ended, line delivered, important choice made, relationship changes, progression tag mutations, progression dirty, choice lookahead, signal fired, audio requested
  - shared runtime events: speaker talkable changes, faction popularity changes, faction speaker reputation changes
- The widget now caches its bound character tag explicitly and exposes it through `GetBoundCharacterTag()`, `SetBoundCharacterTag(...)`, and `BP_OnDialogueWidgetBoundCharacterChanged(...)`, so HUD and UMG logic can react to character swaps without inferring identity from the controller every time.
- Default widget behavior can auto-toggle visibility from dialogue state (visible when view updates arrive, collapsed on session end/deinit).
- Client runtime now mirrors controller RPC dialogue updates back into `UParleyDialogueSubsystem::OnDialogueSessionUpdated/OnDialogueSessionEnded` so subsystem-bound widgets receive live updates on clients without extra project glue.

## Editor Validation Lifetime

- Parley editor validation tooling should cache a transient `UParleyDialogueSubsystem` under the transient package when PIE is not active so `UParleyDialogueSubsystem::ValidateConversation(...)` / `ValidateSpeaker(...)` can run without a live runtime world.
- Do not create or retain a transient `UGameInstance` for editor-only validation. PIE teardown treats lingering transient game instances as stale play-session state and will assert even when they are only refcounted.
- That editor-only validation subsystem must not be kept alive with manual `AddToRoot()` ownership. Use `TStrongObjectPtr` or another scoped owner so editor/PIE teardown does not hit rooted-object asserts during cleanup.
- The Parley editor module now clears those validation caches on `PreBeginPIE`, `PrePIEEnded`, and `EndPIE` so no editor-only validation object survives across play-session boundaries.

## Content Model

Shared dialogue types live in `Plugins/Parley/Source/Parley/Public/ParleyDialogueTypes.h`.

- Speakers: `FParleySpeakerRow` rows resolved through TagKey routes from `SpeakerDefinitionRootTag`.
- Conversations: `UParleyConversationAsset` with:
  - `Header` (`FDialogueConversationHeader`)
  - `CompiledData` (`FDialogueCompiledConversationData`)
- Lines are embedded per conversation (`FDialogueConversationLine`), not global rows.
- Conversation registry source:
  - TagKey DataTable rows (`FParleyConversationAssetRow`) routed by `ConversationDefinitionRootTag` (row tag or built tag from root+row name).

Settings live in `Plugins/Parley/Source/Parley/Public/ParleyDialogueSettings.h`:

- `SpeakerDefinitionRootTag` (speaker row lookup root)
- `ConversationDefinitionRootTag` (conversation lookup row root)
- shared/per-player mode tag containers (shared-session behavior routing only; not an offer/start gate)
- execution guard `MaxExecutionStepsPerAdvance`
- audio mode switch `DialogueAudioMode` (`NativeAudio` / `AudioSignals`)

Default config now uses `SpeakerDefinitionRootTag=Parley.Speaker` and `ConversationDefinitionRootTag=Parley.Conversations`.

## Emotion Runtime

- `UEmoComponent` provides replicated overhead-emotion display state for speaker actors and player characters.
- Emotion state is now a generic registration pool: `SourceId + TargetViewerTags -> EmotionTag/Priority/WriteSerial`.
- Viewer matching is exact-tag overlap only. Empty target tags are global fallback registrations.
- Resolution order is:
  - highest priority first
  - targeted registrations before global registrations on equal priority
  - latest write when same-priority targeted registrations still tie
- Same-priority targeted winner conflicts log a warning and still resolve deterministically.
- `SetEmotionRegistration*` / `ClearEmotionRegistration*` are the only write surface in `UEmoComponent`; slot-specific and layer-specific APIs were removed.
- Dialogue/session systems now bridge through those generic registrations instead of writing plugin-owned base/dialogue/system layers.
- Dialogue applies session-scoped emotion registrations and clears them when the session ends.
- Dialogue line emotion is written as a named registration with priority above `BusyEmotionPriority`; if line-tag resolve fails, no line registration is written so lower-priority state (for example busy) remains visible.
- Dialogue clears prior line-emotion registrations when presenting the next line, so line emotion holds until next line or session end.
- Dialogue line `SpeakerTag` may include an emotion leaf (example: `Parley.Speaker.Fred.Angry`).
- Emotion icon lookup resolves through `UTagKeySubsystem` route root `UEmoSettings::EmotionResolverRootTag` (row type `FEmoIconRow`) and is cached by `UEmoResolverSubsystem`.
- Fallback order is: exact requested tag first, then generic fallback under `GenericEmotionRootTag` (default `Parley.Emotion`) when a speaker tag includes an explicit emotion leaf (for example `Parley.Speaker.Fred.Angry` -> `Parley.Emotion.Angry`).
- `UEmoComponent` remains light-weight authoring: anchor placement + local icon size + optional local preview tag.
- Emotion anchor authoring is offset-only: `AnchorWorldOffset` is applied from owner actor top bounds fallback.
- Built-on-top systems can set/clear generic registrations by source id and priority, including timed auto-clear helpers, with default duration from `UEmoSettings::DefaultTimedSystemOverrideDurationSeconds`.
- Runtime overhead emotion rendering is owned directly by `AARHUDBase` (no separate HUD emotion component).
- `AARHUDBase::DrawHUD` renders overhead emotions natively and applies projection/size/occlusion policy from HUD properties.
- `AARHUDBase` is authoritative for local viewer context and now resolves against `ViewedEmotionTags`.
- Alien Ramen builds `ViewedEmotionTags` from the local player state's current character tag plus the possessed pawn `UParleySpeakerComponent` tag when present.
- Local HUD viewer tags are refreshed when the local controller changes pawn and when `AARPlayerStateBase::CurrentCharacterTag` changes.
- Global registrations are the fallback when the HUD has no matching targeted registrations.
- Runtime suppression (for example cutscenes) should use `AARHUDBase::SetEmotionRenderingSuppressed(...)`.
- `UEmoComponent` display change notifications still fire when effective display state changes, so delegate-driven UI/subsystems are informed without polling.

## Offer + Execution Rules (Current Runtime)

- Offer selection is server-side and bucketed:
  - unseen
  - game-seen/player-unseen catch-up
  - repeatable/other
- Highest numeric priority wins inside the first non-empty bucket.
- Equal-priority candidates resolve by weighted random using `OfferWeight` (default `1`).
- `ChanceOffered` (`0..1`) is rolled per candidate during offer evaluation.
- Failed chance rolls are only persisted to per-cycle skipped state on authoritative offer-attempt flow (`TryStartDialogueBetweenSpeakers` path), not on pure availability queries.
- Per-cycle blockers are character-specific (`seen this cycle`, `skipped this cycle`), controlled by `bBlockOfferPerCycle` (default `true`).
- Even when per-cycle blocking is disabled, seen/skipped-this-cycle and repeatable+completed-by-player candidates are de-prioritized to effective priority `1`.
- Offer checks include:
  - primary speaker exact match
  - per-speaker cycle policy (`FParleySpeakerRow::OfferCyclePolicy` + `OfferCycleLimitCount`, with `ProjectDefault` fallback to `UParleyDialogueSettings::DefaultSpeakerOfferCyclePolicy` / `DefaultSpeakerOfferCycleLimitCount`)
  - limited policy blocks new offers after cap
  - limited + repeat-last policy reoffers the per-speaker last offered conversation after cap
  - limited + repeatables-only policy filters post-cap candidates to `bRepeatable` conversations
  - manual override credits (`GrantManualOfferOverrideForSpeaker`) bypass only the per-speaker cycle-policy limit checks (other offer gates still apply)
  - forced speaker overrides (`OfferConversationTagNowForSpeaker`) bypass normal offer selection and force that conversation tag as the resolved offer when valid
- Repeatable conversations are not blocked by same-cycle seen/skipped gates; non-repeatables keep normal same-cycle suppression behavior.
- Scripted conversation starts through `StartConversationByTagForCharacters(...)` intentionally bypass standard offer/reoffer gate checks (character restriction, seen/completed suppression, per-cycle blocker, and per-cycle speaker offer-policy limits) while still enforcing authority/runtime validity and active-session ownership constraints.
- Starts that consume a forced speaker override also bypass standard offer/reoffer guards for that forced tag, then clear the forced override entry.
- optional conversation-level active-character restriction via `CharacterRestrictionTag`
  - relationship minimum
  - locked/blocked condition groups
  - seen/completed/repeatability suppression flags
- Runtime executes compiled nodes server-side with a step cap from settings.
- Implemented node execution: enter/completed/line/multiline/split-line/choice/bool/switch/route/route-by-character/tag-mutation/relationship-mutation/faction-mutation/signal/random/sequence.
- Line-node auto-advance is now a per-player runtime preference on `AARPlayerStateBase` (`SetDialogueAutoAdvanceEnabled`), not authored per line node.
  - that preference is persisted as player-owned save data
- Runtime line presentation now keeps authored text in standard Unreal rich-text format.
- Bracket command replacement runs before Unreal rich-text parsing.
  - Only the aliases listed below are treated as Parley commands; any other `[Something]` markup is left untouched for rich-text/decorator parsing or literal display.
- Important conversation and important choice flow force passive players into participants/eavesdrop set before interaction.
- Shop eavesdrop requests are immediate-only: `ForceEavesdrop` rejects enable requests when the target slot has no active dialogue session (no queued eavesdrop registration).
- Conversations can be authored as private (`FDialogueConversationHeader::bPrivateConversation`): active private sessions reject eavesdrop requests by default.
- Important choice flow overrides private-session eavesdrop lock while the choice is actively forcing all viewers.
- Optional busy-speaker lock (`UParleyDialogueSettings::bOnlyOneTalkerPerSpeakerInPerPlayerModes`) is mode-agnostic: when enabled, offers/starts for a speaker already owned by another active session are blocked, and optional auto-eavesdrop fallback can be enabled (`bAutoEavesdropOnBusySpeakerByDefault`).
- Busy query helpers are exposed for gameplay/UI traces: `UParleyDialogueSubsystem::IsSpeakerBusyForController(...)` and `AARNPCCharacterBase::IsSpeakerBusyForController(...)`.
- Busy-speaker presentation routes through generic emotion registrations (source `DialogueBusy`) using `UEmoSettings::BusyEmotionTag` and `BusyEmotionPriority`.
- Line nodes (including multiline entries) support the same `CharacterRestrictionTag` filter before skip-conditions are evaluated.
- Blocked-condition defaults now align to spec intent (`Any` by default on blocked groups); locked groups remain `All` by default.
- Logging: normal gating/selection outcomes are logged at `Verbose` level in `ARLog`; invalid graph/runtime corruption is logged as `Warning`/`Error` with conversation tag/session context for debugging.

## Rich Text + Bracket Aliases

### Supported `[]` aliases (complete list)

- Listener aliases:
  - `[Listener]`
  - `[Player]`
- Speaker aliases:
  - `[Speaker]`
  - `[Owner]`
  - `[NPC]`
- Last line speaker alias:
  - `[LastSpeaker]`
- Alias matching is case-insensitive (`[speaker]`, `[SpEaKeR]`, etc. all resolve).
- Resolution rules:
  - listener aliases resolve to the conversation starter (`SourceSpeakerTag`) speaker-row display label.
  - speaker aliases resolve to the conversation owner/primary speaker (`PrimarySpeakerTag`) speaker-row display label.
  - `[LastSpeaker]` resolves to the currently emitted line speaker tag for the active node (fallback: conversation owner/primary speaker).

### Unreal Rich Text references

- UMG Rich Text Block overview (Epic docs):
  - [Rich Text Block in UMG](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-rich-text-blocks-in-unreal-engine)
- `URichTextBlock` API reference:
  - [URichTextBlock API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/Components/URichTextBlock)
- Epic setup walkthrough (style rows/decorators/images):
  - [Advanced Text Styling with Rich Text Block](https://www.unrealengine.com/en-US/tech-blog/advanced-text-styling-with-rich-text-block)

### How to set up rich text for Parley UI

1. In your dialogue widget blueprint, use a `RichTextBlock` for line output (not a plain `TextBlock`).
2. Create/assign a Text Style data table to the widget’s style set (`Rich Text Style Row` table with at least a `Default` row).
3. Optional: add decorator classes (for example image decorator) and corresponding data tables for decorator data.
4. In your widget logic (`BP_OnDialogueViewUpdated` from `UParleyDialogueWidgetBase`), bind/set the line widget from `CurrentDialogueView.LineText`.
5. Keep authored line text in dialogue nodes as standard Unreal rich-text markup (`<StyleName>Text</>` etc.); Parley aliases are replaced first, then Unreal parses rich text.

### How to author it in the Parley Editor

1. Open the conversation in the Parley conversation editor.
2. Select a `Line`, `MultiLine`, or `SplitLine` node and edit its text field.
3. Author rich-text tags directly in the node text (for example `<Shout>...</>`), and use supported aliases where needed (for example `[Speaker]`, `[Listener]`, `[LastSpeaker]`).
4. Save/compile the conversation asset.
5. Preview in runtime/widget with a `RichTextBlock` configured as above; without a rich-text widget style setup, tags will show as plain text.

## Seen vs Completed

- Per-cycle offer blockers (`seen this cycle` / `skipped this cycle`) are persisted per character in save until explicitly cleared via `ClearConversationCycleOfferState(...)`.
- Per-cycle speaker offer counts are also persisted per character (`SpeakerOfferCountsThisCycle`) and drive per-speaker offer policy cap checks.
- Per-cycle per-speaker last-offered conversation tags are persisted (`LastOfferedConversationBySpeakerThisCycle`) for limited repeat-last behavior.
- Manual speaker override credits and forced speaker conversation offers are transient runtime state (not persisted to save).
- Runtime still keeps active-session transient containers for fast gating/evaluation.
- `ClearConversationCycleOfferState(...)` and authoritative offer-selection/start mutation paths are authority-gated; non-authority calls early-out.
- Save bridges can bind `UParleyDialogueSubsystem::OnProgressionStateMarkedDirty` to persist progression/cycle-state changes when dialogue marks data dirty.
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

Save schema is now `v20` with dialogue split by ownership:

- shared:
  - `DialogueSpeakerRelationshipStates` (directed `SourceSpeakerTag -> TargetSpeakerTag` matrix)
  - `DialogueCompletedConversationTagsByGame`
- character-owned:
  - `CharacterStates[].DialogueState`

Alien Ramen game-layer bridge mirrors Brother/Sister matrix edges so player-facing relationship values remain shared across player characters; Parley plugin core stays game-agnostic.

## Speaker Talkable Runtime

`UParleySpeakerSubsystem` no longer owns ramen/want mutation logic. It now:

- caches `SpeakerTag -> bTalkable`
- refreshes from `UParleyDialogueSubsystem::HasUnlockedDialogueForSpeakerForAnyPlayer(...)`
- enumerates refresh targets from dialogue runtime registered speaker tags (conversation primaries + speaker registry), not synthesized DataTable row-name tags
- broadcasts `OnSpeakerTalkableChanged`
- combines subsystem talkable state with `AARNPCCharacterBase::bSpeakerLocalStateAllowsDialogue` (local speaker runtime gate, for example ordering-mode lockouts)

Speaker actor integration now routes through `UParleySpeakerComponent`:

- `UParleySpeakerComponent` owns speaker-side dialogue interaction + replicated dialogue talkable mask/state.
- `AARNPCCharacterBase` remains the owner of non-dialogue local speaker gates (for example serving/customer mode) and combines that with component talkability for public speaker talk checks.
- effective talkable state drives a persistent emotion registration source `TalkableState` using `UEmoSettings::WantsToTalkEmotionTag` (set while talkable, cleared when not talkable).
- `AARNPCCharacterBase` bridges Parley speaker emotion requests into EMO registrations:
  - valid `ViewerCharacterTag` requests become targeted registrations for that exact tag
  - empty `ViewerCharacterTag` requests become global registrations
  - customer ordering and talkable-state presentation currently stay global

## Emotion Resolver Runtime

- `UEmoResolverSubsystem` caches emotion tag->icon mappings from TagKey route root `UEmoSettings::EmotionResolverRootTag` (default `Parley.Emotion`).
- Resolver cache invalidates/rebuilds when configured settings inputs change or when the bound emotion DataTable broadcasts `OnDataTableChanged`.
- Debug console commands:
  - `ar.emotion.log_cache_stats`
  - `ar.emotion.rebuild_cache`
- Optional diagnostics toggles in `UEmoSettings`:
  - `bEnableVerboseResolverLogs`
  - `bEnableVerboseRenderLogs`

## Editor Tooling (Current)

Registered editor tabs:

- `Parley Speaker Editor` (`SDialogueSpeakerEditorPanel`)
- `Parley Conversation Graph` (`SDialogueConversationGraphEditorPanel`)

Conversation graph tooling now provides:

- blueprint-style `SGraphEditor` canvas with right-click node creation
- right-click node creation actions are flat/top-level (no nested "Dialogue Nodes" submenu)
- graph node classes/schema (`UParleyDialogueEdGraph`, `UParleyDialogueEdGraphNode`, `UParleyDialogueEdGraphSchema`)
- conditional graph authoring uses editor-only `Branch` + `Check*` nodes (`CheckTags`, `CheckRelationship`, `CheckProgress`, `CheckLoadout`, `CheckCharacter`, `CheckVariable`) with dedicated bool wires; compile flattens them into existing runtime switch/condition-group data and emits no runtime nodes for the `Check*` sources
- `CheckRelationship` source options now include speaker relationship points/level (optional target speaker tag, defaulting to conversation primary speaker), faction popularity, and faction-speaker reputation (faction tag + optional speaker tag)
- `Relationship Mutation` authoring supports directed `SourceSpeakerTag -> TargetSpeakerTag` edits (source optional override, target optional fallback to conversation primary speaker)
- `Faction Mutation` inline authoring now supports both faction popularity delta and faction-speaker reputation delta (`FactionTag` + optional `TargetSpeakerTag`)
- when a speaker-target field uses `Parley.Speaker.Requester`/`Parley.Speaker.Owner`, runtime resolves it using the requester/owner `UParleySpeakerComponent` tags for relationship/faction condition and mutation evaluation
- character-owned progression resolution prioritizes live `PlayerState.CurrentCharacterTag` for the active slot/controller, with slot-mapped progression cache used only as fallback when live player state is unavailable
- active player identity resolves from the possessed pawn's `UParleySpeakerComponent` speaker tag first, then falls back to project bridge data when that component is unavailable
- player-speaker resolution normalizes gameplay character tags (`Shop.Character.Brother/Sister`) back to canonical dialogue speaker tags (`Parley.Speaker.Brother/Sister`) before character-restriction and requester-resolution checks
- player-speaker resolution prefers the currently possessed pawn's `UParleySpeakerComponent` speaker tag before falling back to mirrored player-state character tags, so swaps/possess flows immediately evaluate offers against the live pawn identity
- speaker talkable cache now refreshes when dialogue progression state mutates and when controllers possess/unpossess pawns, so relationship/tag/completion changes and pawn swaps immediately update NPC talkability
- `AARShopAIController` applies local shop dialogue gating from `State.ShopNPC.Dialogue` to NPCs that still own a customer component; pure dialogue/shop ambient NPCs without `UARCustomerComponent` stay interactable in shop flows
- AR player controllers/player states now expose `GetPlayerSlotTag()` so Emo can resolve viewer-specific P1/P2 dialogue overrides instead of falling back to shared-only display
- graph redraw/open is sourced from persisted `EditorGraph` authoring state (not reconstructed from `CompiledData`)
- editor-side validation fallback now uses a transient-package-owned `UParleyDialogueSubsystem` outside PIE, avoiding both rooted-object asserts and stale transient `GameInstance` PIE teardown asserts during graph compile/save
- signal nodes expose `SignalTag` + optional `PayloadTags`, render signal tag as inline subtitle, and compile as single-output passthrough nodes
- line nodes now render with inline authoring UI: speaker portrait button (left-click cycles base speakers from participants/graph usage, right-click opens emotion-tag picker under current speaker) + wrapped inline line-text edit + inline `Length Seconds` float edit; newly created line, multi-line, and split-line entries default authored length to `1.0`
- custom graph nodes and add-node context actions expose explicit hover tooltips (Blueprint-style)
- drag-link execution wiring with:
  - one outgoing link per output pin
  - multiple incoming links allowed per node input
- full toolbar flow: Save / Validate / Compile Runtime Graph / Focus Enter / Auto Layout / Preview
- details-panel editing for selected node or conversation root
- dynamic branch-pin behavior for choice/branch/switch/random/route-by-character nodes driven by stable branch GUIDs
- split-line node authoring uses multiline-style inline line rows, but runtime selects only the first row matching the active player character and otherwise skips to `Next`
- compile-from-editor-graph into `CompiledData` with node-level validation markers
- validation + preview execution through runtime dialogue subsystem even when PIE is not running
- no-PIE validation falls back to TagKey configured-route resolution when no live `GameInstance`/`UTagKeySubsystem` exists, so conversation graph validation still resolves authored speaker/conversation tables in editor-only flows
- no standalone in-tab global conversation list; graph tab edits a targeted conversation (speaker-hub handoff or explicit asset picker selection)
- preview trace output supports multi-step execution (line waits + auto-choice routing), plus preview-seen flags and typed injected variables
- speaker-tag editor fields are gameplay-tag-filtered to `Parley.Speaker.*` (header primary/participants, line speaker, relationship target, portrait-tag metadata surfaces)
- speaker rows include optional `LineFont` (`UFont` soft reference) for widget-level dialogue font styling; legacy style-tag wrapping remains a fallback path
- compile/create flow ensures `ParticipatingSpeakerTags` always includes the conversation primary speaker and the requester placeholder (`Parley.Speaker.Requester`); line-speaker edits also auto-add the selected base speaker so cycle convenience stays current during authoring
- Speaker details authoring categories for actor/talk/emotion properties use distinct roots (`Alien Ramen|Speaker`, `Alien Ramen|Talk`, `Alien Ramen|Emotion`) to avoid repeated same-name category buckets in Blueprint class-default details.

Speaker hub currently provides:

- TagKey-backed speaker table loading from `SpeakerDefinitionRootTag`
- searchable/sortable speaker list with columns (display name/tag/thresholds/conversation count)
- speaker CRUD (`New`, `Duplicate`, `Delete`) + `Save Speaker` + `Validate Speaker`
- speaker save enforces one-to-one speaker-tag ownership across the speaker table and auto-syncs row name to speaker-tag leaf (for example `Parley.Speaker.TestCactus` -> row `TestCactus`)
- `New` speaker rows are intentionally created without a speaker tag; authoring must assign an unused speaker tag before save.
- speaker-tag picker now emits an immediate warning in editor output when selecting a tag already assigned to another row; save remains blocked until tag is unique.
- reorderable threshold editing/reset (`5,15,30,50` defaults)
- inline portrait list with explicit `Add New` emotion button (tag + texture fields) and highlighted-entry removal
- relationship-level grouped conversation map for selected primary speaker with structured gate/mutation summaries and unlock-chain hints
- conversation cards are draggable between level headers to change minimum-relationship level assignment (replaces level cycle toggle)
- conversation map `Locked by` is inline editable as speaker-scoped gameplay-tag locks (`Parley.Conversations.<Speaker>.*`)
- conversation map `Required Tags` uses gameplay tag container editing (no CSV string entry)
- conversation map rows expose right-click context actions for `Open`, `Rename`, `Duplicate`, `Remove From Lookup`, and `Delete Asset + Remove From Lookup`
- speaker editor supports transaction-backed `Ctrl+Z` / `Ctrl+Y` (`Ctrl+Shift+Z`) undo/redo for conversation create/duplicate/delete flows
- stale lookup rows are cleaned when referenced conversation assets are deleted; removed conversation tags are stripped from lock/block condition references in remaining conversations
- generated conversation tag config cleanup is explicit via `Cleanup Tags` action (not implicit during delete), keeping undo/redo behavior predictable
- conversation create/open actions and broken-conversation scan using runtime validator
