# Alien Ramen Project Notes

## Logging

- Use `ARLog` for code logs of important places where the code is doing something non-trivial or important to track. This is not to spam the logs, but to see if something is erroring or warning that is easily searchable for debugging.

## Save System Contract (Debug Save Tool V1)

### Authoritative Save Assets (runtime target)

- `/Game/CodeAlong/Blueprints/SaveSystem/SG_SaveIndex`
- `/Game/CodeAlong/Blueprints/SaveSystem/SG_AlienRamenSave`

### SG_SaveIndex

- Variable: `SlotNames` (Array of `ST_SaveSlotInfo`)

### ST_SaveSlotInfo

- `SlotName` : Name
- `SlotNumber` : Integer
- `SaveVersion` : Integer
- `CyclesPlayed` : Integer
- `LastSavedTime` : DateTime
- `Money` : Integer

### SG_AlienRamenSave (tracked vars)

- `SeenDialogue` : GameplayTagContainer
- `DialogueFlags` : GameplayTagContainer
- `Unlocks` : GameplayTagContainer
- `Choices` : GameplayTagContainer
- `Money` : Integer
- `Meat` : `ST_Meat`
- `Material` : Integer
- `Cycles` : Integer
- `SaveSlot` : Name
- `SaveGameVersion` : Integer
- `SaveSlotNumber` : Integer
- `LastSaved` : DateTime
- `PlayerStates` : Array of `ST_PlayerStateData`

### ST_Meat

- `Color` : `E Enemy Color`
- `Amount` : Integer

### ST_PlayerStateData

- `Id` : Integer
- `hasBeenSaved` : Boolean
- `DisplayName` : Text
- `PlayerSlot` : `E Player Slot`
- `CharacterPicked` : `E Character Choices`
- `LoadoutTags` : GameplayTagContainer

## Debug Save Namespace Policy

- Debug index slot name: `SaveIndexDebug`
- Debug save slot suffix: `_debug`
- Debug save tool must only read/write debug slots and must not mutate production `SaveIndex` entries.

## Debug Save Tool V1 Behavior

- Writes use raw variable reflection only.
- V1 intentionally bypasses save helper functions and production backup replication logic.
- Targeted typed edits in V1:
    - Loadout tags (`PlayerStates[*].LoadoutTags`)
    - Progression tags (`Unlocks`, `Choices`, and dialogue tag containers)
    - Currency/resources (`Money`, `Material`, `Meat.Amount`, `Cycles`)

## Logging Convention

- Use `ARLog` for tool logs with prefixes:
    - `[DebugSaveTool]`
    - `[DebugSaveTool|IO]`
    - `[DebugSaveTool|Validation]`
