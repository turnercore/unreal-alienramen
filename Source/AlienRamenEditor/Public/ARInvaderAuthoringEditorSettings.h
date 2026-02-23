#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARInvaderAuthoringEditorSettings.generated.h"

class UWorld;

UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, meta=(DisplayName="Alien Ramen Invader Authoring"))
class ALIENRAMENEDITOR_API UARInvaderAuthoringEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UARInvaderAuthoringEditorSettings();

	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }

	UPROPERTY(Config, EditAnywhere, Category = "Testing")
	TSoftObjectPtr<UWorld> DefaultTestMap;

	UPROPERTY(Config, EditAnywhere, Category = "Testing", meta=(ClampMin="0"))
	int32 LastSeed = 1337;

	UPROPERTY(Config, EditAnywhere, Category = "Palette")
	TArray<FSoftClassPath> FavoriteEnemyClasses;

	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	bool bHideOtherLayersInWavePreview = true;

	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	bool bShowApproximatePreviewBanner = true;
};
