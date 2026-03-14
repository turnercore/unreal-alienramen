#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FGraphPanelNodeFactory;
class SDockTab;
class FSpawnTabArgs;
struct FToolMenuContext;

class FParleyEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<SDockTab> SpawnDialogueSpeakerTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SDockTab> SpawnDialogueConversationGraphTab(const FSpawnTabArgs& SpawnTabArgs);
	void RegisterMenus();
	void OpenDialogueSpeakerTab(const FToolMenuContext& ToolMenuContext);
	void OpenDialogueConversationGraphTab(const FToolMenuContext& ToolMenuContext);

	TSharedPtr<FGraphPanelNodeFactory> DialogueLineNodeFactory;
	TSharedPtr<FGraphPanelNodeFactory> DialogueInlineNodeFactory;
};
