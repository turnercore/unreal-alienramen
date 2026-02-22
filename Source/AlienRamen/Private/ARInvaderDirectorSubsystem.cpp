#include "ARInvaderDirectorSubsystem.h"

#include "AREnemyBase.h"
#include "ARAttributeSetCore.h"
#include "ARInvaderDirectorSettings.h"
#include "ARInvaderRuntimeStateComponent.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"

#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"

namespace ARInvaderInternal
{
	static EAREnemyColor SwapEnemyColor(EAREnemyColor InColor)
	{
		if (InColor == EAREnemyColor::Red) return EAREnemyColor::Blue;
		if (InColor == EAREnemyColor::Blue) return EAREnemyColor::Red;
		return InColor;
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
	TimeSinceLastWaveSpawn = TNumericLimits<float>::Max();
	NextWaveInstanceId = 1;
	LeakCount = 0;
	ActiveWaves.Reset();
	OneTimeWaveRowsUsed.Reset();
	LastWaveRowName = NAME_None;
	OffscreenDurationByEnemy.Reset();

	FName StageRow = NAME_None;
	FARStageDefRow StageDef;
	if (SelectStage(StageRow, StageDef))
	{
		SetCurrentStage(StageRow, StageDef);
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
	ActiveWaves.Reset();
	OffscreenDurationByEnemy.Reset();
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
	PushSnapshotToGameState();
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector|Debug] Forced stage '%s'."), *StageRowName.ToString());
	return true;
}

FString UARInvaderDirectorSubsystem::DumpRuntimeState() const
{
	FString Out = FString::Printf(
		TEXT("InvaderRun Active=%d Seed=%d Threat=%.2f Run=%.2fs Stage='%s' StageTime=%.2fs Leak=%d/%d Waves=%d"),
		bRunActive ? 1 : 0,
		RunSeed,
		Threat,
		RunElapsed,
		*CurrentStageRow.ToString(),
		StageElapsed,
		LeakCount,
		GetDefault<UARInvaderDirectorSettings>()->LeakLossThreshold,
		ActiveWaves.Num());

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
	StageElapsed += DeltaTime;

	UpdateThreat(DeltaTime);
	RecountAliveAndHandleLeaks();
	UpdateWaves(DeltaTime);
	SpawnWavesIfNeeded();
	UpdateStage(DeltaTime);
	EvaluateLossConditions();
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
		}
		return;
	}

	if (StageElapsed < CurrentStageDef.StageDuration)
	{
		return;
	}

	if (GetAliveEnemyCount() > 0)
	{
		return;
	}

	FName NewStageRow;
	FARStageDefRow NewStageDef;
	if (SelectStage(NewStageRow, NewStageDef))
	{
		SetCurrentStage(NewStageRow, NewStageDef);
	}
}

