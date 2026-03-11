#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARDialogueEditorSettings.generated.h"

UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Dialogue Tooling"))
class ALIENRAMENEDITOR_API UARDialogueEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UARDialogueEditorSettings();

	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Dialogue - Tooling"); }

	// Long package path under /Game where Speaker Editor writes newly created conversation assets.
	UPROPERTY(Config, EditAnywhere, Category="Speaker Editor")
	FDirectoryPath ConversationAssetsFolder;
};
