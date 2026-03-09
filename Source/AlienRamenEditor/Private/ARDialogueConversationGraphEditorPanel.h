#pragma once

#include "CoreMinimal.h"
#include "ARDialogueTypes.h"
#include "GameplayTagContainer.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SMultiLineEditableTextBox;
class SEditableTextBox;
class SBox;
class SGraphEditor;
class SObjectPropertyEntryBox;
class UARDialogueConversationAsset;
class UARDialogueEdGraph;
class UARDialogueEdGraphNode;
struct FAssetData;
struct FDialogueClientView;
struct FDialogueValidationReport;

class SDialogueConversationGraphEditorPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDialogueConversationGraphEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	static void RequestOpenConversation(UARDialogueConversationAsset* Asset);

private:
	void LoadPendingConversationRequest();
	void AppendLogLine(const FString& Message);
	void SetSelectedConversation(UARDialogueConversationAsset* Asset);
	void RebuildGraphEditorWidget(class UEdGraph* GraphToEdit);
	FString GetSelectedConversationPath() const;
	void OnSelectedConversationChanged(const FAssetData& AssetData);
	void OnGraphSelectionChanged(const TSet<UObject*>& NewSelection);
	bool ParseInjectedVariables(const FString& SourceText, TMap<FName, FDialogueInjectedValue>& OutVariables, FString& OutError) const;

	FReply HandleRefresh();
	FReply HandleSave();
	FReply HandleValidate();
	FReply HandleCompile();
	FReply HandleFocusEnterNode();
	FReply HandleAutoLayout();
	FReply HandlePreviewConversation();

	bool EnsureConversationEditorGraph(UARDialogueConversationAsset* ConversationAsset);
	void RebuildEditorGraphFromCompiled(UARDialogueConversationAsset* ConversationAsset, UARDialogueEdGraph* Graph) const;
	bool CompileEditorGraphToRuntime(UARDialogueConversationAsset* ConversationAsset, FDialogueValidationReport& OutValidationReport) const;
	void ApplyValidationToEditorNodes(UARDialogueConversationAsset* ConversationAsset, const FDialogueValidationReport& ValidationReport) const;
	void ParseTagList(const FString& SourceText, FGameplayTagContainer& OutContainer) const;

	TSharedPtr<SObjectPropertyEntryBox> ConversationAssetPicker;
	TSharedPtr<SBox> GraphEditorHost;
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SMultiLineEditableTextBox> PreviewInjectedVariablesTextBox;
	TSharedPtr<SEditableTextBox> PreviewCombinedTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewPlayerTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewGameTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewTransientTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewLoadoutTagsTextBox;

	TWeakObjectPtr<UARDialogueConversationAsset> SelectedConversation;
	TWeakObjectPtr<UARDialogueEdGraph> SelectedEditorGraph;

	FString ValidationOutput;
	FString PreviewOutput;
	bool bPreviewAsBrother = true;
	float PreviewRelationshipPoints = 0.0f;
	float PreviewTimePlayed = 0.0f;
	int32 PreviewPlayerKills = 0;
	bool bPreviewSeenByPlayer = false;
	bool bPreviewSeenByGame = false;
	bool bPreviewCompletedByPlayer = false;
	bool bPreviewCompletedByGame = false;
};
