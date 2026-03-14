#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ParleyDialogueEditorSettings.generated.h"

UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Dialogue Tooling"))
class PARLEYEDITOR_API UParleyDialogueEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UParleyDialogueEditorSettings();

	virtual FName GetCategoryName() const override { return TEXT("Alien Ramen"); }
	virtual FName GetSectionName() const override { return TEXT("Dialogue Tooling"); }

	// Long package path under /Game where Speaker Editor writes newly created conversation assets.
	UPROPERTY(Config, EditAnywhere, Category="Speaker Editor", meta = (ToolTip = "Content folder path where speaker editor tooling creates new conversation assets."))
	FDirectoryPath ConversationAssetsFolder;
};
