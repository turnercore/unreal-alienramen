#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ARInvaderTypes.h"
#include "ARInvaderDirectorSubsystem.generated.h"

class UARInvaderRuntimeStateComponent;
class UDataTable;

UCLASS()
class ALIENRAMEN_API UARInvaderDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UARInvaderDirectorSubsystem();

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	void StartInvaderRun(int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	void StopInvaderRun();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	bool IsRunActive() const { return bRunActive; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	bool ForceWaveByRow(FName WaveRow);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	bool ForceWavePhase(int32 WaveInstanceId, EARWavePhase NewPhase);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	void ForceThreat(float NewThreat);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	bool ForceStage(FName StageRowName);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader")
	FString DumpRuntimeState() const;

private:
	struct FWaveRuntimeInternal
	{
		int32 WaveInstanceId = INDEX_NONE;
		FARWaveDefRow Def;
		FName RowName = NAME_None;
		float WaveStartTime = 0.f;
		float PhaseStartTime = 0.f;
		float NextSpawnTime = 0.f;
		int32 NextSpawnIndex = 0;
		int32 SpawnedCount = 0;
		int32 AliveCount = 0;
		bool bColorSwap = false;
		FName StageRowName = NAME_None;
		EARWavePhase Phase = EARWavePhase::Entering;
		TArray<TWeakObjectPtr<class AAREnemyBase>> SpawnedEnemies;
	};

	void TickDirector(float DeltaTime);
	void UpdateThreat(float DeltaTime);
	void UpdateStage(float DeltaTime);
	void UpdateWaves(float DeltaTime);
	void SpawnWavesIfNeeded();
	void RecountAliveAndHandleLeaks();
	void EvaluateLossConditions();
	void PushSnapshotToGameState();

	bool SpawnWaveFromDefinition(FName WaveRowName, const FARWaveDefRow& WaveDef, bool bColorSwap);
	bool TransitionWavePhase(FWaveRuntimeInternal& Wave, EARWavePhase NewPhase);
	bool SelectWave(FName& OutWaveRow, FARWaveDefRow& OutWaveDef, bool& bOutColorSwap);
	bool SelectStage(FName& OutStageRow, FARStageDefRow& OutStageDef);
	FVector ComputeSpawnLocation(const FARWaveEnemySpawnDef& SpawnDef, int32 SpawnOrdinal) const;
	bool IsInsideGameplayBounds(const FVector& Location) const;
	bool IsInsideLeakBounds(const FVector& Location) const;
	UARInvaderRuntimeStateComponent* GetOrCreateRuntimeComponent();
	bool EnsureDataTables();
	int32 GetAliveEnemyCount() const;
	int32 GetActiveProjectileCount() const;
	int32 GetActivePlayerCount() const;
	bool AreAllPlayersDown() const;
	void SetCurrentStage(FName StageRowName, const FARStageDefRow& StageDef);

	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	void HandleConsoleForceWave(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleForcePhase(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleForceThreat(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleForceStage(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleDumpState(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleStart(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleStop(const TArray<FString>& Args, UWorld* InWorld);

private:
	bool bRunActive = false;
	bool bWarnedMissingRuntimeComponent = false;

	int32 RunSeed = 0;
	FRandomStream RunRng;
	float Threat = 0.f;
	float RunElapsed = 0.f;
	float StageElapsed = 0.f;
	float TimeSinceLastWaveSpawn = 0.f;
	int32 NextWaveInstanceId = 1;
	int32 LeakCount = 0;

	FName CurrentStageRow = NAME_None;
	FARStageDefRow CurrentStageDef;

	TArray<FWaveRuntimeInternal> ActiveWaves;
	TSet<FName> OneTimeWaveRowsUsed;
	FName LastWaveRowName = NAME_None;

	TObjectPtr<UDataTable> WaveTable = nullptr;
	TObjectPtr<UDataTable> StageTable = nullptr;

	TMap<TWeakObjectPtr<class AAREnemyBase>, float> OffscreenDurationByEnemy;

	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdForceWave;
	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdForcePhase;
	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdForceThreat;
	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdForceStage;
	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdDumpState;
	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdStart;
	TUniquePtr<class FAutoConsoleCommandWithWorldAndArgs> CmdStop;
};
