# Enemy Utilities and Schema

## UAREnemyFacingLibrary
Path: `Source/AlienRamen/Public/AREnemyFacingLibrary.h`

Purpose:
- Utility to snap enemy facing back to straight-down gameplay direction.

Function:
- `ReorientEnemyFacingDown(AActor* EnemyActor, bool bUseDirectorSettingsOffset=true, float AdditionalYawOffset=0.f, bool bZeroPitchAndRoll=true)` (`BP Callable`)

---

## UAREnemyStateTreeSchema
Path: `Source/AlienRamen/Public/AREnemyStateTreeSchema.h`

Purpose:
- Custom StateTree AI component schema defaults for enemy authoring.

Defaults:
- AI controller class: `AAREnemyAIController`
- Context actor class: `AAREnemyBase`

Lifecycle:
- `PostLoad()`
- `PostEditChangeChainProperty(...)` (editor)
- internal `SyncContextDescriptorTypes()`

