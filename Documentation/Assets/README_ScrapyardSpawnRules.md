# Scrapyard Spawn Rule Asset

Author a `UARScrapyardSpawnRuleSet` per map to drive GameMode-managed scrapyard spawns.

## Fields (`FARScrapyardSpawnRules`)

- `MinTotalSpawns` / `MaxTotalSpawns` — total items spawned after always-spawn entries.
- `NoiseScale` — world-units frequency for Perlin noise sampling (larger = broader clusters).
- `NoiseThreshold` — minimum noise score (0–1) required for a spawner to be considered (except `bAlwaysSpawn`).
- `NoiseJitter` — random jitter per spawner to avoid grid artifacts.
- `RarityBudgets` — map of `EARScrapyardItemRarity` ➜ `{MinCount, MaxCount}`. Missing entries default to `{0,0}`.

## Defaults (constructor)

- Common: 3–12
- Uncommon: 2–8
- Rare: 1–5
- Epic: 0–3
- Legendary: 0–1
- MinTotalSpawns: 8, MaxTotalSpawns: 20, NoiseScale: 600, NoiseThreshold: 0.2, NoiseJitter: 0.1

## Authoring notes

- Place the asset in your map’s folder (e.g., `Content/Maps/Scrapyard/DA_SpawnRules_Lvl_Scrapyard`).
- Set `SpawnRuleSet` on the `ARScrapyardGameMode` instance/Blueprint for the map.
- When using managed spawns, set each `AARScrapyardItemSpawner` `bSpawnOnBeginPlay` to **false** to avoid double spawns.
- Use `bAlwaysSpawn` for guaranteed hero props; adjust `SpawnerWeight` to bias noise-weighted selection.
