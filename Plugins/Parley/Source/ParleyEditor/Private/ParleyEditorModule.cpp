#include "ParleyEditorModule.h"

#include "ParleyDialogueConversationGraphEditorPanel.h"
#include "ParleyDialogueEdGraphNode.h"
#include "ParleyDialogueNodeDetailsCustomization.h"
#include "ParleyDialogueSpeakerEditorPanel.h"
#include "SParleyDialogueInlineGraphNode.h"
#include "SParleyDialogueLineGraphNode.h"

#include "EdGraphUtilities.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

namespace ParleyDialogueSpeakerEditor
{
	static const FName TabName(TEXT("AR_DialogueSpeakerEditor"));
}

namespace ParleyDialogueConversationGraphEditor
{
	static const FName TabName(TEXT("AR_DialogueConversationGraphEditor"));
}

IMPLEMENT_MODULE(FParleyEditorModule, ParleyEditor)

void FParleyEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditorModule.RegisterCustomClassLayout(
		UParleyDialogueEdGraphNode::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FParleyDialogueEdGraphNodeDetails::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(
		TEXT("DialogueLineNodeData"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FParleyDialogueLineNodeDataCustomization::MakeInstance));
	PropertyEditorModule.NotifyCustomizationModuleChanged();

	DialogueLineNodeFactory = CreateARDialogueLineGraphNodeFactory();
	FEdGraphUtilities::RegisterVisualNodeFactory(DialogueLineNodeFactory);
	DialogueInlineNodeFactory = CreateARDialogueInlineGraphNodeFactory();
	FEdGraphUtilities::RegisterVisualNodeFactory(DialogueInlineNodeFactory);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ParleyDialogueSpeakerEditor::TabName,
		FOnSpawnTab::CreateRaw(this, &FParleyEditorModule::SpawnDialogueSpeakerTab))
		.SetDisplayName(FText::FromString(TEXT("AR Dialogue Speaker Editor")))
		.SetTooltipText(FText::FromString(TEXT("Speaker-centric dialogue authoring hub.")))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ParleyDialogueConversationGraphEditor::TabName,
		FOnSpawnTab::CreateRaw(this, &FParleyEditorModule::SpawnDialogueConversationGraphTab))
		.SetDisplayName(FText::FromString(TEXT("AR Dialogue Conversation Graph Editor")))
		.SetTooltipText(FText::FromString(TEXT("Conversation graph validation, compile, and preview workflow.")))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FParleyEditorModule::RegisterMenus));
}

void FParleyEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.UnregisterCustomClassLayout(UParleyDialogueEdGraphNode::StaticClass()->GetFName());
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(TEXT("DialogueLineNodeData"));
		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}

	if (DialogueLineNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(DialogueLineNodeFactory);
		DialogueLineNodeFactory.Reset();
	}

	if (DialogueInlineNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(DialogueInlineNodeFactory);
		DialogueInlineNodeFactory.Reset();
	}

	if (UToolMenus::TryGet())
	{
		UToolMenus::UnRegisterStartupCallback(this);
	}

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ParleyDialogueSpeakerEditor::TabName);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ParleyDialogueConversationGraphEditor::TabName);
}

TSharedRef<SDockTab> FParleyEditorModule::SpawnDialogueSpeakerTab(const FSpawnTabArgs& SpawnTabArgs)
{
	(void)SpawnTabArgs;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDialogueSpeakerEditorPanel)
		];
}

TSharedRef<SDockTab> FParleyEditorModule::SpawnDialogueConversationGraphTab(const FSpawnTabArgs& SpawnTabArgs)
{
	(void)SpawnTabArgs;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDialogueConversationGraphEditorPanel)
		];
}

void FParleyEditorModule::RegisterMenus()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));
	Section.AddMenuEntry(
		TEXT("OpenARDialogueSpeakerEditor"),
		FText::FromString(TEXT("AR Dialogue Speaker Editor")),
		FText::FromString(TEXT("Open the dialogue speaker-centric authoring tab.")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"),
		FToolMenuExecuteAction::CreateRaw(this, &FParleyEditorModule::OpenDialogueSpeakerTab));
	Section.AddMenuEntry(
		TEXT("OpenARDialogueConversationGraphEditor"),
		FText::FromString(TEXT("AR Dialogue Conversation Graph Editor")),
		FText::FromString(TEXT("Open the conversation graph authoring and preview tab.")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"),
		FToolMenuExecuteAction::CreateRaw(this, &FParleyEditorModule::OpenDialogueConversationGraphTab));
}

void FParleyEditorModule::OpenDialogueSpeakerTab(const FToolMenuContext& ToolMenuContext)
{
	(void)ToolMenuContext;
	FGlobalTabmanager::Get()->TryInvokeTab(ParleyDialogueSpeakerEditor::TabName);
}

void FParleyEditorModule::OpenDialogueConversationGraphTab(const FToolMenuContext& ToolMenuContext)
{
	(void)ToolMenuContext;
	FGlobalTabmanager::Get()->TryInvokeTab(ParleyDialogueConversationGraphEditor::TabName);
}
