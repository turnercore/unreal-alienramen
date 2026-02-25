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

	// Per-enemy-class authoring shape used by the palette/canvas preview (0-based cycle value).
	UPROPERTY(Config, EditAnywhere, Category = "Palette")
	TMap<FSoftClassPath, int32> EnemyClassShapeCycles;

	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	bool bHideOtherLayersInWavePreview = true;

	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	bool bShowApproximatePreviewBanner = true;

	// Snap authored spawn offsets to a configurable grid when placing/dragging in canvas.
	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	bool bSnapCanvasToGrid = false;

	// Grid step size used by canvas snap and copy/paste offset nudging.
	UPROPERTY(Config, EditAnywhere, Category = "Preview", meta=(ClampMin="1.0", UIMin="1.0"))
	float CanvasGridSize = 100.f;
};
