# Combat Lane Cleanup Asset Audit

This report captures binary content references discovered during the Secondary/Special combat-lane cleanup pass.

- Scope: `.uasset` / `.umap` scan only (no asset mutation).
- Query terms: `Unlock.Secondary`, `Input.Ability.FireSecondary`, `SecondaryDamage`, `SecondaryMaxAmmo`, `SpecialDamage`, `SpecialMaxAmmo`.
- Scan method: binary-safe text scan (`rg -a` + `strings` spot-check).

## Findings

### `.uasset` references

- `Content/CodeAlong/UI/HUD/Shop/ShopTerminals/UITerminal_Loadout.uasset`
  - `Unlock.Secondary`
  - `(TagName="Unlock.Secondary")`
- `Content/CodeAlong/Blueprints/GAS/Actions/GA_Revive.uasset`
  - `Input.Ability.FireSecondary`
- `Content/CodeAlong/Blueprints/GAS/Movement/GA_Dodge.uasset`
  - `Input.Ability.FireSecondary`

### `.umap` references

- No matching `.umap` references found for the query terms.

## Follow-Up (Editor Migration)

- Update `UITerminal_Loadout` to remove any loadout lane/options bound to `Unlock.Secondary`.
- Update `GA_Revive` and `GA_Dodge` to use supported input tags (`Input.Ability.FirePrimary` or `Input.Ability.HatActivate`, depending on intended behavior).
- Re-save migrated assets and re-run this scan to confirm no remaining references.
