#include "ARInvaderDirectorSubsystem.h"

#include "AREnemyBase.h"
#include "ARAttributeSetCore.h"
#include "ContentLookupSubsystem.h"
#include "ARInvaderDirectorSettings.h"
#include "ARInvaderRuntimeStateComponent.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"

#include "AbilitySystemComponent.h"
#include "Algo/StableSort.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace ARInvaderInternal
{
	static EAREnemyColor SwapEnemyColor(EAREnemyColor InColor)
	{
		if (InColor == EAREnemyColor::Red) return EAREnemyColor::Blue;
		if (InColor == EAREnemyColor::Blue) return EAREnemyColor::Red;
		return InColor;
	}

	static const TCHAR* ToPhaseName(EARWavePhase Phase)
	{
		switch (Phase)
		{
		case EARWavePhase::Active: return TEXT("Active");
		case EARWavePhase::Berserk: return TEXT("Berserk");
		default: return TEXT("Unknown");
		}
	}
}

UARInvaderDirectorSubsystem::UARInvaderDirectorSubsystem()
{
}

void UARInvaderDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterConsoleCommands();
}

void UARInvaderDirectorSubsystem::Deinitialize()
{
	UnregisterConsoleCommands();
	Super::Deinitialize();
}

bool UARInvaderDirectorSubsystem::IsTickable() const
{
	return GetWorld() != nullptr;
}

TStatId UARInvaderDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UARInvaderDirectorSubsystem, STATGROUP_Tickables);
}

void UARInvaderDirectorSubsystem::Tick(float DeltaTime)
{
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	if (!GetWorld()->GetAuthGameMode())
	{
		return;
	}

	if (bRunActive)
	{
		TickDirector(DeltaTime);
	}
}

void UARInvaderDirectorSubsystem::StartInvaderRun(int32 Seed)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode())
	{
		return;
	}

	if (!EnsureDataTables())
	{
		UE_LOG(ARLog, Error, TEXT("[InvaderDirector|Validation] Could not start run: wave/stage DataTables are unavailable."));
		return;
	}

	bRunActive = true;
	RunSeed = Seed;
	RunRng.Initialize(RunSeed);
	Threat = 0.f;
	RunElapsed = 0.f;
	StageElapsed = 0.f;
	StageChoiceElapsed = 0.f;
	StageTransitionRemaining = 0.f;
	TimeSinceLastWaveSpawn = TNumericLimits<float>::Max();
	NextWaveInstanceId = 1;
	LeakCount = 0;
	StageSequence = 0;
	RewardEventId = 0;
	FlowState = EARInvaderFlowState::Stopped;
	ActiveWaves.Reset();
	OneTimeWaveRowsUsed.Reset();
	LastWaveRowName = NAME_None;
	ReportedLeakedEnemies.Reset();
	EnemyDefinitionCache.Reset();
	EnemyClassPreloadHandles.Reset();
	OffscreenDurationByEnemy.Reset();
	PendingStageRow = NAME_None;
	ChoiceLeftStageRow = NAME_None;
	ChoiceRightStageRow = NAME_None;
	LastRewardStageRow = NAME_None;
	LastRewardDescriptor.Reset();

	FName StageRow = NAME_None;
	FARStageDefRow StageDef;
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!Settings->InitialStageRow.IsNone())
	{
		if (const FARStageDefRow* InitialRow = StageTable->FindRow<FARStageDefRow>(Settings->InitialStageRow, TEXT("StartInvaderRun.InitialStage")))
		{
			StageRow = Settings->InitialStageRow;
			StageDef = *InitialRow;
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] InitialStageRow '%s' not found; falling back to weighted stage selection."),
				*Settings->InitialStageRow.ToString());
		}
	}

	if (StageRow.IsNone() && SelectStage(StageRow, StageDef))
	{
		SetCurrentStage(StageRow, StageDef);
	}
	else if (!StageRow.IsNone())
	{
		SetCurrentStage(StageRow, StageDef);
	}

	if (!CurrentStageRow.IsNone())
	{
		FlowState = EARInvaderFlowState::Combat;
	}

	PushSnapshotToGameState();
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Started run. Seed=%d Stage='%s'."), RunSeed, *CurrentStageRow.ToString());
}

void UARInvaderDirectorSubsystem::StopInvaderRun()
{
	if (!bRunActive)
	{
		return;
	}

	bRunActive = false;
	FlowState = EARInvaderFlowState::Stopped;
	ActiveWaves.Reset();
	ReportedLeakedEnemies.Reset();
	EnemyDefinitionCache.Reset();
	EnemyClassPreloadHandles.Reset();
	OffscreenDurationByEnemy.Reset();
	ChoiceLeftStageRow = NAME_None;
	ChoiceRightStageRow = NAME_None;
	PushSnapshotToGameState();
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stopped run."));
}

bool UARInvaderDirectorSubsystem::ForceWaveByRow(FName WaveRow)
{
	if (!bRunActive || !WaveTable)
	{
		return false;
	}

	const FARWaveDefRow* Row = WaveTable->FindRow<FARWaveDefRow>(WaveRow, TEXT("ForceWaveByRow"));
	if (!Row)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Debug] ForceWaveByRow failed: wave '%s' not found."), *WaveRow.ToString());
		return false;
	}

	const bool bColorSwap = (LastWaveRowName == WaveRow) && Row->bAllowColorSwap;
	const bool bSpawned = SpawnWaveFromDefinition(WaveRow, *Row, bColorSwap);
	if (bSpawned)
	{
		UE_LOG(ARLog, Log, TEXT("[InvaderDirector|Debug] Forced wave '%s' (ColorSwap=%d)."), *WaveRow.ToString(), bColorSwap ? 1 : 0);
	}
	return bSpawned;
}

bool UARInvaderDirectorSubsystem::ForceWavePhase(int32 WaveInstanceId, EARWavePhase NewPhase)
{
	for (FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		if (Wave.WaveInstanceId == WaveInstanceId)
		{
			const bool bTransitioned = TransitionWavePhase(Wave, NewPhase);
			if (bTransitioned)
			{
				PushSnapshotToGameState();
				UE_LOG(ARLog, Log, TEXT("[InvaderDirector|Debug] Forced wave %d phase -> %d."), WaveInstanceId, static_cast<int32>(NewPhase));
			}
			return bTransitioned;
		}
	}
	return false;
}

bool UARInvaderDirectorSubsystem::ReportEnemyLeaked(AAREnemyBase* Enemy)
{
	if (!Enemy || !GetWorld() || !GetWorld()->GetAuthGameMode() || !bRunActive)
	{
		return false;
	}

	TWeakObjectPtr<AAREnemyBase> WeakEnemy(Enemy);
	if (ReportedLeakedEnemies.Contains(WeakEnemy))
	{
		return false;
	}

	ReportedLeakedEnemies.Add(WeakEnemy);
	LeakCount++;
	UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Enemy leaked '%s'. LeakCount=%d"), *GetNameSafe(Enemy), LeakCount);
	PushSnapshotToGameState();
	return true;
}

void UARInvaderDirectorSubsystem::ForceThreat(float NewThreat)
{
	Threat = FMath::Max(0.f, NewThreat);
	PushSnapshotToGameState();
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector|Debug] Forced threat = %.2f"), Threat);
}

bool UARInvaderDirectorSubsystem::ForceStage(FName StageRowName)
{
	if (!StageTable)
	{
		return false;
	}

	const FARStageDefRow* StageRow = StageTable->FindRow<FARStageDefRow>(StageRowName, TEXT("ForceStage"));
	if (!StageRow)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Debug] ForceStage failed: '%s' not found."), *StageRowName.ToString());
		return false;
	}

	SetCurrentStage(StageRowName, *StageRow);
	FlowState = EARInvaderFlowState::Combat;
	TimeSinceLastWaveSpawn = TNumericLimits<float>::Max();
	PendingStageRow = NAME_None;
	ChoiceLeftStageRow = NAME_None;
	ChoiceRightStageRow = NAME_None;
	PushSnapshotToGameState();
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector|Debug] Forced stage '%s'."), *StageRowName.ToString());
	return true;
}

