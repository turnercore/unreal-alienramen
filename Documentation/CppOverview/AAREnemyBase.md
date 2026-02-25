# AAREnemyBase
Path: `Source/AlienRamen/Public/AREnemyBase.h`, `.../Private/AREnemyBase.cpp`

## Purpose
- Enemy actor (`ACharacter`) with actor-owned ASC.
- Holds runtime invader context (wave, formation slot, lock flags, formation target).
- Emits StateTree events via AI controller bridge.

## Automatic Lifecycle
- `BeginPlay`: binds health change delegate.
- `PossessedBy`: initializes ASC actor info, applies startup ability set (server), calls `BP_OnEnemyInitialized`.
- `OnHealthChanged`: calls `HandleDeath` when health <= 0 (server).
- `OnRep_IsDead`: calls `BP_OnEnemyDied(nullptr)` on clients.

## Blueprint Events
- `BP_OnEnemyInitialized` (Implementable)
- `BP_OnEnemyDied(AActor* InstigatorActor)` (Implementable)
- `BP_OnWavePhaseChanged(EARWavePhase NewPhase)` (Implementable)

## Blueprint-callable/Public Functions
- `GetASC()` (`BP Callable`)
- `GetCoreAttributes()` (`BP Callable`)
- `ActivateAbilityByTag(...)` (`BP Callable`, authority-only)
- `CancelAbilitiesByTag(...)` (`BP Callable`, authority-only)
- `HasASCGameplayTag(...)` (`BP Pure`)
- `HasAnyASCGameplayTags(...)` (`BP Pure`)
- `HandleDeath(...)` (`BP Callable`, native event, authority-only)
- `IsDead()` (`BP Pure`)
- `SetEnemyColor(...)` (`BP Callable`, authority-only)
- `SetWaveRuntimeContext(...)` (`BP Callable`, authority-only)
- `SetWavePhase(...)` (`BP Callable`, authority-only)
- `NotifyEnteredGameplayScreen(...)` (`BP Callable`, authority-only)
- `CanFireByWaveRules()` (`BP Pure`)
- `SetFormationLockRules(...)` (`BP Callable`, authority-only)
- `GetWaveInstanceId()` (`BP Pure`)
- `GetWavePhase()` (`BP Pure`)
- `GetFormationLockEnter()` (`BP Pure`)
- `GetFormationLockActive()` (`BP Pure`)
- `HasEnteredGameplayScreen()` (`BP Pure`)
- `HasReachedFormationSlot()` (`BP Pure`)
- `SetReachedFormationSlot(...)` (`BP Callable`, authority-only)
- `SetFormationTargetWorldLocation(...)` (`BP Callable`, authority-only)
- `GetFormationTargetWorldLocation()` (`BP Pure`)
- `HasFormationTargetWorldLocation()` (`BP Pure`)
- `CheckAndMarkLeaked(...)` (`BP Callable`)
- `IsLeaked()` (`BP Pure`)
- `HasBeenCountedAsLeak()` (C++)
- `MarkCountedAsLeak()` (C++)

## Key Variables (current)
- GAS:
- `AbilitySystemComponent` (`UPROPERTY`, BP read-only)
- `StateTreeASC` (`UPROPERTY`, BP read-only alias for StateTree binding)
- `AttributeSetCore` (`UPROPERTY`, BP read-only)
- `StartupAbilitySet` (`UPROPERTY`, BP read-only)
- Gameplay:
- `EnemyColor` (replicated)
- `EnemyArchetypeTag` (replicated)
- Death:
- `bIsDead` (replicated with `OnRep_IsDead`)
- Invader context (replicated via `OnRep_WaveRuntimeContext`):
- `WaveInstanceId`
- `FormationSlotIndex`
- `WavePhase`
- `WavePhaseStartServerTime`
- `bFormationLockEnter`
- `bFormationLockActive`
- `FormationTargetWorldLocation`
- `bHasFormationTargetWorldLocation`
- Internal runtime flags (not BP-exposed): leak/entry/dispatch guards.

## Notes
- `HandleDeath` marks dead and stops AI/abilities; caller decides whether to `Destroy()`.
- Leak counting in director is deduped; enemy leak logic can be BP-owned.