void UARInvaderDirectorSubsystem::UpdateWaves(float DeltaTime)
{
	(void)DeltaTime;

	const float StageBerserkTimeMultiplier = FMath::Max(0.05f, CurrentStageDef.BerserkTimeMultiplier <= 0.f ? 1.f : CurrentStageDef.BerserkTimeMultiplier);
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

			if (!SpawnDef.EnemyClass)
			{
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Wave '%s' has empty EnemyClass at spawn index %d."),
					*Wave.RowName.ToString(), Wave.NextSpawnIndex);
				Wave.NextSpawnIndex++;
				continue;
			}

			if (SpawnDef.FormationNodeId != NAME_None)
			{
				const bool bFoundNode = Wave.Def.FormationNodes.ContainsByPredicate(
					[&SpawnDef](const FARWaveFormationNodeDef& Node) { return Node.NodeId == SpawnDef.FormationNodeId; });
				if (!bFoundNode)
				{
					UE_LOG(ARLog, Warning,
						TEXT("[InvaderDirector|Validation] Wave '%s' spawn index %d references missing formation node '%s'. Falling back to non-formation behavior."),
						*Wave.RowName.ToString(), Wave.NextSpawnIndex, *SpawnDef.FormationNodeId.ToString());
				}
			}

			const FVector SpawnLocation = ComputeSpawnLocation(SpawnDef, Wave.NextSpawnIndex);
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AAREnemyBase* Enemy = GetWorld()->SpawnActor<AAREnemyBase>(SpawnDef.EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
			if (!Enemy)
			{
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector|Validation] Failed to spawn enemy class '%s' for wave '%s'."),
					*GetNameSafe(SpawnDef.EnemyClass), *Wave.RowName.ToString());
				Wave.NextSpawnIndex++;
				continue;
			}

			EAREnemyColor EffectiveColor = SpawnDef.EnemyColor;
			if (Wave.bColorSwap)
			{
				EffectiveColor = ARInvaderInternal::SwapEnemyColor(EffectiveColor);
			}

			Enemy->SetEnemyColor(EffectiveColor);
			Enemy->SetWaveRuntimeContext(Wave.WaveInstanceId, SpawnDef.SlotIndex, Wave.Def.FormationMode, Wave.Phase, GetWorld()->GetTimeSeconds());

			Wave.SpawnedEnemies.Add(Enemy);
			Wave.SpawnedCount++;
			Wave.AliveCount++;
			OffscreenDurationByEnemy.FindOrAdd(Enemy) = 0.f;

			Wave.NextSpawnIndex++;
		}

		const float EnterDuration = FMath::Max(0.f, Wave.Def.EnterDuration);
		const float ActiveDuration = FMath::Max(0.f, Wave.Def.ActiveDuration * StageBerserkTimeMultiplier);
		const float BerserkDuration = FMath::Max(0.f, Wave.Def.BerserkDuration * StageBerserkTimeMultiplier);
		const float TimeInWave = Now - Wave.WaveStartTime;

		if (Wave.Phase == EARWavePhase::Entering)
		{
			if (Wave.Def.EntryMode == EAREntryMode::Immediate || TimeInWave >= EnterDuration)
			{
				TransitionWavePhase(Wave, EARWavePhase::Active);
			}
		}
		else if (Wave.Phase == EARWavePhase::Active)
		{
			if (TimeInWave >= (EnterDuration + ActiveDuration))
			{
				TransitionWavePhase(Wave, EARWavePhase::Berserk);
			}
		}
		else if (Wave.Phase == EARWavePhase::Berserk)
		{
			if (TimeInWave >= (EnterDuration + ActiveDuration + BerserkDuration))
			{
				TransitionWavePhase(Wave, EARWavePhase::Expired);
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
	if (!EnsureDataTables())
	{
		return;
	}

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

	bool bAnyEnteringOrActive = false;
	bool bAnyAlive = false;
	for (const FWaveRuntimeInternal& Wave : ActiveWaves)
	{
		if (Wave.AliveCount > 0)
		{
			bAnyAlive = true;
		}
		if (Wave.Phase == EARWavePhase::Entering || Wave.Phase == EARWavePhase::Active)
		{
			bAnyEnteringOrActive = true;
		}
	}

	bool bShouldSpawn = false;
	float RequiredDelay = Settings->NewWaveDelayAfterClear;
	if (ActiveWaves.IsEmpty() || !bAnyAlive)
	{
		bShouldSpawn = true;
		RequiredDelay = Settings->NewWaveDelayAfterClear;
	}
	else if (!bAnyEnteringOrActive)
	{
		bShouldSpawn = true;
		RequiredDelay = Settings->NewWaveDelayWhenOvertime;
	}

	if (!bShouldSpawn || TimeSinceLastWaveSpawn < RequiredDelay)
	{
		return;
	}

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
				Enemy->NotifyEnteredGameplayScreen(GetWorld()->GetTimeSeconds());
				OffscreenDurationByEnemy.FindOrAdd(Enemy) = 0.f;
			}
			else
			{
				float& OffscreenSeconds = OffscreenDurationByEnemy.FindOrAdd(Enemy);
				OffscreenSeconds += GetWorld()->GetDeltaSeconds();
				if (OffscreenSeconds >= Settings->OffscreenCullSeconds)
				{
					UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Culling offscreen enemy '%s' after %.2fs."),
						*GetNameSafe(Enemy), OffscreenSeconds);
					Enemy->HandleDeath(nullptr);
					Enemy->Destroy();
					continue;
				}
			}

			if (!IsInsideLeakBounds(Loc) && !Enemy->HasBeenCountedAsLeak())
			{
				Enemy->MarkCountedAsLeak();
				LeakCount++;
				UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Enemy leaked. LeakCount=%d"), LeakCount);
				Enemy->HandleDeath(nullptr);
				Enemy->Destroy();
				continue;
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
		UE_LOG(ARLog, Warning, TEXT("[InvaderDirector] Loss condition reached: all players down."));
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
	Snapshot.Threat = Threat;
	Snapshot.RunElapsedTime = RunElapsed;
	Snapshot.StageRowName = CurrentStageRow;
	Snapshot.StageElapsedTime = StageElapsed;
	Snapshot.LeakCount = LeakCount;
	Snapshot.LeakLossThreshold = Settings->LeakLossThreshold;
	Snapshot.Seed = RunSeed;
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
	Wave.WaveStartTime = RunElapsed;
	Wave.PhaseStartTime = RunElapsed;
	Wave.NextSpawnTime = RunElapsed;
	Wave.NextSpawnIndex = 0;
	Wave.SpawnedCount = 0;
	Wave.AliveCount = 0;
	Wave.bColorSwap = bColorSwap;
	Wave.StageRowName = CurrentStageRow;
	Wave.Phase = EARWavePhase::Entering;

	// Ensure deterministic spawn ordering by spawn delay.
	Wave.Def.EnemySpawns.Sort([](const FARWaveEnemySpawnDef& A, const FARWaveEnemySpawnDef& B)
	{
		return A.SpawnDelay < B.SpawnDelay;
	});

	ActiveWaves.Add(MoveTemp(Wave));
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Spawned wave '%s' (WaveId=%d, ColorSwap=%d)."),
		*WaveRowName.ToString(), ActiveWaves.Last().WaveInstanceId, bColorSwap ? 1 : 0);
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

	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Wave %d ('%s') phase -> %d"),
		Wave.WaveInstanceId, *Wave.RowName.ToString(), static_cast<int32>(NewPhase));
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

