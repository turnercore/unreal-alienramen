# Shared Systems Guide

These pages cover the systems that support more than one game mode.

Use this section when the question is not "what happens in Shop/Invader/Scrapyard?" but instead "which shared runtime owns this behavior?"

## Start Here By Concern

| Concern | Start here | Common mode touchpoints |
| --- | --- | --- |
| listen server flow, LAN/online sessions, couch co-op seat handling | [Networking Overview](README_Networking.md) | Shop, Invader, Scrapyard |
| save ownership, hydration, load-entry rules, travel overlays | [Persistence Overview](README_Persistence.md) | Shop, Invader, Scrapyard, Transition |
| loadouts, attributes, ASC ownership, player ability surfaces | [GAS Overview](README_GAS.md) | mostly Invader, plus Shop/Scrapyard integration points |
| Blueprint UI reads for replicated attributes | [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md) | Invader HUD, Scrapyard ship/UI |
| dialogue, speakers, emotion, speaker talkability | [Parley Runtime](README_DialogueNPC.md) | Shop and other NPC-facing flows |
| content lookup by gameplay tag | [TagKey](README_Plugins_TagKey.md) | Shop, Dialogue, Scrapyard, Invader |
| faction voting and election runtime | [Faction Subsystem](README_FactionSubsystem.md) | Shop / dialogue-adjacent flows |

## Networking and Sessions

- [Networking Overview](README_Networking.md)
- [Online Session Subsystem](README_SessionSubsystem.md)

Read this when a feature needs to survive listen-server play, couch co-op, LAN, or future online backend routing.

## Controller Identity and Character Assignment UI

- Runtime controller identity is `AARPlayerStateBase::PlayerSlotId` (controller/profile-owned, not character-owned).
- Canonical gameplay ownership remains character-tag-based (`CurrentCharacterTag`).
- Shared Blueprint widget bridge for lobby/pause character assignment:
  - `Source/AlienRamen/Public/ARCharacterAssignmentWidgetBase.h`
  - `Source/AlienRamen/Private/ARCharacterAssignmentWidgetBase.cpp`
- Concrete Widget Blueprint parent for UMG assets:
  - `Source/AlienRamen/Public/ARLobbyCharacterAssignmentWidget.h`
  - `Source/AlienRamen/Private/ARLobbyCharacterAssignmentWidget.cpp`
- `UARCharacterAssignmentWidgetBase` publishes controller-id -> character-tag snapshots, supports deferred selection + confirm flows, and emits `OnAllTrackedControllersReadyChanged` for menu owners to close/unpause/continue.
- Create lobby Widget Blueprints from `UARLobbyCharacterAssignmentWidget`; keep `UARCharacterAssignmentWidgetBase` as the reusable abstract runtime bridge.

## Save, Travel, and Progression

- [Persistence Overview](README_Persistence.md)
- [Saving, Loading, and Hydration](README_SaveSubsystem.md)
- [Progression and Unlocks](README_ProgressionUnlocks.md)
- [Transition Flow](README_TransitionMode.md)

Read this when data needs to persist across maps, survive mode handoffs, or project back onto `GameState` / `PlayerState`.

## Plugins and Shared Runtime

- [Plugins Overview](README_Plugins.md)
- [GAS Overview](README_GAS.md)
- [GAS Blueprint Attributes](README_GAS_Blueprint_Attributes.md)
- [TagKey](README_Plugins_TagKey.md)
- [Emo](README_Plugins_Emo.md)

This is the shared runtime foundation section. GAS lives here because it is a cross-mode system, even though Invader is its heaviest gameplay consumer.

## Debug Commands

- `ar.debug.log` controls `ARLog`.
- `ar.debug.log.all` controls `ARLog`, `EmoLog`, `ParleyLog`, and `TagKeyLog` together.
- `emo.debug.log`, `parley.debug.log`, and `tagkey.debug.log` control their plugin categories independently.
- Plugin-specific debug commands require an active PIE/Game world context to apply verbosity at runtime.

## Dialogue, Speakers, and Factions

- [Parley Runtime](README_DialogueNPC.md)
- [Parley Plugin Ownership Boundary](README_DialoguePluginBoundary.md)
- [Faction Subsystem](README_FactionSubsystem.md)

Read this when a feature touches NPC interaction, speaker talkability, relationship state, or dialogue-owned mutation surfaces.

## Deep C++ Surfaces

- [Invader C++ Overview](CppOverview/README.md)
- [API Reference (Doxygen)](/unreal-alienramen/doxygen/index.html)

Use these after you know which system owns the behavior and need class-level entry points.
