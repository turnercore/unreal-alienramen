#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SEditableTextBox;
class SBox;
class SGraphEditor;
class STableViewBase;
class UARDialogueConversationAsset;
class UARDialogueEdGraph;
class UARDialogueEdGraphNode;
template <typename ItemType>
class SListView;
struct FDialogueClientView;
struct FDialogueValidationReport;

class SDialogueConversationGraphEditorPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDialogueConversationGraphEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FConversationEntry
	{
		FString Label;
		TWeakObjectPtr<UARDialogueConversationAsset> Asset;
	};

	void RefreshConversations();
	void AppendLogLine(const FString& Message);
	void SetSelectedConversation(UARDialogueConversationAsset* Asset);
	void RebuildGraphEditorWidget(class UEdGraph* GraphToEdit);

	TSharedRef<ITableRow> OnGenerateConversationRow(TSharedPtr<FConversationEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnConversationDoubleClicked(TSharedPtr<FConversationEntry> Item);
	void OnGraphSelectionChanged(const TSet<UObject*>& NewSelection);

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

	TSharedPtr<SListView<TSharedPtr<FConversationEntry>>> ConversationListView;
	TSharedPtr<SBox> GraphEditorHost;
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SEditableTextBox> PreviewCombinedTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewPlayerTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewGameTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewTransientTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewLoadoutTagsTextBox;
	TSharedPtr<SEditableTextBox> PreviewInjectedVariableNameTextBox;
	TSharedPtr<SEditableTextBox> PreviewInjectedVariableValueTextBox;

	TArray<TSharedPtr<FConversationEntry>> ConversationEntries;
	TWeakObjectPtr<UARDialogueConversationAsset> SelectedConversation;
	TWeakObjectPtr<UARDialogueEdGraph> SelectedEditorGraph;

	FString ValidationOutput;
	FString PreviewOutput;
	bool bPreviewAsBrother = true;
	float PreviewRelationshipPoints = 0.0f;
	float PreviewTimePlayed = 0.0f;
	int32 PreviewPlayerKills = 0;
	bool bPreviewCompletedByPlayer = false;
	bool bPreviewCompletedByGame = false;
};