bool UARInvaderDirectorSubsystem::SubmitStageChoice(bool bChooseLeft)
{
	if (!bRunActive || FlowState != EARInvaderFlowState::StageChoice)
	{
		return false;
	}

	const FName ChosenRow = bChooseLeft ? ChoiceLeftStageRow : ChoiceRightStageRow;
	const FARStageDefRow ChosenDef = bChooseLeft ? ChoiceLeftStageDef : ChoiceRightStageDef;
	if (ChosenRow.IsNone())
	{
		return false;
	}

	EnterTransition(ChosenRow, ChosenDef);
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage choice submitted: %s"), *ChosenRow.ToString());
	return true;
}

FString UARInvaderDirectorSubsystem::DumpRuntimeState() const
{
	FString Out = FString::Printf(
		TEXT("InvaderRun Active=%d Flow=%d Seed=%d Threat=%.2f Run=%.2fs Stage='%s' StageTime=%.2fs Leak=%d/%d Waves=%d Choice(L='%s',R='%s',t=%.2f)"),
		bRunActive ? 1 : 0,
		static_cast<int32>(FlowState),
		RunSeed,
		Threat,
		RunElapsed,
		*CurrentStageRow.ToString(),
		StageElapsed,
		LeakCount,
		GetDefault<UARInvaderDirectorSettings>()->LeakLossThreshold,
		ActiveWaves.Num(),
		*ChoiceLeftStageRow.ToString(),
		*ChoiceRightStageRow.ToString(),
		StageChoiceElapsed);

	for (const FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		Out += FString::Printf(
			TEXT("\n  WaveId=%d Row='%s' Phase=%d Spawned=%d Alive=%d ColorSwap=%d"),
			Wave.WaveInstanceId,
			*Wave.RowName.ToString(),
			static_cast<int32>(Wave.Phase),
			Wave.SpawnedCount,
			Wave.AliveCount,
			Wave.bColorSwap ? 1 : 0);
	}

	return Out;
}

void UARInvaderDirectorSubsystem::TickDirector(float DeltaTime)
{
	TimeSinceLastWaveSpawn += DeltaTime;
	RunElapsed += DeltaTime;

	switch (FlowState)
	{
	case EARInvaderFlowState::Combat:
		StageElapsed += DeltaTime;
		UpdateThreat(DeltaTime);
		RecountAliveAndHandleLeaks();
		UpdateWaves(DeltaTime);
		SpawnWavesIfNeeded();
		UpdateStage(DeltaTime);
		EvaluateLossConditions();
		break;
	case EARInvaderFlowState::AwaitStageClear:
		RecountAliveAndHandleLeaks();
		UpdateWaves(DeltaTime);
		EvaluateLossConditions();
		UpdateStage(DeltaTime);
		break;
	case EARInvaderFlowState::StageChoice:
		TickStageChoice(DeltaTime);
		break;
	case EARInvaderFlowState::Transition:
		TickTransition(DeltaTime);
		break;
	case EARInvaderFlowState::Stopped:
	default:
		break;
	}

	PushSnapshotToGameState();
}

void UARInvaderDirectorSubsystem::UpdateThreat(float DeltaTime)
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const float StageMultiplier = CurrentStageDef.ThreatGainMultiplier > 0.f ? CurrentStageDef.ThreatGainMultiplier : 1.f;
	Threat += Settings->BaseThreatGainPerSecond * StageMultiplier * DeltaTime;
}

void UARInvaderDirectorSubsystem::UpdateStage(float DeltaTime)
{
	(void)DeltaTime;

	if (CurrentStageRow.IsNone())
	{
		FName NewStageRow;
		FARStageDefRow NewStageDef;
		if (SelectStage(NewStageRow, NewStageDef))
		{
			SetCurrentStage(NewStageRow, NewStageDef);
			FlowState = EARInvaderFlowState::Combat;
			TimeSinceLastWaveSpawn = TNumericLimits<float>::Max();
		}
		return;
	}

	if (FlowState == EARInvaderFlowState::Combat)
	{
		if (StageElapsed >= CurrentStageDef.StageDuration)
		{
			EnterAwaitStageClear();
		}
		return;
	}

	if (FlowState == EARInvaderFlowState::AwaitStageClear)
	{
		if (GetAliveEnemyCount() <= 0)
		{
			DispatchStageReward(CurrentStageRow, CurrentStageDef);
			EnterStageChoice();
		}
	}
}

void UARInvaderDirectorSubsystem::UpdateWaves(float DeltaTime)
{
	(void)DeltaTime;

	const float Now = RunElapsed;

	for (FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		while (Wave.NextSpawnIndex < Wave.Def.EnemySpawns.Num())
		{
			const FARWaveEnemySpawnDef& SpawnDef = Wave.Def.EnemySpawns[Wave.NextSpawnIndex];
			if (Now < (Wave.WaveStartTime + SpawnDef.SpawnDelay))
			{
				break;
			}

			FARInvaderEnemyDefRow EnemyDef;
			FString EnemyResolveError;
			if (!ResolveEnemyDefinitionByTag(SpawnDef.EnemyIdentifierTag, EnemyDef, EnemyResolveError))
			{
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Wave '%s' has invalid enemy tag at spawn index %d: %s"),
					*Wave.RowName.ToString(), Wave.NextSpawnIndex, *EnemyResolveError);
				Wave.NextSpawnIndex++;
				continue;
			}
			if (!EnemyDef.bEnabled)
			{
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Enemy tag '%s' is disabled; skipping spawn in wave '%s'."),
					*SpawnDef.EnemyIdentifierTag.ToString(), *Wave.RowName.ToString());
				Wave.NextSpawnIndex++;
				continue;
			}

			UClass* EnemyClass = EnemyDef.EnemyClass.LoadSynchronous();
			if (!EnemyClass)
			{
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Enemy tag '%s' resolved with no enemy class for wave '%s'."),
					*SpawnDef.EnemyIdentifierTag.ToString(), *Wave.RowName.ToString());
				Wave.NextSpawnIndex++;
				continue;
			}

			const FVector FormationTargetLocation = ComputeFormationTargetLocation(SpawnDef, Wave.bFlipX, Wave.bFlipY);
			const FVector SpawnLocation = ComputeSpawnLocation(SpawnDef, Wave.NextSpawnIndex, Wave.bFlipX, Wave.bFlipY);
			const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
			// Face straight down gameplay progression (toward low-X / player side) consistently.
			constexpr float BaseSpawnYaw = 180.f;
			const FRotator SpawnRotation(0.f, BaseSpawnYaw + Settings->SpawnFacingYawOffset, 0.f);
			const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
			AAREnemyBase* Enemy = GetWorld()->SpawnActorDeferred<AAREnemyBase>(
				EnemyClass,
				SpawnTransform,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Enemy)
			{
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Failed to spawn enemy class '%s' for wave '%s'."),
					*GetNameSafe(EnemyClass), *Wave.RowName.ToString());
				Wave.NextSpawnIndex++;
				continue;
			}

			Enemy->SetEnemyIdentifierTag(SpawnDef.EnemyIdentifierTag);
			UGameplayStatics::FinishSpawningActor(Enemy, SpawnTransform);

			EAREnemyColor EffectiveColor = SpawnDef.EnemyColor;
			if (Wave.bColorSwap)
			{
				EffectiveColor = ARInvaderInternal::SwapEnemyColor(EffectiveColor);
			}

			Enemy->SetEnemyColor(EffectiveColor);
			Enemy->SetFormationTargetWorldLocation(FormationTargetLocation);
			Enemy->SetFormationLockRules(Wave.Def.bFormationLockEnter, Wave.Def.bFormationLockActive);
			Enemy->SetWaveRuntimeContext(Wave.WaveInstanceId, Wave.NextSpawnIndex, Wave.Phase, GetWorld()->GetTimeSeconds());
			ApplyEnemyGameplayEffects(Enemy, Wave.Def, SpawnDef);

			Wave.SpawnedEnemies.Add(Enemy);
			Wave.SpawnedCount++;
			Wave.AliveCount++;
			OffscreenDurationByEnemy.FindOrAdd(Enemy) = 0.f;

			Wave.NextSpawnIndex++;
		}

		const float WaveDuration = FMath::Max(0.f, Wave.Def.WaveDuration);
		const float TimeInWave = Now - Wave.WaveStartTime;

		if (Wave.Phase == EARWavePhase::Active)
		{
			if (TimeInWave >= WaveDuration)
			{
				UE_LOG(
					ARLog,
					Log,
					TEXT("[InvaderDirector|Phase] Wave %d ('%s') Active->Berserk elapsed=%.2fs waveDuration=%.2fs."),
					Wave.WaveInstanceId,
					*Wave.RowName.ToString(),
					TimeInWave,
					WaveDuration);
				TransitionWavePhase(Wave, EARWavePhase::Berserk);
			}
		}
	}

	ActiveWaves.RemoveAll([](const FWaveRuntimeInternal& Wave)
	{
		const bool bFullySpawned = Wave.NextSpawnIndex >= Wave.Def.EnemySpawns.Num();
		const bool bCleared = Wave.AliveCount <= 0;
		return bFullySpawned && bCleared;
	});
}

