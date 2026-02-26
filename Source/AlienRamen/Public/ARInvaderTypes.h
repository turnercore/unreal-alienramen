#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPtr.h"
#include "ARAbilitySet.h"
#include "ARInvaderTypes.generated.h"

class AAREnemyBase;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EAREnemyColor : uint8
{
	Red = 0,
	White = 1,
	Blue = 2,
};

UENUM(BlueprintType)
enum class EARWavePhase : uint8
{
	Active = 0,
	Berserk = 1
};

UENUM(BlueprintType)
enum class EARInvaderFlowState : uint8
{
	Combat = 0,
	AwaitStageClear = 1,
	StageChoice = 2,
	Transition = 3,
	Stopped = 4
};

UENUM(BlueprintType)
enum class EARSpawnEdge : uint8
{
	Top = 0,
	Left = 1,
	Right = 2
};

USTRUCT(BlueprintType)
struct FARWaveEnemySpawnDef
{
	GENERATED_BODY()

	// Enemy identity is authored by gameplay tag and resolved through the enemy definition table.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FGameplayTag EnemyIdentifierTag;

	// Legacy path retained for migration support only; runtime spawn identity should use EnemyIdentifierTag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta=(DeprecatedProperty, DeprecationMessage="Use EnemyIdentifierTag. EnemyClass is legacy migration-only."))
	TSubclassOf<AAREnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	EAREnemyColor EnemyColor = EAREnemyColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	EARSpawnEdge SpawnEdge = EARSpawnEdge::Top;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float SpawnDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FVector2D AuthoredScreenOffset = FVector2D::ZeroVector;

	// Effects applied to this spawned enemy instance (in addition to wave/stage effects).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TArray<TSubclassOf<UGameplayEffect>> EnemyGameplayEffects;
};

USTRUCT(BlueprintType)
struct FARInvaderEnemyRuntimeInitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	float Damage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	float CollisionDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	float MoveSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	float FireRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	float DamageTakenMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	FGameplayTag EnemyArchetypeTag;

	// Per-enemy startup abilities applied after common/archetype sets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	TArray<FARAbilitySet_AbilityEntry> EnemySpecificAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	TArray<TSubclassOf<UGameplayEffect>> StartupGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Runtime")
	FGameplayTagContainer StartupLooseTags;
};

USTRUCT(BlueprintType)
struct FARInvaderEnemyDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TSoftClassPtr<AAREnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FGameplayTag EnemyIdentifierTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FARInvaderEnemyRuntimeInitData RuntimeInit;
};

USTRUCT(BlueprintType)
struct FARBerserkProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float MoveSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float FireRateMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FGameplayTagContainer BehaviorTags;
};

USTRUCT(BlueprintType)
struct FARWaveDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float MinThreat = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float MaxThreat = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	int32 SelectionWeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	int32 MinPlayers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	int32 MaxPlayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bOneTimeOnlyPerRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bAllowColorSwap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float RepeatWeightPenalty = 0.1f;

	// If true, enemies stay formation-locked until they report formation-slot arrival.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bFormationLockEnter = false;

	// If true, AI should treat enemies in this wave as formation-locked during Active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bFormationLockActive = false;

	// If true, runtime may mirror authored spawn offsets across gameplay-bounds center on X.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bAllowFlipX = false;

	// If true, runtime may mirror authored spawn offsets across gameplay-bounds center on Y.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bAllowFlipY = false;

	// Entering primarily ends by runtime condition checks; this is timeout fallback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float EnterDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta=(DisplayName="Wave Duration"))
	float WaveDuration = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FARBerserkProfile BerserkProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FARWaveEnemySpawnDef> EnemySpawns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FGameplayTagContainer WaveTags;

	// Effects applied to all enemies spawned by this wave.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<TSubclassOf<UGameplayEffect>> EnemyGameplayEffects;
};

USTRUCT(BlueprintType)
struct FARStageDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	float SelectionWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	float StageDuration = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	float ThreatGainMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	float EnemyHealthMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTagContainer RequiredWaveTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTagContainer BlockedWaveTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTagContainer BannedArchetypeTags;

	// Effects applied to all enemies spawned while this stage is active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	TArray<TSubclassOf<UGameplayEffect>> EnemyGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FString RewardDescriptor;
};

USTRUCT(BlueprintType)
struct FARWaveInstanceState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 WaveInstanceId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FName WaveRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	EARWavePhase Phase = EARWavePhase::Active;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	float WaveStartServerTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	float PhaseStartServerTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 SpawnedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 AliveCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	bool bColorSwap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FName StageRowName = NAME_None;
};

USTRUCT(BlueprintType)
struct FARInvaderRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	EARInvaderFlowState FlowState = EARInvaderFlowState::Stopped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	float Threat = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	float RunElapsedTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FName StageRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	float StageElapsedTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FName StageChoiceLeftRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FName StageChoiceRightRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FString StageChoiceLeftReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FString StageChoiceRightReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	float StageChoiceElapsedTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 StageSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 LeakCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 LeakLossThreshold = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 RewardEventId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FName LastRewardStageRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	FString LastRewardDescriptor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 SoftCapAliveEnemies = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	int32 SoftCapActiveProjectiles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Runtime")
	TArray<FARWaveInstanceState> ActiveWaves;
};
