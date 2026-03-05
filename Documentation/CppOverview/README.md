# Alien Ramen C++ Overview

This folder is a quick API inventory for Invader runtime/authoring C++ types. Each page maps to an in-engine class and notes Blueprint exposure, ownership, and lifecycle expectations.

## How to read

- Look up a class page to see entry points, replication/authority rules, and BP exposure.
- Pair with [Doxygen](../doxygen/html/index.html) for signatures and member-level details.
- If you edit a class, update its page to keep contracts accurate.

## Index

- [AAREnemyBase](AAREnemyBase.md) — enemy ASC ownership, damage routing, death lifecycle
- [AAREnemyAIController](AAREnemyAIController.md) — invader enemy AI control layer
- [UARInvaderDirectorSubsystem](UARInvaderDirectorSubsystem.md) — wave/stage orchestration
- [UARInvaderRuntimeStateComponent](UARInvaderRuntimeStateComponent.md) — runtime state snapshot
- [UARInvaderDirectorSettings](UARInvaderDirectorSettings.md) — tunable director settings
- [InvaderDataTypes](InvaderDataTypes.md) — core structs/enums for invader flow
- [EnemyUtilities](EnemyUtilities.md) — helper utilities for enemies
- [Editor_SInvaderAuthoringPanel](Editor_SInvaderAuthoringPanel.md) — editor authoring UI
- [Editor_Settings](Editor_Settings.md) — editor-side settings