void UARInvaderDirectorSubsystem::SpawnWavesIfNeeded()
{
	if (FlowState != EARInvaderFlowState::Combat)
	{
		return;
	}

	if (!EnsureDataTables())
	{
		return;
	}

	PreloadEnemyClassesForWaveCandidates();

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const int32 AliveEnemies = GetAliveEnemyCount();
	if (Settings->SoftCapAliveEnemies > 0 && AliveEnemies > Settings->SoftCapAliveEnemies)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Perf] Alive enemies soft cap exceeded: %d/%d."),
			AliveEnemies, Settings->SoftCapAliveEnemies);
		if (Settings->bBlockSpawnsWhenEnemySoftCapExceeded)
		{
			return;
		}
	}

	bool bAnyActive = false;
	bool bAnyAlive = false;
	for (const FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		if (Wave.AliveCount > 0)
		{
			bAnyAlive = true;
		}
		if (Wave.Phase == EARWavePhase::Active)
		{
			bAnyActive = true;
		}
	}

	bool bShouldSpawn = false;
	float RequiredDelay = Settings->NewWaveDelayAfterClear;
	if (ActiveWaves.IsEmpty() || !bAnyAlive)
	{
		bShouldSpawn = true;
		RequiredDelay = Settings->NewWaveDelayAfterClear;
	}
	else if (!bAnyActive)
	{
		bShouldSpawn = true;
		RequiredDelay = Settings->NewWaveDelayWhenOvertime;
	}

	if (!bShouldSpawn || TimeSinceLastWaveSpawn < RequiredDelay)
	{
		return;
	}

	const TCHAR* SpawnReason = TEXT("Unknown");
	if (ActiveWaves.IsEmpty() || !bAnyAlive)
	{
		SpawnReason = TEXT("NoAliveWaves");
	}
	else if (!bAnyActive)
	{
		SpawnReason = TEXT("Overtime_NoActiveWave");
	}

	UE_LOG(
		ARLog,
		Log,
		TEXT("[InvaderDirector|SpawnGate] Triggered reason=%s activeWaves=%d aliveEnemies=%d hasActive=%d timeSinceLast=%.2fs requiredDelay=%.2fs."),
		SpawnReason,
		ActiveWaves.Num(),
		AliveEnemies,
		bAnyActive ? 1 : 0,
		TimeSinceLastWaveSpawn,
		RequiredDelay);

	FName WaveRow;
	FARWaveDefRow WaveDef;
	bool bColorSwap = false;
	if (!SelectWave(WaveRow, WaveDef, bColorSwap))
	{
		return;
	}

	if (SpawnWaveFromDefinition(WaveRow, WaveDef, bColorSwap))
	{
		TimeSinceLastWaveSpawn = 0.f;
		LastWaveRowName = WaveRow;
		if (WaveDef.bOneTimeOnlyPerRun)
		{
			OneTimeWaveRowsUsed.Add(WaveRow);
		}
	}
}

void UARInvaderDirectorSubsystem::RecountAliveAndHandleLeaks()
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();

	for (FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		int32 NewAlive = 0;
		for (TWeakObjectPtr<AAREnemyBase>& WeakEnemy : Wave.SpawnedEnemies)
		{
			AAREnemyBase* Enemy = WeakEnemy.Get();
			if (!Enemy || Enemy->IsDead())
			{
				continue;
			}

			const FVector Loc = Enemy->GetActorLocation();
			if (IsInsideGameplayBounds(Loc))
			{
				if (IsInsideEnteredScreenBounds(Loc))
				{
					Enemy->NotifyEnteredGameplayScreen(GetWorld()->GetTimeSeconds());
				}
				OffscreenDurationByEnemy.FindOrAdd(Enemy) = 0.f;
			}
			else
			{
				// Do not offscreen-cull before first gameplay entry; entering paths can legitimately begin offscreen.
				if (!Enemy->HasEnteredGameplayScreen())
				{
					// Still alive while entering from offscreen; keep wave alive accounting correct.
					NewAlive++;
					continue;
				}

				float& OffscreenSeconds = OffscreenDurationByEnemy.FindOrAdd(Enemy);
				OffscreenSeconds += GetWorld()->GetDeltaSeconds();
				if (OffscreenSeconds >= Settings->OffscreenCullSeconds)
				{
					UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Culling offscreen enemy '%s' after %.2fs at Loc=(%.1f, %.1f, %.1f)."),
						*GetNameSafe(Enemy), OffscreenSeconds, Loc.X, Loc.Y, Loc.Z);
					Enemy->HandleDeath(nullptr);
					Enemy->Destroy();
					continue;
				}
			}

			NewAlive++;
		}
		Wave.AliveCount = NewAlive;
	}

	const int32 ActiveProjectiles = GetActiveProjectileCount();
	if (Settings->SoftCapActiveProjectiles > 0 && ActiveProjectiles > Settings->SoftCapActiveProjectiles)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Perf] Active projectiles soft cap exceeded: %d/%d."),
			ActiveProjectiles, Settings->SoftCapActiveProjectiles);
	}
}

void UARInvaderDirectorSubsystem::EvaluateLossConditions()
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (LeakCount >= Settings->LeakLossThreshold)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Loss condition reached: leaks (%d/%d)."), LeakCount, Settings->LeakLossThreshold);
		StopInvaderRun();
		return;
	}

	if (AreAllPlayersDown())
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Loss condition reached: all evaluated players are down."));
		StopInvaderRun();
	}
}