bool UARInvaderDirectorSubsystem::SelectStage(FName& OutStageRow, FARStageDefRow& OutStageDef)
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
		const FARStageDefRow* Row = reinterpret_cast<const FARStageDefRow*>(Pair.Value);
		if (!Row)
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

FVector UARInvaderDirectorSubsystem::ComputeSpawnLocation(const FARWaveEnemySpawnDef& SpawnDef, int32 SpawnOrdinal) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	FVector Loc = Settings->SpawnOrigin;
	const float OffsetFromLane = SpawnOrdinal * Settings->SpawnLaneSpacing;

	switch (SpawnDef.SpawnEdge)
	{
	case EARSpawnEdge::Top:
		Loc.Y += Settings->SpawnOffscreenDistance;
		Loc.X += SpawnDef.AuthoredScreenOffset.X + OffsetFromLane;
		Loc.Y += SpawnDef.AuthoredScreenOffset.Y;
		break;
	case EARSpawnEdge::Left:
		Loc.X -= Settings->SpawnOffscreenDistance;
		Loc.X += SpawnDef.AuthoredScreenOffset.X;
		Loc.Y += SpawnDef.AuthoredScreenOffset.Y + OffsetFromLane;
		break;
	case EARSpawnEdge::Right:
		Loc.X += Settings->SpawnOffscreenDistance;
		Loc.X += SpawnDef.AuthoredScreenOffset.X;
		Loc.Y += SpawnDef.AuthoredScreenOffset.Y + OffsetFromLane;
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
			Loc.Y = Settings->GameplayBoundsMax.Y + FMath::Abs(Settings->SpawnOffscreenDistance * 0.5f);
			break;
		case EARSpawnEdge::Left:
			Loc.X = Settings->GameplayBoundsMin.X - FMath::Abs(Settings->SpawnOffscreenDistance * 0.5f);
			break;
		case EARSpawnEdge::Right:
			Loc.X = Settings->GameplayBoundsMax.X + FMath::Abs(Settings->SpawnOffscreenDistance * 0.5f);
			break;
		default:
			break;
		}
	}

	return Loc;
}

