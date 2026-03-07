/**
 * @file ARInvaderDirectorSettings.h
 * @brief ARInvaderDirectorSettings header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ARInvaderDropTypes.h"
#include "ARInvaderDirectorSettings.generated.h"

class AActor;
class UARAbilitySet;
class UCurveFloat;
class AARInvaderDropBase;

USTRUCT(BlueprintType)
struct FAREnemyArchetypeAbilitySetEntry
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy|Abilities")
	FGameplayTag EnemyArchetypeTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy|Abilities")
	TSoftObjectPtr<UARAbilitySet> AbilitySet;
};

USTRUCT(BlueprintType)
struct FARInvaderDropStackDefinition
{
	GENERATED_BODY()

	// Currency value represented by one pickup spawn for this entry.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta = (ClampMin = "1"))
	int32 Denomination = 1;

	// Pickup class spawned for this denomination.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TSoftClassPtr<AARInvaderDropBase> DropClass;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Invader Director"))
class ALIENRAMEN_API UARInvaderDirectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTag WaveDefinitionRootTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTag StageDefinitionRootTag;

	// Applied to all enemies before archetype/specific startup abilities.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy|Abilities")
	TSoftObjectPtr<UARAbilitySet> EnemyCommonAbilitySet;

	// Optional archetype-specific startup ability sets. Exact archetype tag match preferred.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy|Abilities")
	TArray<FAREnemyArchetypeAbilitySetEntry> EnemyArchetypeAbilitySets;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName InitialStageRow;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	float BaseThreatGainPerSecond = 1.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	float NewWaveDelayAfterClear = 1.5f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	float NewWaveDelayWhenOvertime = 3.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	float StageTransitionDelay = 0.75f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	float StageChoiceAutoSelectSeconds = 0.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	bool bStageChoiceAutoSelectLeft = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FVector SpawnOrigin = FVector::ZeroVector;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnOffscreenDistance = 350.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnLaneSpacing = 220.f;

	// Additional yaw correction applied after look-at spawn facing.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnFacingYawOffset = 0.f;

	// World-space gameplay bounds. Note that X is intentionally asymmetric:
	// the player side is at the low-X boundary (X = 0), and positive X extends
	// into the playfield. This matches the coordinate convention documented in
	// Agents.md (see line 126) and replaces the older symmetric bounds.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	FVector2D GameplayBoundsMin = FVector2D(0.f, -1350.f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	FVector2D GameplayBoundsMax = FVector2D(1400.f, 1550.f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	float OffscreenCullSeconds = 12.f;

	// Seconds projectiles can remain outside gameplay bounds before auto-release.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds|Projectiles", meta=(ClampMin="0.0"))
	float ProjectileOffscreenCullSeconds = 0.1f;

	// Seconds pickups can remain outside gameplay bounds before auto-release.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds|Pickups", meta=(ClampMin="0.0"))
	float PickupOffscreenCullSeconds = 0.1f;

	// Baseline chance applied to enemy DropChance attribute at runtime.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultEnemyDropChance = 0.5f;

	// Baseline chance applied to enemy DropChance when DropType is Scrap.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultEnemyScrapDropChance = 0.5f;

	// Baseline chance applied to enemy DropChance when DropType is Meat.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultEnemyMeatDropChance = 0.2f;

	// Max fractional deviation around authored drop amount (0.25 => +/-25%).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropAmountVarianceFraction = 0.25f;

	// Max fractional deviation for scrap drop amount.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ScrapDropAmountVarianceFraction = 0.25f;

	// Max fractional deviation for meat drop amount.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MeatDropAmountVarianceFraction = 0.25f;

	// Optional curve sampled with random [0..1], expected output in [-1..1] to bias drop amount roll around center.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TSoftObjectPtr<UCurveFloat> DropAmountVarianceCurve;

	// Optional variance curve for scrap amount rolls. Falls back to DropAmountVarianceCurve when unset.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TSoftObjectPtr<UCurveFloat> ScrapDropAmountVarianceCurve;

	// Optional variance curve for meat amount rolls. Falls back to DropAmountVarianceCurve when unset.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TSoftObjectPtr<UCurveFloat> MeatDropAmountVarianceCurve;

	// Initial random XY linear speed injected into spawned drop physics.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0"))
	float DropInitialLinearSpeedMin = 120.0f;

	// Initial random XY linear speed injected into spawned drop physics.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops", meta=(ClampMin="0.0"))
	float DropInitialLinearSpeedMax = 220.0f;

	// Optional denomination definitions for scrap drops. Runtime decomposes the final
	// drop amount into an optimal pickup set (fewest actors, then larger denominations).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TArray<FARInvaderDropStackDefinition> ScrapDropStacks;

	// Optional denomination definitions for meat drops. Runtime decomposes the final
	// drop amount into an optimal pickup set (fewest actors, then larger denominations).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops")
	TArray<FARInvaderDropStackDefinition> MeatDropStacks;

	// Controls whether drifting drops collide with pawns (enemy bumping) or ignore all pawns.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops|Physics")
	EARInvaderDropPawnCollisionMode DropPawnCollisionMode = EARInvaderDropPawnCollisionMode::CollideWithPawns;

	// Invader "up" direction used to orient custom drop gravity frame.
	// Earth gravity direction resolves to `-Normalized(InvaderDesiredUpDirection)`.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops|Physics")
	FVector InvaderDesiredUpDirection = FVector(1.0f, 0.0f, 0.0f);

	// Acceleration magnitude used when debug-enabling drop earth gravity.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops|Physics", meta=(ClampMin="0.0"))
	float DropEarthGravityAcceleration = 980.0f;

	// If true, runtime derives pickup radius from pawn capsule as:
	// Radius + Diameter, which is roughly one character-length outside the ship on all sides.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops|Pickup", meta=(InlineEditConditionToggle))
	bool bUseCapsuleDerivedPlayerPickupRadius = true;

	// Default player pickup radius written to ASC PickupRadius at spawn/init when capsule-derived sizing is disabled.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Drops|Pickup", meta=(ClampMin="0.0", EditCondition="!bUseCapsuleDerivedPlayerPickupRadius"))
	float DefaultPlayerPickupRadius = 150.0f;

	// Screen-entry detection inset from gameplay bounds used for first-visibility events.
	// Positive values require enemies to be farther inside bounds before "entered screen" is considered true.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	float EnteredScreenInset = 40.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps")
	int32 SoftCapAliveEnemies = 120;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps")
	int32 SoftCapActiveProjectiles = 500;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps")
	bool bBlockSpawnsWhenEnemySoftCapExceeded = false;

	// Number of eligible wave definitions to peek ahead when preloading enemy classes.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps", meta=(ClampMin="0", UIMin="0"))
	int32 EnemyPreloadWaveLookahead = 2;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Telemetry")
	TSoftClassPtr<AActor> ProjectileActorClass;
};