void UARInvaderDirectorSubsystem::PushSnapshotToGameState()
{
	UARInvaderRuntimeStateComponent* RuntimeComp = GetOrCreateRuntimeComponent();
	if (!RuntimeComp)
	{
		if (!bWarnedMissingRuntimeComponent)
		{
			bWarnedMissingRuntimeComponent = true;
			UE_LOG(ARLog, Error, TEXT("[InvaderDirector|Validation] Missing UARInvaderRuntimeStateComponent; running in degraded mode."));
		}
		return;
	}

	FARInvaderRuntimeSnapshot Snapshot;
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	Snapshot.FlowState = FlowState;
	Snapshot.Threat = Threat;
	Snapshot.RunElapsedTime = RunElapsed;
	Snapshot.StageRowName = CurrentStageRow;
	Snapshot.StageElapsedTime = StageElapsed;
	Snapshot.StageChoiceLeftRowName = ChoiceLeftStageRow;
	Snapshot.StageChoiceRightRowName = ChoiceRightStageRow;
	Snapshot.StageChoiceLeftReward = ChoiceLeftStageDef.RewardDescriptor;
	Snapshot.StageChoiceRightReward = ChoiceRightStageDef.RewardDescriptor;
	Snapshot.StageChoiceElapsedTime = StageChoiceElapsed;
	Snapshot.StageSequence = StageSequence;
	Snapshot.LeakCount = LeakCount;
	Snapshot.LeakLossThreshold = Settings->LeakLossThreshold;
	Snapshot.Seed = RunSeed;
	Snapshot.RewardEventId = RewardEventId;
	Snapshot.LastRewardStageRowName = LastRewardStageRow;
	Snapshot.LastRewardDescriptor = LastRewardDescriptor;
	Snapshot.SoftCapAliveEnemies = Settings->SoftCapAliveEnemies;
	Snapshot.SoftCapActiveProjectiles = Settings->SoftCapActiveProjectiles;

	Snapshot.ActiveWaves.Reserve(ActiveWaves.Num());
	for (const FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		FARWaveInstanceState Row;
		Row.WaveInstanceId = Wave.WaveInstanceId;
		Row.WaveRowName = Wave.RowName;
		Row.Phase = Wave.Phase;
		Row.WaveStartServerTime = Wave.WaveStartTime;
		Row.PhaseStartServerTime = Wave.PhaseStartTime;
		Row.SpawnedCount = Wave.SpawnedCount;
		Row.AliveCount = Wave.AliveCount;
		Row.bColorSwap = Wave.bColorSwap;
		Row.StageRowName = Wave.StageRowName;
		Snapshot.ActiveWaves.Add(Row);
	}

	RuntimeComp->SetRuntimeSnapshot(Snapshot);
}

bool UARInvaderDirectorSubsystem::SpawnWaveFromDefinition(FName WaveRowName, const FARWaveDefRow& WaveDef, bool bColorSwap)
{
	FWaveRuntimeInternal Wave;
	Wave.WaveInstanceId = NextWaveInstanceId++;
	Wave.RowName = WaveRowName;
	Wave.Def = WaveDef;
	Wave.Def.EnterDuration = FMath::Max(0.f, Wave.Def.EnterDuration);
	if (Wave.Def.WaveDuration <= 0.f)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Wave '%s' has WaveDuration <= 0; defaulting to 16s."), *WaveRowName.ToString());
		Wave.Def.WaveDuration = 16.f;
	}
	Wave.WaveStartTime = RunElapsed;
	Wave.PhaseStartTime = RunElapsed;
	Wave.NextSpawnTime = RunElapsed;
	Wave.NextSpawnIndex = 0;
	Wave.SpawnedCount = 0;
	Wave.AliveCount = 0;
	Wave.bColorSwap = bColorSwap;
	Wave.bFlipX = Wave.Def.bAllowFlipX && RunRng.FRand() < 0.5f;
	Wave.bFlipY = Wave.Def.bAllowFlipY && RunRng.FRand() < 0.5f;
	Wave.StageRowName = CurrentStageRow;
	Wave.Phase = EARWavePhase::Active;

	// Ensure deterministic spawn ordering by spawn delay.
	Algo::StableSort(Wave.Def.EnemySpawns, [](const FARWaveEnemySpawnDef& A, const FARWaveEnemySpawnDef& B)
	{
		return A.SpawnDelay < B.SpawnDelay;
	});

	ActiveWaves.Add(MoveTemp(Wave));
	const FWaveRuntimeInternal& SpawnedWave = ActiveWaves.Last();

	float MaxSpawnDelay = 0.f;
	for (const FARWaveEnemySpawnDef& SpawnDef : SpawnedWave.Def.EnemySpawns)
	{
		MaxSpawnDelay = FMath::Max(MaxSpawnDelay, SpawnDef.SpawnDelay);
	}

	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Spawned wave '%s' (WaveId=%d, ColorSwap=%d)."),
		*WaveRowName.ToString(), SpawnedWave.WaveInstanceId, bColorSwap ? 1 : 0);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[InvaderDirector|WaveDef] WaveId=%d row='%s' waveDuration=%.2fs spawns=%d maxSpawnDelay=%.2fs lockEnter=%d lockActive=%d."),
		SpawnedWave.WaveInstanceId,
		*WaveRowName.ToString(),
		SpawnedWave.Def.WaveDuration,
		SpawnedWave.Def.EnemySpawns.Num(),
		MaxSpawnDelay,
		SpawnedWave.Def.bFormationLockEnter ? 1 : 0,
		SpawnedWave.Def.bFormationLockActive ? 1 : 0);
	UE_LOG(
		ARLog,
		Log,
		TEXT("[InvaderDirector|WaveDef] WaveId=%d flipX=%d flipY=%d (allowedX=%d allowedY=%d)."),
		SpawnedWave.WaveInstanceId,
		SpawnedWave.bFlipX ? 1 : 0,
		SpawnedWave.bFlipY ? 1 : 0,
		SpawnedWave.Def.bAllowFlipX ? 1 : 0,
		SpawnedWave.Def.bAllowFlipY ? 1 : 0);
	return true;
}

bool UARInvaderDirectorSubsystem::TransitionWavePhase(FWaveRuntimeInternal& Wave, EARWavePhase NewPhase)
{
	if (Wave.Phase == NewPhase)
	{
		return false;
	}

	Wave.Phase = NewPhase;
	Wave.PhaseStartTime = RunElapsed;

	for (TWeakObjectPtr<AAREnemyBase>& WeakEnemy : Wave.SpawnedEnemies)
	{
		if (AAREnemyBase* Enemy = WeakEnemy.Get())
		{
			Enemy->SetWavePhase(NewPhase, GetWorld()->GetTimeSeconds());
		}
	}

	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Wave %d ('%s') phase -> %s"),
		Wave.WaveInstanceId, *Wave.RowName.ToString(), ARInvaderInternal::ToPhaseName(NewPhase));
	return true;
}

