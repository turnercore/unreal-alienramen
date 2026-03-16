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

## Where to read details

- [Parley runtime](README_DialogueNPC.md)
- [Shop runtime](README_ShopRamenSystem.md)
- [Faction subsystem](README_FactionSubsystem.md)
- [Scrapyard mode](README_ScrapyardMode.md)
