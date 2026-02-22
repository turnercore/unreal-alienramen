# Alien Ramen Project Notes

## High-Level Architecture

- Core code module: `Source/AlienRamen`
- Editor tooling module: `Source/AlienRamenEditor`
- Gameplay/content is heavily Blueprint-driven in `Content/CodeAlong/Blueprints`
- GAS modules are enabled in `AlienRamen.Build.cs` (`GameplayAbilities`, `GameplayTags`, `GameplayTasks`)
- Default startup map/game framework comes from project config (`DefaultEngine.ini`) and points to Blueprint classes

## GAS Setup

### ASC Ownership Model

- `AARPlayerStateBase` owns the authoritative `UAbilitySystemComponent` and `UARAttributeSetCore`
- `AARPlayerStateBase` implements `IAbilitySystemInterface`
- Pawn (`AARShipCharacterBase`) uses PlayerState ASC as avatar binding target
- This supports persistence across pawn swaps (Lyra-style owner/avatar split)

### Pawn Initialization and Possession Flow

- On possession and replication, ship character initializes actor info with:
- Owner = `PlayerState`
- Avatar = `Pawn`
- Server possession flow applies loadout baseline in this order:
- clear previous grants/effects/loose tags
- grant common ability set from controller
- read `PlayerState.LoadoutTags`
- resolve ship/secondary/gadget rows via content lookup
- apply row baseline (effects/abilities/tags/weapon data)

### Loadout Tags and ASC Mirroring

- Canonical persistent source is `PlayerState.LoadoutTags`
- Runtime convenience mirror now copies all loadout tags into ASC loose tags at possession in Invader flow
- Mirrored tags are added to `AppliedLooseTags` for unified cleanup in `ClearAppliedLoadout`
- Cleanup removes both row-applied loose tags and mirrored loadout tags on unpossess/swap

### Ability Activation and Matching

- Pawn exposes generic activate/cancel-by-tag APIs
- Ability matching is deterministic:
- exact tag match scores higher than parent/hierarchy match
- tie-break uses ability level and stable order
- Dynamic grant-time tags (`DynamicSpecSourceTags`) are considered alongside ability asset tags

### Attributes

- `UARAttributeSetCore` is a shared central attribute set used for current player ship gameplay and planned enemy injection points
- Pre/post GE hooks clamp current/max and multiplier ranges
- Attributes replicate with notify handlers

### Cooldown MMC

- `UMMC_FireCooldownDuration` captures source `FireRate` and computes cooldown as `1 / FireRate`

## Tag-Driven Content Lookup

- `UContentLookupSubsystem` resolves gameplay tags to data table rows via `UContentLookupRegistry`
- Registry asset path is config-driven (`DA_ContentLookupReg`)
- System uses best root prefix match then leaf tag segment as row name
- Blueprint API cleanup direction:
- internal helper lookup functions should be non-BP-facing unless explicitly needed
- subsystem functions should appear under category path `Alien Ramen|Content Lookup`

## Other Custom C++ Systems

- `UGameplayTagUtilities`: hierarchy helpers and slot replacement (`ReplaceTagInSlot`)
- `USaveSlotSortLibrary`: custom thunk to sort struct arrays by `FDateTime`
- `UHelperLibrary`: reflection-based struct/object property copy by normalized names
- `UARWeaponDefinition`: weapon data asset (`ProjectileClass`, fire rate, damage GE, base damage)
- `AAREnemyBase`: C++ enemy base for Invader enemies with ASC/attribute injection foundation

## Logging Standard

- Global project logging category is `ARLog` (searchable, consistent)
- Use concise logs at key failure/decision points; avoid per-frame/per-field spam
- Keep severity meaningful (`Log`, `Warning`, `Error`) and include subsystem prefixes where useful
- Debug save tool prefixes:
- `[DebugSaveTool]`
- `[DebugSaveTool|IO]`
- `[DebugSaveTool|Validation]`

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
- Debug save tool only reads/writes debug slots and does not mutate production `SaveIndex`

## Debug Save Tool V1 Behavior

- Writes use raw variable reflection only
- V1 intentionally bypasses SG helper function flows (`Init`, `UpdateSavedGameState`, `UpdateSavedPlayerStates`)
- V1 intentionally does not replicate production backup behavior
- `SaveSlotNumber` is treated as a per-slot revision counter (not slot-list index)
- Physical save files use revision naming: `<BaseSlotName>__<SaveSlotNumber>` (example: `test_debug__3`)
- Legacy single-underscore revision names are still recognized for migration compatibility
- Index `SlotName` stores base slot name and `SlotNumber` stores latest revision for load/rollback behavior
- Saving increments revision and keeps trailing 5 revisions for that slot base
- Creating a slot fails if the base already exists (prevents accidental overwrite); use load + save to make next revision
- Targeted typed edits:
- Loadout tags (`PlayerStates[*].LoadoutTags`)
- Progression/dialogue tags (`Unlocks`, `Choices`, `SeenDialogue`, `DialogueFlags`)
- Currency/resources (`Money`, `Material`, `Meat.Amount`, `Cycles`)

## Debug Save Tool Implemented Surfaces

- Shared backend: `UARDebugSaveToolLibrary`
- Runtime frontend: `UARDebugSaveToolWidget` (minimal debug widget path)
- Editor frontend: `AlienRamenEditor` tab with details panel-based editing for loaded save object
- Editor menu entry added under `Window` as `Alien Ramen Debug Save Tool`
- Editor convenience action: `Unlock All (Current)` sets save `Unlocks` to all known `Unlocks.*` gameplay tags, then user presses `Save Current` to persist
- `Unlock All (Current)` uses leaf-only unlock tags (no parent nodes like `Unlocks.Ships`; only terminal tags like `Unlocks.Ships.Sammy`)

## Build and Integration Notes Learned

- `ARLog` must be exported for cross-module use: `ALIENRAMEN_API DECLARE_LOG_CATEGORY_EXTERN(...)`
- Primary module cpp include order must keep `AlienRamen.h` first in `AlienRamen.cpp`
- `USaveGame` usage in cpp files requires `#include "GameFramework/SaveGame.h"`
- `TStrongObjectPtr` assignment from raw pointer should use `Reset(...)`
- `UToolMenus::IsToolMenusAvailable()` is not available in current engine branch; use `UToolMenus::TryGet()` pattern
- Deprecated warning cleanup completed:
- `UGameplayAbility::AbilityTags` usage replaced with `GetAssetTags()`
- Removed deprecated `SListView::ItemHeight` usage in editor panel