bool UARInvaderDirectorSubsystem::SelectWave(FName& OutWaveRow, FARWaveDefRow& OutWaveDef, bool& bOutColorSwap)
{
	OutWaveRow = NAME_None;
	bOutColorSwap = false;

	if (!WaveTable)
	{
		return false;
	}

	const int32 Players = GetActivePlayerCount();
	const FARStageDefRow& StageDef = CurrentStageDef;

	struct FCandidate
	{
		FName RowName;
		FARWaveDefRow Def;
		float Weight = 1.f;
	};

	TArray<FCandidate> Candidates;
	for (const TPair<FName, uint8*>& Pair : WaveTable->GetRowMap())
	{
		const FARWaveDefRow* Row = reinterpret_cast<const FARWaveDefRow*>(Pair.Value);
		if (!Row)
		{
			continue;
		}

		if (!Row->bEnabled)
		{
			continue;
		}

		if (Threat < Row->MinThreat || Threat > Row->MaxThreat)
		{
			continue;
		}

		if (Players < Row->MinPlayers || Players > Row->MaxPlayers)
		{
			continue;
		}

		if (Row->bOneTimeOnlyPerRun && OneTimeWaveRowsUsed.Contains(Pair.Key))
		{
			continue;
		}

		if (!StageDef.RequiredWaveTags.IsEmpty() && !Row->WaveTags.HasAll(StageDef.RequiredWaveTags))
		{
			continue;
		}

		if (!StageDef.BlockedWaveTags.IsEmpty() && Row->WaveTags.HasAny(StageDef.BlockedWaveTags))
		{
			continue;
		}

		float Weight = FMath::Max(1.f, static_cast<float>(Row->SelectionWeight));
		if (Pair.Key == LastWaveRowName)
		{
			Weight *= FMath::Clamp(Row->RepeatWeightPenalty, 0.01f, 1.f);
		}

		Candidates.Add({Pair.Key, *Row, Weight});
	}

	if (Candidates.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] No wave candidate matched. Threat=%.2f Players=%d Stage='%s'"),
			Threat, Players, *CurrentStageRow.ToString());
		return false;
	}

	float TotalWeight = 0.f;
	for (const FCandidate& Candidate : Candidates)
	{
		TotalWeight += Candidate.Weight;
	}

	float Roll = RunRng.FRandRange(0.f, TotalWeight);
	for (const FCandidate& Candidate : Candidates)
	{
		Roll -= Candidate.Weight;
		if (Roll <= 0.f)
		{
			OutWaveRow = Candidate.RowName;
			OutWaveDef = Candidate.Def;
			break;
		}
	}

	if (OutWaveRow.IsNone())
	{
		OutWaveRow = Candidates[0].RowName;
		OutWaveDef = Candidates[0].Def;
	}

	bOutColorSwap = (OutWaveRow == LastWaveRowName) && OutWaveDef.bAllowColorSwap;
	return true;
}

bool UARInvaderDirectorSubsystem::SelectStage(FName& OutStageRow, FARStageDefRow& OutStageDef, const TSet<FName>* ExcludedRows)
{
	OutStageRow = NAME_None;

	if (!StageTable || StageTable->GetRowMap().IsEmpty())
	{
		return false;
	}

	float TotalWeight = 0.f;
	struct FCandidate
	{
		FName RowName;
		FARStageDefRow Def;
		float Weight = 1.f;
	};
	TArray<FCandidate> Candidates;

	for (const TPair<FName, uint8*>& Pair : StageTable->GetRowMap())
	{
		if (ExcludedRows && ExcludedRows->Contains(Pair.Key))
		{
			continue;
		}

		const FARStageDefRow* Row = reinterpret_cast<const FARStageDefRow*>(Pair.Value);
		if (!Row)
		{
			continue;
		}

		if (!Row->bEnabled)
		{
			continue;
		}

		const float Weight = FMath::Max(0.01f, Row->SelectionWeight);
		TotalWeight += Weight;
		Candidates.Add({Pair.Key, *Row, Weight});
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	float Roll = RunRng.FRandRange(0.f, TotalWeight);
	for (const FCandidate& Candidate : Candidates)
	{
		Roll -= Candidate.Weight;
		if (Roll <= 0.f)
		{
			OutStageRow = Candidate.RowName;
			OutStageDef = Candidate.Def;
			return true;
		}
	}

	OutStageRow = Candidates[0].RowName;
	OutStageDef = Candidates[0].Def;
	return true;
}

FVector UARInvaderDirectorSubsystem::ComputeFormationTargetLocation(const FARWaveEnemySpawnDef& SpawnDef, bool bFlipX, bool bFlipY) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const float CenterX = (Settings->GameplayBoundsMin.X + Settings->GameplayBoundsMax.X) * 0.5f;
	const float CenterY = (Settings->GameplayBoundsMin.Y + Settings->GameplayBoundsMax.Y) * 0.5f;

	FVector2D AuthoredOffset = SpawnDef.AuthoredScreenOffset;
	if (bFlipX)
	{
		AuthoredOffset.X = (CenterX * 2.f) - AuthoredOffset.X;
	}
	if (bFlipY)
	{
		AuthoredOffset.Y = (CenterY * 2.f) - AuthoredOffset.Y;
	}

	FVector Loc = Settings->SpawnOrigin + FVector(AuthoredOffset.X, AuthoredOffset.Y, -26.f);
	return Loc;
}

FVector UARInvaderDirectorSubsystem::ComputeSpawnLocation(const FARWaveEnemySpawnDef& SpawnDef, int32 SpawnOrdinal, bool bFlipX, bool bFlipY) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	FVector Loc = ComputeFormationTargetLocation(SpawnDef, bFlipX, bFlipY);
	const float OffscreenDistance = FMath::Abs(Settings->SpawnOffscreenDistance);
	const float GameplaySizeX = FMath::Max(0.f, Settings->GameplayBoundsMax.X - Settings->GameplayBoundsMin.X);
	const float GameplaySizeY = FMath::Max(0.f, Settings->GameplayBoundsMax.Y - Settings->GameplayBoundsMin.Y);

	switch (SpawnDef.SpawnEdge)
	{
	case EARSpawnEdge::Top:
		// Keep authored formation shape, translated one gameplay-screen above the top boundary.
		Loc.X += (GameplaySizeX + OffscreenDistance);
		break;
	case EARSpawnEdge::Left:
		// Keep authored formation shape, translated one gameplay-screen left of the left boundary.
		Loc.Y -= (GameplaySizeY + OffscreenDistance);
		break;
	case EARSpawnEdge::Right:
		// Keep authored formation shape, translated one gameplay-screen right of the right boundary.
		Loc.Y += (GameplaySizeY + OffscreenDistance);
		break;
	default:
		break;
	}

	if (IsInsideGameplayBounds(Loc))
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Computed spawn was inside gameplay bounds; pushing out to edge."));
		switch (SpawnDef.SpawnEdge)
		{
		case EARSpawnEdge::Top:
			Loc.X = Settings->GameplayBoundsMax.X + FMath::Abs(Settings->SpawnOffscreenDistance * 0.5f);
			break;
		case EARSpawnEdge::Left:
			Loc.Y = Settings->GameplayBoundsMin.Y - FMath::Abs(Settings->SpawnOffscreenDistance * 0.5f);
			break;
		case EARSpawnEdge::Right:
			Loc.Y = Settings->GameplayBoundsMax.Y + FMath::Abs(Settings->SpawnOffscreenDistance * 0.5f);
			break;
		default:
			break;
		}
	}

	Loc.Z = -26.f;
	if (SpawnOrdinal < 4)
	{
	UE_LOG(
			ARLog,
			Verbose,
			TEXT("[InvaderDirector|SpawnPos] Ordinal=%d edge=%d authored=(%.1f,%.1f) world=(%.1f,%.1f,%.1f)."),
			SpawnOrdinal,
			static_cast<int32>(SpawnDef.SpawnEdge),
			SpawnDef.AuthoredScreenOffset.X,
			SpawnDef.AuthoredScreenOffset.Y,
			Loc.X,
			Loc.Y,
			Loc.Z);
	}
	return Loc;
}

void UARInvaderDirectorSubsystem::ApplyEnemyGameplayEffects(AAREnemyBase* Enemy, const FARWaveDefRow& WaveDef, const FARWaveEnemySpawnDef& SpawnDef)
{
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Cannot apply enemy gameplay effects: missing ASC on '%s'."),
			*GetNameSafe(Enemy));
		return;
	}

	int32 AppliedCount = 0;
	auto ApplyEffects = [ASC, &AppliedCount](const TArray<TSubclassOf<UGameplayEffect>>& Effects)
	{
		for (const TSubclassOf<UGameplayEffect>& EffectClass : Effects)
		{
			if (!EffectClass)
			{
				continue;
			}

			const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				AppliedCount++;
			}
		}
	};

	ApplyEffects(CurrentStageDef.EnemyGameplayEffects);
	ApplyEffects(WaveDef.EnemyGameplayEffects);
	ApplyEffects(SpawnDef.EnemyGameplayEffects);

	if (AppliedCount > 0)
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderDirector] Applied %d gameplay effects to enemy '%s'."), AppliedCount, *GetNameSafe(Enemy));
	}
}

