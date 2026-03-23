/**
 * @file ARInvaderDirectorSubsystem.h
 * @brief ARInvaderDirectorSubsystem header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ARInvaderTypes.h"
#include "ARInvaderDirectorSubsystem.generated.h"

class UARInvaderRuntimeStateComponent;
class UARCharacterSubsystem;
class UDataTable;
class IConsoleObject;
class AAREnemyBase;
class AARCharacterStateRuntime;
class AARPlayerStateBase;
class AARGameStateBase;
class UAbilitySystemComponent;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnInvaderRunEndedSignature, EARInvaderRunEndReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnInvaderEnemyLeakedSignature, int32, NewLeakCount, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnInvaderAllCharactersDownChangedSignature, bool, bAllCharactersDown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnInvaderAllCharactersDeadChangedSignature, bool, bAllCharactersDead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnInvaderCharacterDownedChangedSignature, AARCharacterStateRuntime*, CharacterRuntime, bool, bIsDowned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnInvaderCharacterDeadChangedSignature, AARCharacterStateRuntime*, CharacterRuntime, bool, bIsDead);

/** World subsystem that orchestrates Invader runs: wave spawning, stage progression, leaks/death tracking, and console debug hooks. */
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

	// Marks/unmarks this player's intent to end the run early. Run ends when all active players vote yes.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	bool SubmitEarlyBailVote(AARPlayerStateBase* PlayerState, bool bVoteYes);

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

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader", meta = (BlueprintAuthorityOnly))
	bool SubmitStageChoice(bool bChooseLeft);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	EARInvaderFlowState GetFlowState() const { return FlowState; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	EARInvaderRunEndReason GetLastRunEndReason() const { return LastRunEndReason; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	bool IsCharacterDowned(const AARCharacterStateRuntime* CharacterRuntime) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	bool IsCharacterDead(const AARCharacterStateRuntime* CharacterRuntime) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	int32 GetDownedCharacterCount() const { return DownedCharacterCountCached; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	int32 GetDeadCharacterCount() const { return DeadCharacterCountCached; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	int32 GetEvaluatedCharacterCount() const { return EvaluatedCharacterCountCached; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	bool AreAllCharactersDowned() const { return bAllCharactersDownCached; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader")
	bool AreAllCharactersDead() const { return bAllCharactersDeadCached; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader")
	FString DumpRuntimeState() const;

	UFUNCTION(
		BlueprintCallable,
		Category = "Alien Ramen|Invader",
		meta = (
			DisplayName = "Report Enemy Leaked",
			ToolTip = "Server-side leak report for an enemy actor. Returns true only when the leak is newly accepted and counted; returns false if ignored (invalid/run inactive/non-authority) or already reported."
		))
	bool ReportEnemyLeaked(AAREnemyBase* Enemy);

	/** Registers a tracked projectile actor for active-projectile counting. */
	void RegisterTrackedProjectile(AActor* Projectile);

	/** Unregisters a tracked projectile actor from active-projectile counting. */
	void UnregisterTrackedProjectile(AActor* Projectile);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnInvaderRunEndedSignature OnRunEnded;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnInvaderEnemyLeakedSignature OnEnemyLeaked;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnInvaderAllCharactersDownChangedSignature OnAllCharactersDownChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnInvaderAllCharactersDeadChangedSignature OnAllCharactersDeadChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnInvaderCharacterDownedChangedSignature OnCharacterDownedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader")
	FAROnInvaderCharacterDeadChangedSignature OnCharacterDeadChanged;

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
		bool bFlipX = false;
		bool bFlipY = false;
		FName StageRowName = NAME_None;
		EARWavePhase Phase = EARWavePhase::Active;
		TArray<TWeakObjectPtr<class AAREnemyBase>> SpawnedEnemies;
	};

	void TickDirector(float DeltaTime);
	void UpdateThreat(float DeltaTime);
	void UpdateStage(float DeltaTime);
	void UpdateWaves(float DeltaTime);
	void SpawnWavesIfNeeded();
	void RecountAliveAndHandleLeaks();
	void EvaluateLossConditions();
	void EvaluateLossConditionsFromSignals();
	void PushSnapshotToGameState();
	void EnterAwaitStageClear();
	void EnterStageChoice();
	void EnterTransition(FName ChosenStageRow, const FARStageDefRow& ChosenStageDef);
	void DispatchStageReward(const FName& CompletedStageRow, const FARStageDefRow& CompletedStageDef);
	bool BuildStageChoiceOptions(FName& OutLeftRow, FARStageDefRow& OutLeftDef, FName& OutRightRow, FARStageDefRow& OutRightDef);
	void TickStageChoice(float DeltaTime);
	void TickTransition(float DeltaTime);
	void StopInvaderRunWithReason(EARInvaderRunEndReason EndReason);
	void ResetRunState(bool bForActiveRunStart);
	void RefreshCharacterStatusSignals();
	void EvaluateEarlyBailVotes();
	void RebuildCharacterStatusBindings();
	void ClearCharacterStatusBindings();

	UFUNCTION()
	void HandleTrackedPlayersChanged();

	UFUNCTION()
	void HandleCharacterRuntimeRegistered(AARCharacterStateRuntime* Runtime);

	UFUNCTION()
	void HandleCharacterRuntimeUnregistered(AARCharacterStateRuntime* Runtime);

	UFUNCTION()
	void HandleCharacterRuntimeDownedSignal(AARCharacterStateRuntime* SourceRuntime, FGameplayTag CharacterTag, bool bNewDowned, bool bOldDowned);

	UFUNCTION()
	void HandleCharacterRuntimeDeadSignal(AARCharacterStateRuntime* SourceRuntime, FGameplayTag CharacterTag, bool bNewDead, bool bOldDead);

	bool SpawnWaveFromDefinition(FName WaveRowName, const FARWaveDefRow& WaveDef, bool bColorSwap);
	bool TransitionWavePhase(FWaveRuntimeInternal& Wave, EARWavePhase NewPhase);
	bool SelectWave(FName& OutWaveRow, FARWaveDefRow& OutWaveDef, bool& bOutColorSwap);
	bool SelectStage(FName& OutStageRow, FARStageDefRow& OutStageDef, const TSet<FName>* ExcludedRows = nullptr);
	bool ResolveEnemyDefinitionByTag(FGameplayTag EnemyIdentifierTag, FARInvaderEnemyDefRow& OutDef, FString& OutError);
	void PreloadEnemyClassesForWaveCandidates();
	void PreloadEnemyClass(const TSoftClassPtr<AAREnemyBase>& EnemyClassRef);
	FVector ComputeFormationTargetLocation(const FARWaveEnemySpawnDef& SpawnDef, bool bFlipX, bool bFlipY) const;
	FVector ComputeSpawnLocation(const FARWaveEnemySpawnDef& SpawnDef, int32 SpawnOrdinal, bool bFlipX, bool bFlipY) const;
	void ApplyEnemyGameplayEffects(class AAREnemyBase* Enemy, const FARWaveDefRow& WaveDef, const FARWaveEnemySpawnDef& SpawnDef);
	bool IsInsideGameplayBounds(const FVector& Location) const;
	bool IsInsideEnteredScreenBounds(const FVector& Location) const;
	UARInvaderRuntimeStateComponent* GetOrCreateRuntimeComponent();
	bool EnsureDataTables();
	int32 GetAliveEnemyCount() const;
	int32 GetActiveProjectileCount() const;
	int32 GetActivePlayerCount() const;
	bool AreAllCharactersDown() const;
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
	void HandleConsoleChooseStage(const TArray<FString>& Args, UWorld* InWorld);
	void HandleConsoleCaptureBounds(const TArray<FString>& Args, UWorld* InWorld);
	void DestroyManagedInvaderEnemies();
	UClass* ResolveTrackedProjectileClass() const;

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
	int32 StageSequence = 0;
	int32 RewardEventId = 0;
	int32 RunEndEventId = 0;

	EARInvaderFlowState FlowState = EARInvaderFlowState::Stopped;
	EARInvaderRunEndReason LastRunEndReason = EARInvaderRunEndReason::None;
	float StageChoiceElapsed = 0.f;
	float StageTransitionRemaining = 0.f;

	FName CurrentStageRow = NAME_None;
	FARStageDefRow CurrentStageDef;
	FName PendingStageRow = NAME_None;
	FARStageDefRow PendingStageDef;
	FName ChoiceLeftStageRow = NAME_None;
	FARStageDefRow ChoiceLeftStageDef;
	FName ChoiceRightStageRow = NAME_None;
	FARStageDefRow ChoiceRightStageDef;
	FName LastRewardStageRow = NAME_None;
	FString LastRewardDescriptor;

	TArray<FWaveRuntimeInternal> ActiveWaves;
	TSet<FName> OneTimeWaveRowsUsed;
	FName LastWaveRowName = NAME_None;
	TSet<TWeakObjectPtr<AAREnemyBase>> ReportedLeakedEnemies;
	TSet<TWeakObjectPtr<const AActor>> ActiveProjectileActors;
	bool bAllCharactersDownCached = false;
	bool bAllCharactersDeadCached = false;
	int32 EvaluatedCharacterCountCached = 0;
	int32 DownedCharacterCountCached = 0;
	int32 DeadCharacterCountCached = 0;
	TMap<TWeakObjectPtr<AARCharacterStateRuntime>, uint8> CharacterDownedCache;
	TMap<TWeakObjectPtr<AARCharacterStateRuntime>, uint8> CharacterDeadCache;
	TSet<int32> EarlyBailVotesByPlayerSlotId;
	mutable TObjectPtr<UClass> CachedProjectileActorClass = nullptr;
	mutable bool bProjectileActorClassResolved = false;
	int32 CachedActiveProjectileCount = 0;

	struct FCharacterStatusBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FDelegateHandle HealthChangedHandle;
		FDelegateHandle MaxHealthChangedHandle;
		FDelegateHandle DownedTagChangedHandle;
		FDelegateHandle DeadTagChangedHandle;
	};
	TMap<TWeakObjectPtr<AARCharacterStateRuntime>, FCharacterStatusBinding> CharacterStatusBindings;
	TWeakObjectPtr<AARGameStateBase> BoundTrackedPlayersGameState;
	TWeakObjectPtr<UARCharacterSubsystem> BoundCharacterSubsystem;

	TObjectPtr<UDataTable> WaveTable = nullptr;
	TObjectPtr<UDataTable> StageTable = nullptr;

	TMap<FGameplayTag, FARInvaderEnemyDefRow> EnemyDefinitionCache;
	TMap<FSoftObjectPath, TSharedPtr<FStreamableHandle>> EnemyClassPreloadHandles;

	TMap<TWeakObjectPtr<class AAREnemyBase>, float> OffscreenDurationByEnemy;

	IConsoleObject* CmdForceWave = nullptr;
	IConsoleObject* CmdForcePhase = nullptr;
	IConsoleObject* CmdForceThreat = nullptr;
	IConsoleObject* CmdForceStage = nullptr;
	IConsoleObject* CmdDumpState = nullptr;
	IConsoleObject* CmdStart = nullptr;
	IConsoleObject* CmdStop = nullptr;
	IConsoleObject* CmdChooseStage = nullptr;
	IConsoleObject* CmdCaptureBounds = nullptr;
};
