#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARInvaderDirectorSettings.generated.h"

class UDataTable;
class AActor;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Alien Ramen Invader Director"))
class ALIENRAMEN_API UARInvaderDirectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> WaveDataTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> StageDataTable;

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
	float DefaultStageIntroSeconds = 2.5f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run|Debug")
	bool bOverrideStageIntroForDebug = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run|Debug", meta = (EditCondition = "bOverrideStageIntroForDebug", ClampMin = "0.0"))
	float DebugStageIntroSeconds = 0.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	float StageChoiceAutoSelectSeconds = 0.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	bool bStageChoiceAutoSelectLeft = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run")
	int32 LeakLossThreshold = 20;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FVector SpawnOrigin = FVector::ZeroVector;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnOffscreenDistance = 2800.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnLaneSpacing = 220.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	FVector2D GameplayBoundsMin = FVector2D(-2800.f, -2200.f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	FVector2D GameplayBoundsMax = FVector2D(2800.f, 2200.f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	float OffscreenCullSeconds = 12.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps")
	int32 SoftCapAliveEnemies = 120;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps")
	int32 SoftCapActiveProjectiles = 500;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SoftCaps")
	bool bBlockSpawnsWhenEnemySoftCapExceeded = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Telemetry")
	TSoftClassPtr<AActor> ProjectileActorClass;
};
