# TagKey (Tag Content Resolver)

`TagKey` in this project maps to gameplay-tag-driven content lookup using `UTagKeySubsystem`.

## What it does

- Resolves gameplay tags to authored rows/assets.
- Keeps plugin and gameplay systems data-driven without hard-coded table references.
- Provides shared routes consumed by Shop, Parley, Faction, Scrapyard, and Invader systems.
- Supports preload workflows:
  - Route table preloads for startup policies.
  - `PreloadRootTableAndSoftReferences` for a root table plus discovered soft object and soft class references.
  - Optional recursive walk of referenced DataTables (depth-limited) without synchronously loading every non-table soft reference.

## Runtime constraints

- Resolver APIs are game-thread-only; static configured-route helpers enforce the same thread requirement as subsystem instance methods.
- Root-tag hierarchy overlap is allowed intentionally; tag resolution walks parent tags from leaf to root and uses the nearest configured ancestor root.

## Debugging

- `tagkey.debug.log` defaults `TagKeyLog` to `VeryVerbose`.
- `tagkey.debug.log verbose|log|warning|error|off|reset` supports the same verbosity shorthands as `ar.debug.log`.
- `tagkey.debug.log` requires an active PIE/Game world context; it will no-op with a warning from editor-only/non-game contexts.
- Native console command `log logtagkey <level>` still works for direct log-category control.

## Where to read details

- [Parley runtime](README_DialogueNPC.md)
- [Shop runtime](README_ShopRamenSystem.md)
- [Faction subsystem](README_FactionSubsystem.md)
- [Scrapyard mode](README_ScrapyardMode.md)
