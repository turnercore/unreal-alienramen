#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
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
	Entering = 0,
	Active = 1,
	Berserk = 2,
	Expired = 3
};

UENUM(BlueprintType)
enum class EARInvaderFlowState : uint8
{
	Combat = 0,
	AwaitStageClear = 1,
	StageChoice = 2,
	Transition = 3,
	StageIntro = 4,
	Stopped = 5
};

UENUM(BlueprintType)
enum class EAREntryMode : uint8
{
	StreamIn = 0,
	FormationEnter = 1,
	Immediate = 2
};

UENUM(BlueprintType)
enum class EARFormationMode : uint8
{
	None = 0,
	Hold = 1,
	RotateCW = 2,
	RotateCCW = 3
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	TSubclassOf<AAREnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	EAREnemyColor EnemyColor = EAREnemyColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	EARSpawnEdge SpawnEdge = EARSpawnEdge::Top;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float SpawnDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	FVector2D AuthoredScreenOffset = FVector2D::ZeroVector;

	// Effects applied to this spawned enemy instance (in addition to wave/stage effects).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	TArray<TSubclassOf<UGameplayEffect>> EnemyGameplayEffects;
};

USTRUCT(BlueprintType)
struct FARBerserkProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float MoveSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float FireRateMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	FGameplayTagContainer BehaviorTags;
};

USTRUCT(BlueprintType)
struct FARWaveDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float MinThreat = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float MaxThreat = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	int32 SelectionWeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	int32 MinPlayers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	int32 MaxPlayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	bool bOneTimeOnlyPerRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	bool bAllowColorSwap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float RepeatWeightPenalty = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	EAREntryMode EntryMode = EAREntryMode::StreamIn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	EARFormationMode FormationMode = EARFormationMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float EnterDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float ActiveDuration = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	float BerserkDuration = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	FARBerserkProfile BerserkProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	TArray<FARWaveEnemySpawnDef> EnemySpawns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	FGameplayTagContainer WaveTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	FGameplayTagContainer StageTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	FGameplayTagContainer BannedArchetypeTags;

	// Effects applied to all enemies spawned by this wave.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Wave")
	TArray<TSubclassOf<UGameplayEffect>> EnemyGameplayEffects;
};

USTRUCT(BlueprintType)
struct FARStageDefRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	float SelectionWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	float StageDuration = 55.f;

	// If < 0, director settings default intro duration is used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	float StageIntroSeconds = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	float ThreatGainMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	float BerserkTimeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	float EnemyHealthMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	FGameplayTagContainer RequiredWaveTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	FGameplayTagContainer BlockedWaveTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	FGameplayTagContainer BannedArchetypeTags;

	// Effects applied to all enemies spawned while this stage is active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
	TArray<TSubclassOf<UGameplayEffect>> EnemyGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Invader|Stage")
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
	EARWavePhase Phase = EARWavePhase::Entering;

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
	float StageIntroRemainingTime = 0.f;

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