bool UARInvaderDirectorSubsystem::IsInsideGameplayBounds(const FVector& Location) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	return Location.X >= Settings->GameplayBoundsMin.X
		&& Location.X <= Settings->GameplayBoundsMax.X
		&& Location.Y >= Settings->GameplayBoundsMin.Y
		&& Location.Y <= Settings->GameplayBoundsMax.Y;
}

bool UARInvaderDirectorSubsystem::IsInsideLeakBounds(const FVector& Location) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	return Location.Y >= Settings->GameplayBoundsMin.Y;
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

	int32 NumPlayers = 0;
	int32 NumDown = 0;
	for (APlayerState* PSBase : GS->PlayerArray)
	{
		AARPlayerStateBase* PS = Cast<AARPlayerStateBase>(PSBase);
		if (!PS || PS->IsOnlyASpectator())
		{
			continue;
		}

		NumPlayers++;
		const UAbilitySystemComponent* ASC = PS->GetASC();
		if (!ASC)
		{
			continue;
		}

		const float Health = ASC->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
		if (Health <= 0.f)
		{
			NumDown++;
		}
	}

	return NumPlayers > 0 && NumPlayers == NumDown;
}

void UARInvaderDirectorSubsystem::SetCurrentStage(FName StageRowName, const FARStageDefRow& StageDef)
{
	CurrentStageRow = StageRowName;
	CurrentStageDef = StageDef;
	StageElapsed = 0.f;
	UE_LOG(ARLog, Log, TEXT("[InvaderDirector] Stage changed to '%s'."), *CurrentStageRow.ToString());
}

void UARInvaderDirectorSubsystem::RegisterConsoleCommands()
{
	CmdForceWave = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.force_wave"),
		TEXT("Force spawn a wave by row name."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForceWave));

	CmdForcePhase = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.force_phase"),
		TEXT("Force wave phase: <WaveId> <Entering|Active|Berserk|Expired>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForcePhase));

	CmdForceThreat = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.force_threat"),
		TEXT("Force threat value."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForceThreat));

	CmdForceStage = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.force_stage"),
		TEXT("Force stage row name."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleForceStage));

	CmdDumpState = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.dump_state"),
		TEXT("Dump invader runtime state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleDumpState));

	CmdStart = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.start"),
		TEXT("Start invader run. Optional arg: seed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleStart));

	CmdStop = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("ar.invader.stop"),
		TEXT("Stop invader run."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &UARInvaderDirectorSubsystem::HandleConsoleStop));
}

void UARInvaderDirectorSubsystem::UnregisterConsoleCommands()
{
	CmdForceWave.Reset();
	CmdForcePhase.Reset();
	CmdForceThreat.Reset();
	CmdForceStage.Reset();
	CmdDumpState.Reset();
	CmdStart.Reset();
	CmdStop.Reset();
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

	EARWavePhase Phase = EARWavePhase::Active;
	if (PhaseStr == TEXT("entering")) Phase = EARWavePhase::Entering;
	else if (PhaseStr == TEXT("active")) Phase = EARWavePhase::Active;
	else if (PhaseStr == TEXT("berserk")) Phase = EARWavePhase::Berserk;
	else if (PhaseStr == TEXT("expired")) Phase = EARWavePhase::Expired;

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
