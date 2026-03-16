# Faction Runtime Guide (`UParleyFactionSubsystem`)

This document describes the current faction runtime split:

- `Parley` owns generic faction data/state APIs.
- AR game layer owns election/voting orchestration.

## Ownership Split

- **Parley-owned (`UParleyFactionSubsystem`)**
  - Faction definition resolution from TagKey
  - Faction popularity state
  - Faction reputation per speaker (`FactionTag + SpeakerTag`)
  - Mutation events for save bridges
  - Save-state injection APIs (load-time hydrate)

- **Game-owned (AR layer)**
  - Voting candidates
  - Vote submission and winner policy
  - Election-specific effects and game outcomes
  - Any vote-specific DataTable that layers on top of Parley faction definitions

## AR Voting Subsystem

Primary subsystem:
- `UARFactionVotingSubsystem` (`Source/AlienRamen/Public/ARFactionVotingSubsystem.h`)

Settings:
- `UARFactionVotingSettings` (`Project Settings -> Alien Ramen -> Faction Voting`)
- required DataTable row type: `FARFactionVotingDefinitionRow`

Runtime behavior:
- Candidate pool resolves from AR-owned voting DataTable only; no runtime fallback candidate source.
- Candidate count scales with save-owned `FactionClout` using settings:
  - `MinCandidateCount`
  - `MaxCandidateCount`
  - `CloutPerAdditionalCandidate`
- Votes are submitted by runtime controller slot id (`PlayerSlotId` on `AARPlayerStateBase`), with legacy slot-tag wrappers kept only for compatibility.
- Winner resolution order:
  1. vote count
  2. effective popularity from Parley
  3. candidate priority
  4. faction-tag lexical fallback
- Winner applies to game-owned state:
  - `AARGameStateBase::ActiveFactionTag`
  - `AARGameStateBase::ActiveFactionEffectTags`
  - current save payload mirror + `MarkSaveDirty()`

Transition timing:
- Election finalization is owned by transition flow, not Parley and not shop fallback.
- `AARTransitionGameMode` finalizes once when transition context is `Source=Shop` and `Reason=ShopToInvader`.

## Parley API Surface

Primary subsystem:
- `UParleyFactionSubsystem` (`Plugins/Parley/Source/Parley/Public/ParleyFactionSubsystem.h`)

Definition/data access:
- `GetFactionDefinition(FactionTag, OutDefinition)`
- `GetAllFactionTags(OutFactionTags)`

Popularity:
- `GetFactionPopularity(FactionTag)`
- `GetEffectiveFactionPopularity(FactionTag)` (includes injected progression-rule modifiers)
- `ModifyFactionPopularity(FactionTag, DeltaPopularity)`
- `SetFactionPopularityStates(States)` (save bridge injection)

Speaker reputation:
- `GetFactionSpeakerReputation(FactionTag, SpeakerTag)`
- `ModifyFactionSpeakerReputation(FactionTag, SpeakerTag, DeltaReputation)`
- `SetFactionSpeakerReputationStates(States)` (save bridge injection)

Events:
- `OnFactionPopularityChanged(FactionTag, Delta, NewTotal)`
- `OnFactionSpeakerReputationChanged(FactionTag, SpeakerTag, Delta, NewTotal)`

## Save Bridge Contract

`UARParleySaveBridge` subscribes to Parley faction events and persists:

- `UARSaveGame::FactionPopularityStates`
- `UARSaveGame::FactionSpeakerReputationStates`

On save load, bridge injects:

- `SetFactionPopularityStates(...)`
- `SetFactionSpeakerReputationStates(...)`
- `SetProgressionTags(...)` (for effective-score modifier evaluation)

Policy:
- event handlers **mark save dirty only**
- no forced autosave from Parley faction mutations

## Definition Row Contract

`FARFactionDefinitionRow` remains the canonical faction definition row:

- display fields (`DisplayName`, `Description`, `IconTexture`, `PosterTexture`)
- popularity bounds/base
- optional drift/effect fields for game-owned voting layers
- optional progression-based popularity modifier rules

## Notes

- `AARShopGameMode` no longer finalizes faction election through Parley.
- Voting/election runtime should be implemented entirely in AR on top of Parley faction state.
- Blueprint-exposed voting + Parley faction surfaces should include `ToolTip` metadata for editor discoverability.
