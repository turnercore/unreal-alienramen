#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARInvaderToolingSettings.generated.h"

class UDataTable;
class UWorld;

UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Alien Ramen Tooling"))
class ALIENRAMENEDITOR_API UARInvaderToolingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UARInvaderToolingSettings();

	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Tooling"); }

	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring")
	TSoftObjectPtr<UDataTable> WaveDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring")
	TSoftObjectPtr<UDataTable> StageDataTable;

	// Long package path, e.g. /Game/CodeAlong/Blueprints/Enemies.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring")
	FDirectoryPath EnemiesFolder;

	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|Persistence")
	bool bAutoSaveTablesOnEdit = false;

	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|Backups")
	bool bCreateBackupOnToolOpen = true;

	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|Backups", meta=(ClampMin="1", UIMin="1"))
	int32 BackupRetentionCount = 10;

	// Long package path under /Game where backup snapshots are written, e.g. /Game/Data/Backups.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|Backups")
	FDirectoryPath BackupsFolder;

	// If enabled, PIE harness will boot into a loading map, call GameInstance LoadSave(SlotName, SlotNumber), then travel to debug map.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|PIE Save Bootstrap")
	bool bEnablePIESaveBootstrap = false;

	// Optional loading/bootstrap map used when bEnablePIESaveBootstrap is true.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|PIE Save Bootstrap")
	TSoftObjectPtr<UWorld> PIEBootstrapLoadingMap;

	// Slot name value passed to GameInstance LoadSave.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|PIE Save Bootstrap")
	FName PIELoadSlotName = TEXT("Debug");

	// Slot number value passed to GameInstance LoadSave.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|PIE Save Bootstrap")
	int32 PIELoadSlotNumber = 0;

	// Optional map to travel to after GameInstance load call. Falls back to editor DefaultTestMap when unset.
	UPROPERTY(Config, EditAnywhere, Category="Invader Authoring|PIE Save Bootstrap")
	TSoftObjectPtr<UWorld> PIEBootstrapDebugMap;

};
