# Faction Subsystem Guide (`UARFactionSubsystem`)

This document describes the current server-authoritative faction election flow and its integration with save state and mode travel.

## Runtime Ownership

- Primary runtime API: `UARFactionSubsystem` (`Source/AlienRamen/Public/ARFactionSubsystem.h`)
- Settings source: `UARFactionSettings` (`Project Settings -> Alien Ramen -> Alien Ramen Factions`)
- Content source: `UContentLookupSubsystem` root `Faction.Definition`
- Persistence source: `UARSaveSubsystem` / `UARSaveGame`

## Core API Surface

- `RefreshElectionSnapshot()`
- `GetCurrentCandidates()`
- `SubmitVote(PlayerSlot, SelectedFactionTag)`
- `ClearVotes()`
- `FinalizeElectionForTravel(OutWinnerFactionTag, OutReason)`

Events:
- `OnFactionCandidatesRefreshed`
- `OnFactionElectionFinalized`

## Election Snapshot Model

`RefreshElectionSnapshot()` (authority-only) rebuilds:
- ranked faction list
- candidate list
- transient vote state reset

Ranking inputs per faction row:
- persisted popularity (or row `BasePopularity` when not present)
- per-election random drift (`DriftPerCycleMin..DriftPerCycleMax`)
- progression modifiers (`PopularityModifierRules` against save `ProgressionTags`)

Popularity is clamped to row min/max bounds before ranking.

## Clout + Candidates

- Candidate count is clamped from `FactionClout`:
  - `CandidateCount = clamp(FactionClout, 0, RankedFactionCount)`
- If clout is `0` at finalize time:
  - winner is cleared
  - reason is `DisabledByClout`
  - active faction/effects are removed from save/runtime state

## Vote Resolution Policy

Finalization winner resolution order:
1. Both players voted same faction -> `SamePick`
2. Both players voted different factions -> random tie-break (`DivergedRandom`)
3. Only one player voted -> `SinglePick`
4. No votes -> top-ranked faction (`NoVotesTopPopularity`)

If no valid faction definitions resolve, finalization falls back to `NoValidFactions`.

## Apply + Persist Side Effects

On successful finalize:
- Saves popularity snapshot to `SaveGame.FactionPopularityStates`
- Writes elected state to save:
  - `ActiveFactionTag`
  - `ActiveFactionEffectTags`
- Applies elected state to replicated runtime GameState:
  - `SetActiveFactionTagFromSave(...)`
  - `SetActiveFactionEffectTagsFromSave(...)`
- Marks save dirty
- Broadcasts `OnFactionElectionFinalized`

## Mode Integration

- `AARShopGameMode::PreStartTravel(...)` calls `FinalizeElectionForTravel(...)`.
- Travel is blocked if faction finalization fails.
- This ensures the next mode sees finalized elected faction state before transition.

## Data Contracts

Faction definition row type: `FARFactionDefinitionRow`
- canonical `FactionTag`
- `BasePopularity`, min/max bounds
- drift range
- elected `EffectTags`
- progression-based modifier rules

Runtime state types:
- `FARFactionRuntimeState` (tag + popularity)
- `FARFactionVoteSelection` (slot + selected faction)
- `EARFactionWinnerReason`

## Troubleshooting

- Snapshot refresh fails:
  - verify authority context
  - verify `FactionDefinitionRootTag` in project settings
  - verify content lookup registry route/table shape
- Finalize blocked in Shop travel:
  - check `UARFactionSubsystem` exists
  - inspect content lookup resolution warnings for faction rows
- Unexpected candidate list size:
  - verify `FactionClout` in current save and progression modifiers