bool UARInvaderDirectorSubsystem::ResolveEnemyDefinitionByTag(FGameplayTag EnemyIdentifierTag, FARInvaderEnemyDefRow& OutDef, FString& OutError)
{
	OutDef = FARInvaderEnemyDefRow();
	OutError.Reset();

	if (!EnemyIdentifierTag.IsValid())
	{
		OutError = TEXT("EnemyIdentifierTag is invalid.");
		return false;
	}

	if (const FARInvaderEnemyDefRow* Cached = EnemyDefinitionCache.Find(EnemyIdentifierTag))
	{
		OutDef = *Cached;
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("No world.");
		return false;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		OutError = TEXT("No game instance.");
		return false;
	}

	UContentLookupSubsystem* Lookup = GI->GetSubsystem<UContentLookupSubsystem>();
	if (!Lookup)
	{
		OutError = TEXT("No ContentLookupSubsystem.");
		return false;
	}

	FInstancedStruct ResolvedRow;
	if (!Lookup->LookupWithGameplayTag(EnemyIdentifierTag, ResolvedRow, OutError))
	{
		return false;
	}

	const UScriptStruct* RowType = ResolvedRow.GetScriptStruct();
	const void* RowData = ResolvedRow.GetMemory();
	if (!RowType || !RowData)
	{
		OutError = TEXT("Resolved enemy row has no data.");
		return false;
	}

	if (RowType == FARInvaderEnemyDefRow::StaticStruct())
	{
		OutDef = *static_cast<const FARInvaderEnemyDefRow*>(RowData);
	}
	else
	{
		auto FindAnyProp = [RowType](const TCHAR* A, const TCHAR* B = nullptr) -> const FProperty*
		{
			if (const FProperty* P = RowType->FindPropertyByName(A))
			{
				return P;
			}
			return B ? RowType->FindPropertyByName(B) : nullptr;
		};

		if (const FProperty* EnabledProp = FindAnyProp(TEXT("bEnabled")))
		{
			if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(EnabledProp))
			{
				OutDef.bEnabled = BoolProp->GetPropertyValue_InContainer(RowData);
			}
		}

		if (const FProperty* DisplayNameProp = FindAnyProp(TEXT("DisplayName")))
		{
			if (const FTextProperty* TextProp = CastField<FTextProperty>(DisplayNameProp))
			{
				OutDef.DisplayName = TextProp->GetPropertyValue_InContainer(RowData);
			}
		}

		if (const FProperty* DescriptionProp = FindAnyProp(TEXT("Description")))
		{
			if (const FTextProperty* TextProp = CastField<FTextProperty>(DescriptionProp))
			{
				OutDef.Description = TextProp->GetPropertyValue_InContainer(RowData);
			}
		}

		if (const FProperty* IdentifierProp = FindAnyProp(TEXT("EnemyIdentifierTag"), TEXT("IdentifierTag")))
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(IdentifierProp))
			{
				if (StructProp->Struct == FGameplayTag::StaticStruct())
				{
					const void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(RowData);
					OutDef.EnemyIdentifierTag = *reinterpret_cast<const FGameplayTag*>(ValuePtr);
				}
			}
		}

		if (const FProperty* EnemyClassProp = FindAnyProp(TEXT("EnemyClass"), TEXT("Blueprint")))
		{
			if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(EnemyClassProp))
			{
				const FSoftObjectPtr SoftClassObj = SoftClassProp->GetPropertyValue_InContainer(RowData);
				OutDef.EnemyClass = TSoftClassPtr<AAREnemyBase>(SoftClassObj.ToSoftObjectPath());
			}
			else if (const FClassProperty* ClassProp = CastField<FClassProperty>(EnemyClassProp))
			{
				OutDef.EnemyClass = Cast<UClass>(ClassProp->GetPropertyValue_InContainer(RowData));
			}
		}

		if (const FProperty* RuntimeInitProp = FindAnyProp(TEXT("RuntimeInit")))
		{
			if (const FStructProperty* StructProp = CastField<FStructProperty>(RuntimeInitProp))
			{
				if (StructProp->Struct == FARInvaderEnemyRuntimeInitData::StaticStruct())
				{
					const void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(RowData);
					OutDef.RuntimeInit = *reinterpret_cast<const FARInvaderEnemyRuntimeInitData*>(ValuePtr);
				}
			}
		}

		if (OutDef.RuntimeInit.MaxHealth <= 0.f)
		{
			if (const FProperty* MaxHealthProp = FindAnyProp(TEXT("MaxHealth"), TEXT("maxHp")))
			{
				if (const FNumericProperty* NumericProp = CastField<FNumericProperty>(MaxHealthProp))
				{
					const void* ValuePtr = NumericProp->ContainerPtrToValuePtr<void>(RowData);
					OutDef.RuntimeInit.MaxHealth = static_cast<float>(NumericProp->GetFloatingPointPropertyValue(ValuePtr));
				}
			}
		}
	}

	if (!OutDef.EnemyIdentifierTag.IsValid())
	{
		OutDef.EnemyIdentifierTag = EnemyIdentifierTag;
	}
	if (OutDef.RuntimeInit.MaxHealth <= 0.f)
	{
		OutDef.RuntimeInit.MaxHealth = 100.f;
	}

	EnemyDefinitionCache.Add(EnemyIdentifierTag, OutDef);
	return true;
}

void UARInvaderDirectorSubsystem::PreloadEnemyClass(const TSoftClassPtr<AAREnemyBase>& EnemyClassRef)
{
	if (EnemyClassRef.IsNull())
	{
		return;
	}

	const FSoftObjectPath Path = EnemyClassRef.ToSoftObjectPath();
	if (!Path.IsValid() || EnemyClassPreloadHandles.Contains(Path))
	{
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	if (TSharedPtr<FStreamableHandle> Handle = Streamable.RequestAsyncLoad(Path, FStreamableDelegate()))
	{
		EnemyClassPreloadHandles.Add(Path, Handle);
	}
}

void UARInvaderDirectorSubsystem::PreloadEnemyClassesForWaveCandidates()
{
	if (!WaveTable || !StageTable)
	{
		return;
	}

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const int32 Lookahead = FMath::Max(0, Settings->EnemyPreloadWaveLookahead);
	if (Lookahead <= 0)
	{
		return;
	}

	const int32 Players = GetActivePlayerCount();
	struct FWaveCandidate
	{
		FName RowName = NAME_None;
		const FARWaveDefRow* Def = nullptr;
		float Weight = 0.f;
	};

	TArray<FWaveCandidate> Candidates;
	Candidates.Reserve(WaveTable->GetRowMap().Num());
	for (const TPair<FName, uint8*>& Pair : WaveTable->GetRowMap())
	{
		const FARWaveDefRow* Row = reinterpret_cast<const FARWaveDefRow*>(Pair.Value);
		if (!Row || !Row->bEnabled)
		{
			continue;
		}
		if (Threat < Row->MinThreat || Threat > Row->MaxThreat)
		{
			continue;
		}
		if (Players < Row->MinPlayers || Players > Row->MaxPlayers)
		{
			continue;
		}
		if (Row->bOneTimeOnlyPerRun && OneTimeWaveRowsUsed.Contains(Pair.Key))
		{
			continue;
		}
		if (!CurrentStageDef.RequiredWaveTags.IsEmpty() && !Row->WaveTags.HasAll(CurrentStageDef.RequiredWaveTags))
		{
			continue;
		}
		if (!CurrentStageDef.BlockedWaveTags.IsEmpty() && Row->WaveTags.HasAny(CurrentStageDef.BlockedWaveTags))
		{
			continue;
		}

		float Weight = FMath::Max(1.f, static_cast<float>(Row->SelectionWeight));
		if (Pair.Key == LastWaveRowName)
		{
			Weight *= FMath::Clamp(Row->RepeatWeightPenalty, 0.01f, 1.f);
		}

		FWaveCandidate Candidate;
		Candidate.RowName = Pair.Key;
		Candidate.Def = Row;
		Candidate.Weight = Weight;
		Candidates.Add(Candidate);
	}

	Candidates.Sort([](const FWaveCandidate& A, const FWaveCandidate& B)
	{
		if (FMath::IsNearlyEqual(A.Weight, B.Weight))
		{
			return A.RowName.LexicalLess(B.RowName);
		}
		return A.Weight > B.Weight;
	});

	const int32 Count = FMath::Min(Lookahead, Candidates.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FARWaveDefRow* Row = Candidates[Index].Def;
		if (!Row)
		{
			continue;
		}

		for (const FARWaveEnemySpawnDef& SpawnDef : Row->EnemySpawns)
		{
			FARInvaderEnemyDefRow EnemyDef;
			FString Error;
			if (ResolveEnemyDefinitionByTag(SpawnDef.EnemyIdentifierTag, EnemyDef, Error))
			{
				PreloadEnemyClass(EnemyDef.EnemyClass);
			}
		}
	}
}

bool UARInvaderDirectorSubsystem::IsInsideGameplayBounds(const FVector& Location) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	return Location.X >= Settings->GameplayBoundsMin.X
		&& Location.X <= Settings->GameplayBoundsMax.X
		&& Location.Y >= Settings->GameplayBoundsMin.Y
		&& Location.Y <= Settings->GameplayBoundsMax.Y;
}

