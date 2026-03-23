#pragma once

#include "CoreMinimal.h"
#include "ParleyDialogueTypes.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SBox;
class SGraphEditor;
class SObjectPropertyEntryBox;
class UParleyConversationAsset;
class UParleyDialogueEdGraph;
class UParleyDialogueEdGraphNode;
class FUICommandList;
struct FAssetData;
struct FDialogueValidationReport;
struct FGeometry;
struct FKeyEvent;
struct FInputChord;

class SDialogueConversationGraphEditorPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDialogueConversationGraphEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/// Clears editor-only validation cache so transient validation objects do not survive across PIE boundaries.
	static void ResetValidationSubsystemCache();
	static void RequestOpenConversation(UParleyConversationAsset* Asset);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	enum class EEditorStatusType : uint8
	{
		Info,
		Success,
		Warning,
		Error
	};

	void LoadPendingConversationRequest();
	void AppendLogLine(const FString& Message);
	void SetStatusMessage(const FString& Message, EEditorStatusType StatusType);
	void SetSelectedConversation(UParleyConversationAsset* Asset);
	void RebuildGraphEditorWidget(class UEdGraph* GraphToEdit);
	FString GetSelectedConversationPath() const;
	void OnSelectedConversationChanged(const FAssetData& AssetData);
	void OnGraphSelectionChanged(const TSet<UObject*>& NewSelection);
	bool HandleVerifyNodeTextCommit(const FText& NewText, UEdGraphNode* Node, FText& OutErrorMessage) const;
	void HandleNodeTextCommitted(const FText& NewText, ETextCommit::Type CommitType, UEdGraphNode* Node);
	FReply HandleSpawnNodeByShortcut(FInputChord InChord, const FVector2f& Location);
	void HandleCreateComment();
	bool CanCreateComment() const;
	void HandleRenameSelectedNode();
	bool CanRenameSelectedNode() const;

	FReply HandleRefresh();
	FReply HandleSave();
	FReply HandleValidate();
	FReply HandleCompile();
	void HandleCopySelectedNodes();
	bool CanCopySelectedNodes() const;
	void HandleCutSelectedNodes();
	bool CanCutSelectedNodes() const;
	void HandlePasteNodes();
	bool CanPasteNodes() const;
	void HandleDuplicateSelectedNodes();
	bool CanDuplicateSelectedNodes() const;
	void PasteNodesAtLocation(FVector2f Location);
	void HandleDeleteSelectedNodes();
	bool CanDeleteSelectedNodes() const;
	void ExecuteSaveCommand();
	void CreateCommentAtLocation(const FVector2f& Location);

	bool EnsureConversationEditorGraph(UParleyConversationAsset* ConversationAsset);
	bool CompileEditorGraphToRuntime(UParleyConversationAsset* ConversationAsset, FDialogueValidationReport& OutValidationReport) const;
	void ApplyValidationToEditorNodes(UParleyConversationAsset* ConversationAsset, const FDialogueValidationReport& ValidationReport) const;

	TSharedPtr<SObjectPropertyEntryBox> ConversationAssetPicker;
	TSharedPtr<SBox> GraphEditorHost;
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<FUICommandList> GraphEditorCommands;
	TSharedPtr<IDetailsView> DetailsView;

	TWeakObjectPtr<UParleyConversationAsset> SelectedConversation;
	TWeakObjectPtr<UParleyDialogueEdGraph> SelectedEditorGraph;

	FString ValidationOutput;
	FString StatusMessage;
	FLinearColor StatusColor = FLinearColor(0.68f, 0.68f, 0.68f, 1.0f);
};
