#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARInvaderToolingSettings.generated.h"

class UDataTable;

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
};