bool UARInvaderDirectorSubsystem::IsInsideEnteredScreenBounds(const FVector& Location) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const float Inset = FMath::Max(0.f, Settings->EnteredScreenInset);

	const float MinX = Settings->GameplayBoundsMin.X + Inset;
	const float MaxX = Settings->GameplayBoundsMax.X - Inset;
	const float MinY = Settings->GameplayBoundsMin.Y + Inset;
	const float MaxY = Settings->GameplayBoundsMax.Y - Inset;

	// If inset collapses the range, fallback to normal gameplay bounds.
	if (MinX >= MaxX || MinY >= MaxY)
	{
		return IsInsideGameplayBounds(Location);
	}

	return Location.X >= MinX
		&& Location.X <= MaxX
		&& Location.Y >= MinY
		&& Location.Y <= MaxY;
}

UARInvaderRuntimeStateComponent* UARInvaderDirectorSubsystem::GetOrCreateRuntimeComponent()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return nullptr;
	}

	if (UARInvaderRuntimeStateComponent* Existing = GS->FindComponentByClass<UARInvaderRuntimeStateComponent>())
	{
		return Existing;
	}

	if (!GS->HasAuthority())
	{
		return nullptr;
	}

	UARInvaderRuntimeStateComponent* NewComp = NewObject<UARInvaderRuntimeStateComponent>(GS, TEXT("ARInvaderRuntimeStateComponent"));
	if (!NewComp)
	{
		return nullptr;
	}

	GS->AddInstanceComponent(NewComp);
	NewComp->RegisterComponent();
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Added runtime state component to GameState '%s'."), *GetNameSafe(GS));
	return NewComp;
}

bool UARInvaderDirectorSubsystem::EnsureDataTables()
{
	if (WaveTable && StageTable)
	{
		return true;
	}

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!Settings)
	{
		return false;
	}
	if (!WaveTable)
	{
		WaveTable = Settings->WaveDataTable.LoadSynchronous();
	}
	if (!StageTable)
	{
		StageTable = Settings->StageDataTable.LoadSynchronous();
	}

	if (!WaveTable || !StageTable)
	{
		UE_LOG(ARLog, Error, TEXT("[InvaderDirector|Validation] Missing data tables. Wave='%s' Stage='%s'"),
			*Settings->WaveDataTable.ToString(), *Settings->StageDataTable.ToString());
		return false;
	}
	return true;
}

int32 UARInvaderDirectorSubsystem::GetAliveEnemyCount() const
{
	int32 Count = 0;
	for (const FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		Count += Wave.AliveCount;
	}
	return Count;
}

int32 UARInvaderDirectorSubsystem::GetActiveProjectileCount() const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	UClass* ProjectileClass = Settings->ProjectileActorClass.LoadSynchronous();
	if (!ProjectileClass || !GetWorld())
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<AActor> It(GetWorld(), ProjectileClass); It; ++It)
	{
		if (IsValid(*It))
		{
			Count++;
		}
	}
	return Count;
}

int32 UARInvaderDirectorSubsystem::GetActivePlayerCount() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return 0;
	}

	int32 Num = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (PS && !PS->IsOnlyASpectator())
		{
			Num++;
		}
	}
	return Num;
}

bool UARInvaderDirectorSubsystem::AreAllPlayersDown() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return false;
	}

	int32 NumEvaluatedPlayers = 0;
	int32 NumDown = 0;
	for (APlayerState* PSBase : GS->PlayerArray)
	{
		AARPlayerStateBase* PS = Cast<AARPlayerStateBase>(PSBase);
		if (!PS || PS->IsOnlyASpectator())
		{
			continue;
		}

		const UAbilitySystemComponent* ASC = PS->GetASC();
		if (!ASC)
		{
			continue;
		}

		const float MaxHealth = ASC->GetNumericAttribute(UARAttributeSetCore::GetMaxHealthAttribute());
		const float Health = ASC->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
		if (MaxHealth <= 0.f)
		{
			// Player state/attributes are not initialized yet; do not treat as down.
			continue;
		}

		NumEvaluatedPlayers++;
		if (Health <= 0.f)
		{
			NumDown++;
		}
	}

	return NumEvaluatedPlayers > 0 && NumEvaluatedPlayers == NumDown;
}

void UARInvaderDirectorSubsystem::SetCurrentStage(FName StageRowName, const FARStageDefRow& StageDef)
{
	CurrentStageRow = StageRowName;
	CurrentStageDef = StageDef;
	StageElapsed = 0.f;
	StageSequence++;
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage changed to '%s'."), *CurrentStageRow.ToString());
}

void UARInvaderDirectorSubsystem::EnterAwaitStageClear()
{
	if (FlowState != EARInvaderFlowState::Combat)
	{
		return;
	}

	FlowState = EARInvaderFlowState::AwaitStageClear;
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage '%s' reached duration; waiting for clear."), *CurrentStageRow.ToString());
}

void UARInvaderDirectorSubsystem::EnterStageChoice()
{
	FName LeftRow = NAME_None;
	FARStageDefRow LeftDef;
	FName RightRow = NAME_None;
	FARStageDefRow RightDef;
	if (!BuildStageChoiceOptions(LeftRow, LeftDef, RightRow, RightDef))
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Could not build stage choice options; selecting fallback stage."));
		if (SelectStage(LeftRow, LeftDef))
		{
			RightRow = LeftRow;
			RightDef = LeftDef;
		}
	}

	ChoiceLeftStageRow = LeftRow;
	ChoiceLeftStageDef = LeftDef;
	ChoiceRightStageRow = RightRow;
	ChoiceRightStageDef = RightDef;
	StageChoiceElapsed = 0.f;
	FlowState = EARInvaderFlowState::StageChoice;

	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage choice started. Left='%s' Right='%s'."),
		*ChoiceLeftStageRow.ToString(), *ChoiceRightStageRow.ToString());
}

void UARInvaderDirectorSubsystem::EnterTransition(FName ChosenStageRow, const FARStageDefRow& ChosenStageDef)
{
	PendingStageRow = ChosenStageRow;
	PendingStageDef = ChosenStageDef;
	FlowState = EARInvaderFlowState::Transition;
	StageTransitionRemaining = FMath::Max(0.f, GetDefault<UARInvaderDirectorSettings>()->StageTransitionDelay);

	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage transition started -> '%s' (delay=%.2fs)."),
		*PendingStageRow.ToString(), StageTransitionRemaining);
}

void UARInvaderDirectorSubsystem::DispatchStageReward(const FName& CompletedStageRow, const FARStageDefRow& CompletedStageDef)
{
	LastRewardStageRow = CompletedStageRow;
	LastRewardDescriptor = CompletedStageDef.RewardDescriptor;
	RewardEventId++;

	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage '%s' reward dispatched: '%s'."),
		*CompletedStageRow.ToString(), *CompletedStageDef.RewardDescriptor);
}

bool UARInvaderDirectorSubsystem::BuildStageChoiceOptions(
	FName& OutLeftRow,
	FARStageDefRow& OutLeftDef,
	FName& OutRightRow,
	FARStageDefRow& OutRightDef)
{
	OutLeftRow = NAME_None;
	OutRightRow = NAME_None;

	TSet<FName> Excluded;
	if (!CurrentStageRow.IsNone())
	{
		Excluded.Add(CurrentStageRow);
	}

	if (!SelectStage(OutLeftRow, OutLeftDef, &Excluded))
	{
		return false;
	}

	Excluded.Add(OutLeftRow);
	if (!SelectStage(OutRightRow, OutRightDef, &Excluded))
	{
		OutRightRow = OutLeftRow;
		OutRightDef = OutLeftDef;
	}

	return true;
}

void UARInvaderDirectorSubsystem::TickStageChoice(float DeltaTime)
{
	StageChoiceElapsed += DeltaTime;

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (Settings->StageChoiceAutoSelectSeconds > 0.f
		&& StageChoiceElapsed >= Settings->StageChoiceAutoSelectSeconds)
	{
		SubmitStageChoice(Settings->bStageChoiceAutoSelectLeft);
	}
}

void UARInvaderDirectorSubsystem::TickTransition(float DeltaTime)
{
	StageTransitionRemaining -= DeltaTime;
	if (StageTransitionRemaining > 0.f)
	{
		return;
	}

	if (PendingStageRow.IsNone())
	{
		FlowState = EARInvaderFlowState::Combat;
		TimeSinceLastWaveSpawn = TNumericLimits<float>::Max();
		return;
	}

	SetCurrentStage(PendingStageRow, PendingStageDef);
	PendingStageRow = NAME_None;
	ChoiceLeftStageRow = NAME_None;
	ChoiceRightStageRow = NAME_None;
	StageChoiceElapsed = 0.f;
	FlowState = EARInvaderFlowState::Combat;
	TimeSinceLastWaveSpawn = TNumericLimits<float>::Max();
}

void UARInvaderDirectorSubsystem::RegisterConsoleCommands()
{
	// Console command names are global; PIE world travel can initialize a new subsystem
	// before the previous world deinitializes, so clear any prior registration first.
	UnregisterConsoleCommands();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	CmdForceWave = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.force_wave"),
		TEXT("Force spawn a wave by row name."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForceWave),
		ECVF_Default);

	CmdForcePhase = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.force_phase"),
		TEXT("Force wave phase: <WaveId> <Active|Berserk>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForcePhase),
		ECVF_Default);

	CmdForceThreat = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.force_threat"),
		TEXT("Force threat value."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForceThreat),
		ECVF_Default);

	CmdForceStage = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.force_stage"),
		TEXT("Force stage row name."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForceStage),
		ECVF_Default);

	CmdDumpState = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.dump_state"),
		TEXT("Dump invader runtime state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleDumpState),
		ECVF_Default);

	CmdStart = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.start"),
		TEXT("Start invader run. Optional arg: seed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleStart),
		ECVF_Default);

	CmdStop = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.stop"),
		TEXT("Stop invader run."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleStop),
		ECVF_Default);

	CmdChooseStage = ConsoleManager.RegisterConsoleCommand(
		TEXT("ar.invader.choose_stage"),
		TEXT("Submit stage choice: left|right."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleChooseStage),
		ECVF_Default);
}

void UARInvaderDirectorSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	auto UnregisterByName = [&ConsoleManager](const TCHAR* Name)
	{
		if (!Name || !ConsoleManager.IsNameRegistered(Name))
		{
			return;
		}

		ConsoleManager.UnregisterConsoleObject(Name, false);
	};

	UnregisterByName(TEXT("ar.invader.force_wave"));
	UnregisterByName(TEXT("ar.invader.force_phase"));
	UnregisterByName(TEXT("ar.invader.force_threat"));
	UnregisterByName(TEXT("ar.invader.force_stage"));
	UnregisterByName(TEXT("ar.invader.dump_state"));
	UnregisterByName(TEXT("ar.invader.start"));
	UnregisterByName(TEXT("ar.invader.stop"));
	UnregisterByName(TEXT("ar.invader.choose_stage"));

	CmdForceWave = nullptr;
	CmdForcePhase = nullptr;
	CmdForceThreat = nullptr;
	CmdForceStage = nullptr;
	CmdDumpState = nullptr;
	CmdStart = nullptr;
	CmdStop = nullptr;
	CmdChooseStage = nullptr;
}

void UARInvaderDirectorSubsystem::HandleConsoleForceWave(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld != GetWorld() || Args.Num() < 1) return;
	ForceWaveByRow(FName(*Args[0]));
}

void UARInvaderDirectorSubsystem::HandleConsoleForcePhase(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld != GetWorld() || Args.Num() < 2) return;

	int32 WaveId = INDEX_NONE;
	LexFromString(WaveId, *Args[0]);
	const FString PhaseStr = Args[1].ToLower();

	EARWavePhase Phase;
	if (PhaseStr == TEXT("active"))
	{
		Phase = EARWavePhase::Active;
	}
	else if (PhaseStr == TEXT("berserk"))
	{
		Phase = EARWavePhase::Berserk;
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Debug] Invalid phase '%s'. Valid: Active|Berserk."), *Args[1]);
		return;
	}

	ForceWavePhase(WaveId, Phase);
}

void UARInvaderDirectorSubsystem::HandleConsoleForceThreat(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld != GetWorld() || Args.Num() < 1) return;
	float NewThreat = 0.f;
	LexFromString(NewThreat, *Args[0]);
	ForceThreat(NewThreat);
}

void UARInvaderDirectorSubsystem::HandleConsoleForceStage(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld != GetWorld() || Args.Num() < 1) return;
	ForceStage(FName(*Args[0]));
}

void UARInvaderDirectorSubsystem::HandleConsoleDumpState(const TArray<FString>& Args, UWorld* InWorld)
{
	(void)Args;
	if (InWorld != GetWorld()) return;
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector|Debug] %s"), *DumpRuntimeState());
}

void UARInvaderDirectorSubsystem::HandleConsoleStart(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld != GetWorld()) return;

	int32 Seed = 1337;
	if (Args.Num() >= 1)
	{
		LexFromString(Seed, *Args[0]);
	}
	StartInvaderRun(Seed);
}

void UARInvaderDirectorSubsystem::HandleConsoleStop(const TArray<FString>& Args, UWorld* InWorld)
{
	(void)Args;
	if (InWorld != GetWorld()) return;
	StopInvaderRun();
}

void UARInvaderDirectorSubsystem::HandleConsoleChooseStage(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld != GetWorld() || Args.Num() < 1) return;

	const FString Side = Args[0].ToLower();
	if (Side == TEXT("left") || Side == TEXT("l"))
	{
		SubmitStageChoice(true);
	}
	else if (Side == TEXT("right") || Side == TEXT("r"))
	{
		SubmitStageChoice(false);
	}
}
