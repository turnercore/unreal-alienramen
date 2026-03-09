#include "ARDialogueConversationGraphEditorPanel.h"

#include "ARDialogueConversationAsset.h"
#include "ARDialogueEdGraph.h"
#include "ARDialogueEdGraphNode.h"
#include "ARDialogueEdGraphSchema.h"
#include "ARDialogueSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Engine.h"
#include "EdGraphUtilities.h"
#include "FileHelpers.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Math/NumericLimits.h"

namespace
{
	static UARDialogueSubsystem* GetDialogueSubsystemFromPIE()
	{
		if (!GEditor)
		{
			return nullptr;
		}

		const FWorldContext* PIEContext = GEditor->GetPIEWorldContext();
		if (!PIEContext || !PIEContext->OwningGameInstance)
		{
			return nullptr;
		}

		return PIEContext->OwningGameInstance->GetSubsystem<UARDialogueSubsystem>();
	}

	static UARDialogueSubsystem* GetDialogueSubsystemForValidation()
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemFromPIE())
		{
			return DialogueSubsystem;
		}

		static TWeakObjectPtr<UARDialogueSubsystem> Cached;
		if (!Cached.IsValid())
		{
			Cached = NewObject<UARDialogueSubsystem>(GetTransientPackage());
		}
		return Cached.Get();
	}

	static void AddValidationIssue(
		FDialogueValidationReport& OutReport,
		const EDialogueValidationSeverity Severity,
		const FGuid& NodeId,
		const FString& Message)
	{
		FDialogueValidationIssue& Issue = OutReport.Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.NodeId = NodeId;
		Issue.Message = FText::FromString(Message);
	}

	static void SyncDialogueGraphPins(UARDialogueEdGraph* Graph)
	{
		if (!Graph)
		{
			return;
		}

		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
			if (!DialogueNode)
			{
				continue;
			}

			DialogueNode->EnsureStableIds(false, false);
			DialogueNode->ReconstructNode();
		}

		Graph->NotifyGraphChanged();
	}

	static TWeakObjectPtr<UARDialogueConversationAsset> GPendingConversationToEdit;
}

void SDialogueConversationGraphEditorPanel::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bLockable = false;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bShowObjectLabel = false;
	DetailsArgs.bShowLooseProperties = true;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);

	FGenericCommands::Register();
	GraphEditorCommands = MakeShared<FUICommandList>();
	FGraphEditorCommands::Register();
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleCopySelectedNodes),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanCopySelectedNodes));
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleCutSelectedNodes),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanCutSelectedNodes));
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandlePasteNodes),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanPasteNodes));
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleDuplicateSelectedNodes),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanDuplicateSelectedNodes));
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleDeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanDeleteSelectedNodes));
	GraphEditorCommands->MapAction(
		FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleCreateComment),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanCreateComment));
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleRenameSelectedNode),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanRenameSelectedNode));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 2.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Asset")))
				.ToolTipText(FText::FromString(TEXT("Conversation asset currently opened in this editor panel.")))
			]
			+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(2.0f)
			[
				SAssignNew(ConversationAssetPicker, SObjectPropertyEntryBox)
				.AllowedClass(UARDialogueConversationAsset::StaticClass())
				.ObjectPath(this, &SDialogueConversationGraphEditorPanel::GetSelectedConversationPath)
				.OnObjectChanged(this, &SDialogueConversationGraphEditorPanel::OnSelectedConversationChanged)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SBox)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Refresh")))
				.ToolTipText(FText::FromString(TEXT("Reloads the selected conversation and rebuilds the graph/details views from the current asset state.")))
				.OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleRefresh)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Validate")))
				.ToolTipText(FText::FromString(TEXT("Compiles the current graph to runtime data in-memory and runs validation checks. Does not save the asset package.")))
				.OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleValidate)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Compile")))
				.ToolTipText(FText::FromString(TEXT("Builds compile-managed runtime node/link data from editor graph pins and writes it back to the conversation asset.")))
				.OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleCompile)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.66f)
			[
				SNew(SBorder)
				.Padding(2.0f)
				[
					SAssignNew(GraphEditorHost, SBox)
				]
			]
			+ SSplitter::Slot().Value(0.30f)
			[
				SNew(SBorder)
				.Padding(4.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(0.72f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						DetailsView.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Validation / Compile Output")))
						.ToolTipText(FText::FromString(TEXT("Action log with validation issues, compile status, and save flow feedback.")))
					]
					+ SVerticalBox::Slot().FillHeight(0.28f)
					[
						SNew(SBorder)
						.Padding(4.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return FText::FromString(ValidationOutput); })
								.AutoWrapText(true)
							]
						]
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 0.0f, 8.0f, 6.0f)
		.HAlign(HAlign_Right)
		[
			SNew(SBorder)
			.Padding(FMargin(8.0f, 4.0f))
			.ToolTipText(FText::FromString(TEXT("Latest action status for this panel (info, warning, or error).")))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(StatusMessage); })
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(StatusColor); })
			]
		]
	];

	RebuildGraphEditorWidget(nullptr);
	LoadPendingConversationRequest();
	SetStatusMessage(TEXT("Ready."), EEditorStatusType::Info);
}

void SDialogueConversationGraphEditorPanel::RequestOpenConversation(UARDialogueConversationAsset* Asset)
{
	GPendingConversationToEdit = Asset;
}

FReply SDialogueConversationGraphEditorPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::S && (InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown()))
	{
		ExecuteSaveCommand();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::D
		&& (InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown())
		&& !InKeyEvent.IsAltDown()
		&& !InKeyEvent.IsShiftDown())
	{
		if (CanDuplicateSelectedNodes())
		{
			HandleDuplicateSelectedNodes();
			return FReply::Handled();
		}
	}
	if ((InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace) && !InKeyEvent.IsControlDown() && !InKeyEvent.IsCommandDown())
	{
		if (CanDeleteSelectedNodes())
		{
			HandleDeleteSelectedNodes();
			return FReply::Handled();
		}
	}
	if (GraphEditorCommands.IsValid() && GraphEditorCommands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SDialogueConversationGraphEditorPanel::LoadPendingConversationRequest()
{
	if (GPendingConversationToEdit.IsValid())
	{
		SetSelectedConversation(GPendingConversationToEdit.Get());
		GPendingConversationToEdit.Reset();
	}
}

void SDialogueConversationGraphEditorPanel::AppendLogLine(const FString& Message)
{
	if (!ValidationOutput.IsEmpty())
	{
		ValidationOutput += TEXT("\n");
	}
	ValidationOutput += Message;
}

void SDialogueConversationGraphEditorPanel::SetStatusMessage(const FString& Message, const EEditorStatusType StatusType)
{
	StatusMessage = Message;
	switch (StatusType)
	{
	case EEditorStatusType::Success:
		StatusColor = FLinearColor(0.21f, 0.8f, 0.4f, 1.0f);
		break;
	case EEditorStatusType::Warning:
		StatusColor = FLinearColor(0.92f, 0.75f, 0.2f, 1.0f);
		break;
	case EEditorStatusType::Error:
		StatusColor = FLinearColor(0.9f, 0.28f, 0.28f, 1.0f);
		break;
	case EEditorStatusType::Info:
	default:
		StatusColor = FLinearColor(0.68f, 0.68f, 0.68f, 1.0f);
		break;
	}
}

void SDialogueConversationGraphEditorPanel::SetSelectedConversation(UARDialogueConversationAsset* Asset)
{
	SelectedConversation = Asset;
	SelectedEditorGraph.Reset();

	if (!Asset)
	{
		RebuildGraphEditorWidget(nullptr);
		if (DetailsView.IsValid())
		{
			DetailsView->SetObject(nullptr);
		}
		SetStatusMessage(TEXT("No conversation selected."), EEditorStatusType::Info);
		return;
	}

	if (!EnsureConversationEditorGraph(Asset))
	{
		AppendLogLine(TEXT("Failed to initialize editor graph for selected conversation."));
		SetStatusMessage(TEXT("Failed to initialize editor graph."), EEditorStatusType::Error);
		return;
	}

	SelectedEditorGraph = Cast<UARDialogueEdGraph>(Asset->EditorGraph);
	SyncDialogueGraphPins(SelectedEditorGraph.Get());
	RebuildGraphEditorWidget(SelectedEditorGraph.Get());
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Asset);
	}
	SetStatusMessage(FString::Printf(TEXT("Editing '%s'."), *Asset->GetName()), EEditorStatusType::Info);
}

void SDialogueConversationGraphEditorPanel::RebuildGraphEditorWidget(UEdGraph* GraphToEdit)
{
	if (!GraphEditorHost.IsValid())
	{
		return;
	}

	GraphEditorWidget.Reset();
	if (!IsValid(GraphToEdit))
	{
		GraphEditorHost->SetContent(
			SNew(SBorder)
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("No conversation selected.")))
				.AutoWrapText(true)
			]);
		return;
	}

	FGraphAppearanceInfo GraphAppearance;
	GraphAppearance.CornerText = FText::FromString(TEXT("Dialogue Conversation Graph"));

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SDialogueConversationGraphEditorPanel::OnGraphSelectionChanged);
	GraphEvents.OnSpawnNodeByShortcutAtLocation = SGraphEditor::FOnSpawnNodeByShortcutAtLocation::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleSpawnNodeByShortcut);

	SAssignNew(GraphEditorWidget, SGraphEditor)
		.Appearance(GraphAppearance)
		.GraphToEdit(GraphToEdit)
		.IsEditable(true)
		.ShowGraphStateOverlay(false)
		.AdditionalCommands(GraphEditorCommands)
		.GraphEvents(GraphEvents);

	if (GraphEditorWidget.IsValid())
	{
		GraphEditorHost->SetContent(GraphEditorWidget.ToSharedRef());
	}
}

FString SDialogueConversationGraphEditorPanel::GetSelectedConversationPath() const
{
	return SelectedConversation.IsValid() ? SelectedConversation->GetPathName() : FString();
}

void SDialogueConversationGraphEditorPanel::OnSelectedConversationChanged(const FAssetData& AssetData)
{
	if (!AssetData.IsValid())
	{
		SetSelectedConversation(nullptr);
		return;
	}

	SetSelectedConversation(Cast<UARDialogueConversationAsset>(AssetData.GetAsset()));
}

void SDialogueConversationGraphEditorPanel::OnGraphSelectionChanged(const TSet<UObject*>& NewSelection)
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	if (NewSelection.Num() == 1)
	{
		if (UObject* SelectedObject = *NewSelection.CreateConstIterator())
		{
			if (Cast<UEdGraphNode>(SelectedObject))
			{
				DetailsView->SetObject(SelectedObject);
				return;
			}
		}
	}

	DetailsView->SetObject(SelectedConversation.Get());
}

FReply SDialogueConversationGraphEditorPanel::HandleSpawnNodeByShortcut(FInputChord InChord, const FVector2f& Location)
{
	if (InChord.Key != EKeys::R && InChord.Key != EKeys::C)
	{
		return FReply::Unhandled();
	}

	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph)
	{
		return FReply::Unhandled();
	}

	if (InChord.Key == EKeys::R)
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Add Dialogue Route Node")));
		Graph->Modify();

		UARDialogueEdGraphNode* RouteNode = NewObject<UARDialogueEdGraphNode>(Graph);
		RouteNode->SetFlags(RF_Transactional);
		RouteNode->InitializeForNodeType(EDialogueNodeType::Route);
		RouteNode->NodePosX = static_cast<int32>(Location.X);
		RouteNode->NodePosY = static_cast<int32>(Location.Y);
		RouteNode->CreateNewGuid();
		RouteNode->PostPlacedNewNode();
		RouteNode->AllocateDefaultPins();
		Graph->AddNode(RouteNode, true, true);

		if (GraphEditorWidget.IsValid())
		{
			if (UEdGraphPin* DragPin = GraphEditorWidget->GetGraphPinForMenu())
			{
				RouteNode->AutowireNewNode(DragPin);
			}
		}

		Graph->NotifyGraphChanged();
		if (UARDialogueConversationAsset* Conversation = SelectedConversation.Get())
		{
			Conversation->MarkPackageDirty();
		}

		SetStatusMessage(TEXT("Route node added."), EEditorStatusType::Info);
		return FReply::Handled();
	}

	// C: Add a comment node. If nodes are selected, wrap around their bounds.
	CreateCommentAtLocation(Location);
	return FReply::Handled();
}

void SDialogueConversationGraphEditorPanel::HandleCreateComment()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	CreateCommentAtLocation(FVector2f(GraphEditorWidget->GetPasteLocation2f()));
}

bool SDialogueConversationGraphEditorPanel::CanCreateComment() const
{
	return SelectedEditorGraph.IsValid() && GraphEditorWidget.IsValid();
}

void SDialogueConversationGraphEditorPanel::HandleRenameSelectedNode()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		return;
	}

	UObject* SelectedObject = *SelectedNodes.CreateConstIterator();
	UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
	if (!SelectedNode)
	{
		return;
	}

	GraphEditorWidget->IsNodeTitleVisible(SelectedNode, true);
}

bool SDialogueConversationGraphEditorPanel::CanRenameSelectedNode() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		return false;
	}

	const UObject* SelectedObject = *SelectedNodes.CreateConstIterator();
	return Cast<UEdGraphNode>(SelectedObject) != nullptr;
}

void SDialogueConversationGraphEditorPanel::CreateCommentAtLocation(const FVector2f& Location)
{
	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph)
	{
		return;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Add Dialogue Comment")));
	Graph->Modify();

	UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(Graph);
	CommentNode->SetFlags(RF_Transactional);
	CommentNode->CreateNewGuid();
	CommentNode->NodeComment = TEXT("Comment");
	CommentNode->NodePosX = static_cast<int32>(Location.X);
	CommentNode->NodePosY = static_cast<int32>(Location.Y);
	CommentNode->NodeWidth = 360;
	CommentNode->NodeHeight = 180;
	CommentNode->SnapToGrid(16);
	Graph->AddNode(CommentNode, true, true);

	if (GraphEditorWidget.IsValid())
	{
		const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
		int32 MinX = TNumericLimits<int32>::Max();
		int32 MinY = TNumericLimits<int32>::Max();
		int32 MaxX = TNumericLimits<int32>::Lowest();
		int32 MaxY = TNumericLimits<int32>::Lowest();
		int32 WrappedCount = 0;
		for (UObject* SelectedObject : SelectedNodes)
		{
			UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
			if (!SelectedNode || SelectedNode == CommentNode)
			{
				continue;
			}

			const int32 NodeWidth = FMath::Max(80, SelectedNode->NodeWidth);
			const int32 NodeHeight = FMath::Max(60, SelectedNode->NodeHeight);
			MinX = FMath::Min(MinX, SelectedNode->NodePosX);
			MinY = FMath::Min(MinY, SelectedNode->NodePosY);
			MaxX = FMath::Max(MaxX, SelectedNode->NodePosX + NodeWidth);
			MaxY = FMath::Max(MaxY, SelectedNode->NodePosY + NodeHeight);
			++WrappedCount;
		}

		if (WrappedCount > 0)
		{
			constexpr int32 CommentPadding = 48;
			CommentNode->NodePosX = MinX - CommentPadding;
			CommentNode->NodePosY = MinY - CommentPadding;
			CommentNode->NodeWidth = FMath::Max(220, (MaxX - MinX) + (CommentPadding * 2));
			CommentNode->NodeHeight = FMath::Max(140, (MaxY - MinY) + (CommentPadding * 2));
			CommentNode->SnapToGrid(16);
		}

		GraphEditorWidget->ClearSelectionSet();
		GraphEditorWidget->SetNodeSelection(CommentNode, true);
	}

	Graph->NotifyGraphChanged();
	if (UARDialogueConversationAsset* Conversation = SelectedConversation.Get())
	{
		Conversation->MarkPackageDirty();
	}

	SetStatusMessage(TEXT("Comment node added."), EEditorStatusType::Info);
}

FReply SDialogueConversationGraphEditorPanel::HandleRefresh()
{
	if (SelectedConversation.IsValid())
	{
		SetSelectedConversation(SelectedConversation.Get());
		SetStatusMessage(TEXT("Refresh complete."), EEditorStatusType::Info);
	}
	else
	{
		LoadPendingConversationRequest();
		SetStatusMessage(TEXT("Refresh complete (no conversation selected)."), EEditorStatusType::Info);
	}
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleSave()
{
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		SetStatusMessage(TEXT("Save failed: no conversation selected."), EEditorStatusType::Error);
		return FReply::Handled();
	}

	FDialogueValidationReport ValidationReport;
	const bool bCompiled = CompileEditorGraphToRuntime(Conversation, ValidationReport);
	const bool bHasErrors = ValidationReport.HasErrors();
	Conversation->LastCompileValidation = ValidationReport;
	Conversation->bLastCompileSucceeded = bCompiled;
	Conversation->CompileVersion += 1;
	ApplyValidationToEditorNodes(Conversation, ValidationReport);

	Conversation->MarkPackageDirty();
	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(Conversation->GetOutermost());
	const FEditorFileUtils::EPromptReturnCode SaveResult = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);

	AppendLogLine(FString::Printf(
		TEXT("Save requested for '%s' | Compile: %s | Version: %d"),
		*Conversation->GetName(),
		bCompiled ? TEXT("SUCCESS") : TEXT("FAILED"),
		Conversation->CompileVersion));
	for (const FDialogueValidationIssue& Issue : ValidationReport.Issues)
	{
		const TCHAR* SeverityLabel = TEXT("INFO");
		if (Issue.Severity == EDialogueValidationSeverity::Warning)
		{
			SeverityLabel = TEXT("WARN");
		}
		else if (Issue.Severity == EDialogueValidationSeverity::Error)
		{
			SeverityLabel = TEXT("ERROR");
		}
		AppendLogLine(FString::Printf(TEXT("[%s] %s"), SeverityLabel, *Issue.Message.ToString()));
	}

	switch (SaveResult)
	{
	case FEditorFileUtils::PR_Success:
		if (bHasErrors || !bCompiled)
		{
			SetStatusMessage(
				FString::Printf(TEXT("Saved '%s' with compile/validation issues."), *Conversation->GetName()),
				EEditorStatusType::Warning);
		}
		else if (ValidationReport.Issues.Num() > 0)
		{
			SetStatusMessage(
				FString::Printf(TEXT("Save successful with warnings: '%s' (Compile Version %d)."), *Conversation->GetName(), Conversation->CompileVersion),
				EEditorStatusType::Warning);
		}
		else
		{
			SetStatusMessage(
				FString::Printf(TEXT("Save successful: '%s' (Compile Version %d)."), *Conversation->GetName(), Conversation->CompileVersion),
				EEditorStatusType::Success);
		}
		break;
	case FEditorFileUtils::PR_Declined:
		SetStatusMessage(TEXT("Save declined."), EEditorStatusType::Warning);
		break;
	case FEditorFileUtils::PR_Cancelled:
		SetStatusMessage(TEXT("Save cancelled."), EEditorStatusType::Warning);
		break;
	case FEditorFileUtils::PR_Failure:
	default:
		SetStatusMessage(TEXT("Save failed."), EEditorStatusType::Error);
		break;
	}

	return FReply::Handled();
}

void SDialogueConversationGraphEditorPanel::ExecuteSaveCommand()
{
	HandleSave();
}

FReply SDialogueConversationGraphEditorPanel::HandleValidate()
{
	ValidationOutput.Empty();
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		SetStatusMessage(TEXT("Validation failed: no conversation selected."), EEditorStatusType::Error);
		return FReply::Handled();
	}

	FDialogueValidationReport ValidationReport;
	const bool bValid = CompileEditorGraphToRuntime(Conversation, ValidationReport);
	ApplyValidationToEditorNodes(Conversation, ValidationReport);

	AppendLogLine(bValid ? TEXT("Validation succeeded.") : TEXT("Validation failed."));
	for (const FDialogueValidationIssue& Issue : ValidationReport.Issues)
	{
		const TCHAR* SeverityLabel = TEXT("INFO");
		if (Issue.Severity == EDialogueValidationSeverity::Warning)
		{
			SeverityLabel = TEXT("WARN");
		}
		else if (Issue.Severity == EDialogueValidationSeverity::Error)
		{
			SeverityLabel = TEXT("ERROR");
		}

		AppendLogLine(FString::Printf(TEXT("[%s] %s"), SeverityLabel, *Issue.Message.ToString()));
	}

	if (!bValid)
	{
		SetStatusMessage(
			FString::Printf(TEXT("Validation failed (%d issue(s))."), ValidationReport.Issues.Num()),
			EEditorStatusType::Error);
	}
	else if (ValidationReport.Issues.Num() > 0)
	{
		SetStatusMessage(
			FString::Printf(TEXT("Validation successful with warnings (%d issue(s))."), ValidationReport.Issues.Num()),
			EEditorStatusType::Warning);
	}
	else
	{
		SetStatusMessage(TEXT("Validation successful."), EEditorStatusType::Success);
	}
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleCompile()
{
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		SetStatusMessage(TEXT("Compile failed: no conversation selected."), EEditorStatusType::Error);
		return FReply::Handled();
	}

	FDialogueValidationReport ValidationReport;
	const bool bCompiled = CompileEditorGraphToRuntime(Conversation, ValidationReport);
	const bool bHasErrors = ValidationReport.HasErrors();

	Conversation->LastCompileValidation = ValidationReport;
	Conversation->bLastCompileSucceeded = bCompiled;
	Conversation->CompileVersion += 1;
	Conversation->MarkPackageDirty();

	ApplyValidationToEditorNodes(Conversation, ValidationReport);
	AppendLogLine(FString::Printf(TEXT("Compile: %s (Version %d)"),
		bCompiled ? TEXT("SUCCESS") : TEXT("FAILED"),
		Conversation->CompileVersion));
	for (const FDialogueValidationIssue& Issue : ValidationReport.Issues)
	{
		const TCHAR* SeverityLabel = TEXT("INFO");
		if (Issue.Severity == EDialogueValidationSeverity::Warning)
		{
			SeverityLabel = TEXT("WARN");
		}
		else if (Issue.Severity == EDialogueValidationSeverity::Error)
		{
			SeverityLabel = TEXT("ERROR");
		}
		AppendLogLine(FString::Printf(TEXT("[%s] %s"), SeverityLabel, *Issue.Message.ToString()));
	}

	if (!bCompiled || bHasErrors)
	{
		SetStatusMessage(
			FString::Printf(TEXT("Compile failed (Version %d)."), Conversation->CompileVersion),
			EEditorStatusType::Error);
	}
	else if (ValidationReport.Issues.Num() > 0)
	{
		SetStatusMessage(
			FString::Printf(TEXT("Compile successful with warnings (Version %d)."), Conversation->CompileVersion),
			EEditorStatusType::Warning);
	}
	else
	{
		SetStatusMessage(
			FString::Printf(TEXT("Compile successful (Version %d)."), Conversation->CompileVersion),
			EEditorStatusType::Success);
	}
	return FReply::Handled();
}

void SDialogueConversationGraphEditorPanel::HandleCopySelectedNodes()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	TSet<UObject*> NodesToCopy;
	for (UObject* SelectedObject : SelectedNodes)
	{
		UEdGraphNode* GraphNode = Cast<UEdGraphNode>(SelectedObject);
		if (!GraphNode || !GraphNode->CanDuplicateNode())
		{
			continue;
		}

		const UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
		if (DialogueNode && DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
		{
			continue;
		}

		GraphNode->PrepareForCopying();
		NodesToCopy.Add(GraphNode);
	}

	if (NodesToCopy.IsEmpty())
	{
		return;
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
	SetStatusMessage(FString::Printf(TEXT("Copied %d node(s)."), NodesToCopy.Num()), EEditorStatusType::Info);
}

bool SDialogueConversationGraphEditorPanel::CanCopySelectedNodes() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	for (UObject* SelectedObject : SelectedNodes)
	{
		const UEdGraphNode* GraphNode = Cast<UEdGraphNode>(SelectedObject);
		if (!GraphNode || !GraphNode->CanDuplicateNode())
		{
			continue;
		}

		const UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
		if (DialogueNode && DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
		{
			continue;
		}

		return true;
	}

	return false;
}

void SDialogueConversationGraphEditorPanel::HandleCutSelectedNodes()
{
	if (!CanCutSelectedNodes())
	{
		return;
	}

	HandleCopySelectedNodes();
	HandleDeleteSelectedNodes();
}

bool SDialogueConversationGraphEditorPanel::CanCutSelectedNodes() const
{
	return CanCopySelectedNodes() && CanDeleteSelectedNodes();
}

void SDialogueConversationGraphEditorPanel::HandlePasteNodes()
{
	if (!CanPasteNodes())
	{
		return;
	}

	const FVector2f PasteLocation = GraphEditorWidget.IsValid() ? FVector2f(GraphEditorWidget->GetPasteLocation2f()) : FVector2f::ZeroVector;
	PasteNodesAtLocation(PasteLocation);
}

bool SDialogueConversationGraphEditorPanel::CanPasteNodes() const
{
	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph)
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	return FEdGraphUtilities::CanImportNodesFromText(Graph, ClipboardContent);
}

void SDialogueConversationGraphEditorPanel::HandleDuplicateSelectedNodes()
{
	if (!CanDuplicateSelectedNodes())
	{
		return;
	}

	const FVector2f PasteLocation = GraphEditorWidget.IsValid() ? FVector2f(GraphEditorWidget->GetPasteLocation2f()) : FVector2f::ZeroVector;
	HandleCopySelectedNodes();
	PasteNodesAtLocation(PasteLocation);
}

bool SDialogueConversationGraphEditorPanel::CanDuplicateSelectedNodes() const
{
	return CanCopySelectedNodes() && SelectedEditorGraph.IsValid() && GraphEditorWidget.IsValid();
}

void SDialogueConversationGraphEditorPanel::PasteNodesAtLocation(const FVector2f Location)
{
	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph || !GraphEditorWidget.IsValid())
	{
		return;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	if (!FEdGraphUtilities::CanImportNodesFromText(Graph, ClipboardContent))
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	Graph->Modify();

	GraphEditorWidget->ClearSelectionSet();

	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(Graph, ClipboardContent, PastedNodes);
	if (PastedNodes.IsEmpty())
	{
		return;
	}

	FVector2f AveragePosition = FVector2f::ZeroVector;
	int32 AverageCount = 0;
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		if (!PastedNode)
		{
			continue;
		}

		AveragePosition.X += PastedNode->NodePosX;
		AveragePosition.Y += PastedNode->NodePosY;
		++AverageCount;
	}
	if (AverageCount > 0)
	{
		AveragePosition /= static_cast<float>(AverageCount);
	}

	int32 PastedCount = 0;
	int32 SkippedEnterCount = 0;
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		if (!PastedNode)
		{
			continue;
		}

		if (UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(PastedNode))
		{
			if (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
			{
				DialogueNode->Modify();
				DialogueNode->DestroyNode();
				++SkippedEnterCount;
				continue;
			}

			DialogueNode->EnsureStableIds(true, true);
			DialogueNode->ReconstructNode();
		}

		PastedNode->Modify();
		PastedNode->NodePosX = static_cast<int32>((PastedNode->NodePosX - AveragePosition.X) + Location.X);
		PastedNode->NodePosY = static_cast<int32>((PastedNode->NodePosY - AveragePosition.Y) + Location.Y);
		PastedNode->SnapToGrid(16);
		GraphEditorWidget->SetNodeSelection(PastedNode, true);
		++PastedCount;
	}

	Graph->NotifyGraphChanged();
	if (UARDialogueConversationAsset* Conversation = SelectedConversation.Get())
	{
		Conversation->MarkPackageDirty();
	}

	if (SkippedEnterCount > 0)
	{
		SetStatusMessage(
			FString::Printf(TEXT("Pasted %d node(s). Skipped %d Enter node(s)."), PastedCount, SkippedEnterCount),
			EEditorStatusType::Warning);
	}
	else
	{
		SetStatusMessage(FString::Printf(TEXT("Pasted %d node(s)."), PastedCount), EEditorStatusType::Success);
	}
}

void SDialogueConversationGraphEditorPanel::HandleDeleteSelectedNodes()
{
	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph || !GraphEditorWidget.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Delete Dialogue Node")));
	Graph->Modify();

	int32 DeletedCount = 0;
	int32 SkippedEnterCount = 0;
	for (UObject* SelectedObject : SelectedNodes)
	{
		UEdGraphNode* GraphNode = Cast<UEdGraphNode>(SelectedObject);
		if (!GraphNode)
		{
			continue;
		}

		if (UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
			DialogueNode && DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
		{
			++SkippedEnterCount;
			continue;
		}

		if (GraphNode->CanUserDeleteNode())
		{
			GraphNode->Modify();
			GraphNode->DestroyNode();
			++DeletedCount;
		}
	}

	if (DeletedCount > 0)
	{
		Graph->NotifyGraphChanged();
		if (UARDialogueConversationAsset* Conversation = SelectedConversation.Get())
		{
			Conversation->MarkPackageDirty();
		}

		SetStatusMessage(FString::Printf(TEXT("Deleted %d node(s)."), DeletedCount), EEditorStatusType::Success);
		AppendLogLine(FString::Printf(TEXT("Deleted %d node(s)."), DeletedCount));
	}
	else if (SkippedEnterCount > 0)
	{
		SetStatusMessage(TEXT("Enter node cannot be deleted."), EEditorStatusType::Warning);
		AppendLogLine(TEXT("Delete skipped: Enter node cannot be deleted."));
	}
}

bool SDialogueConversationGraphEditorPanel::CanDeleteSelectedNodes() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	for (UObject* SelectedObject : SelectedNodes)
	{
		const UEdGraphNode* GraphNode = Cast<UEdGraphNode>(SelectedObject);
		if (!GraphNode || !GraphNode->CanUserDeleteNode())
		{
			continue;
		}

		const UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
		if (!DialogueNode || DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::Enter)
		{
			return true;
		}
	}

	return false;
}

bool SDialogueConversationGraphEditorPanel::EnsureConversationEditorGraph(UARDialogueConversationAsset* ConversationAsset)
{
	if (!ConversationAsset)
	{
		return false;
	}

	UARDialogueEdGraph* ExistingGraph = Cast<UARDialogueEdGraph>(ConversationAsset->EditorGraph);
	if (ExistingGraph && ExistingGraph->Schema == UARDialogueEdGraphSchema::StaticClass())
	{
		if (ExistingGraph->Nodes.IsEmpty())
		{
			RebuildEditorGraphFromCompiled(ConversationAsset, ExistingGraph);
		}
		return true;
	}

	ConversationAsset->Modify();
	UARDialogueEdGraph* NewGraph = NewObject<UARDialogueEdGraph>(ConversationAsset, NAME_None, RF_Transactional);
	NewGraph->Schema = UARDialogueEdGraphSchema::StaticClass();
	ConversationAsset->EditorGraph = NewGraph;
	RebuildEditorGraphFromCompiled(ConversationAsset, NewGraph);
	ConversationAsset->MarkPackageDirty();
	return true;
}

void SDialogueConversationGraphEditorPanel::RebuildEditorGraphFromCompiled(UARDialogueConversationAsset* ConversationAsset, UARDialogueEdGraph* Graph) const
{
	if (!ConversationAsset || !Graph)
	{
		return;
	}

	Graph->Modify();
	for (int32 Index = Graph->Nodes.Num() - 1; Index >= 0; --Index)
	{
		if (UEdGraphNode* ExistingNode = Graph->Nodes[Index])
		{
			Graph->RemoveNode(ExistingNode);
		}
	}

	TMap<FGuid, UARDialogueEdGraphNode*> NodeById;
	int32 NodeIndex = 0;
	for (const FDialogueCompiledNode& RuntimeNode : ConversationAsset->CompiledData.Nodes)
	{
		UARDialogueEdGraphNode* GraphNode = NewObject<UARDialogueEdGraphNode>(Graph);
		GraphNode->SetFlags(RF_Transactional);
		GraphNode->RuntimeNode = RuntimeNode;
		GraphNode->EnsureStableIds(false, false);
		GraphNode->NodePosX = (NodeIndex % 4) * 420;
		GraphNode->NodePosY = (NodeIndex / 4) * 220;
		GraphNode->CreateNewGuid();
		GraphNode->AllocateDefaultPins();
		Graph->AddNode(GraphNode, true, false);
		NodeById.Add(GraphNode->RuntimeNode.NodeId, GraphNode);
		++NodeIndex;
	}

	auto LinkPinToNode = [&NodeById](UEdGraphPin* SourcePin, const FGuid& TargetNodeId)
	{
		if (!SourcePin || !TargetNodeId.IsValid())
		{
			return;
		}

		UARDialogueEdGraphNode* TargetNode = NodeById.FindRef(TargetNodeId);
		if (!TargetNode)
		{
			return;
		}

		UEdGraphPin* TargetInput = TargetNode->GetExecInputPin();
		if (!TargetInput)
		{
			return;
		}

		SourcePin->MakeLinkTo(TargetInput);
	};

	for (const TPair<FGuid, UARDialogueEdGraphNode*>& Pair : NodeById)
	{
		UARDialogueEdGraphNode* SourceNode = Pair.Value;
		if (!SourceNode)
		{
			continue;
		}

		const FDialogueCompiledNode& RuntimeNode = SourceNode->RuntimeNode;
		switch (RuntimeNode.NodeType)
		{
		case EDialogueNodeType::Enter:
		case EDialogueNodeType::Line:
		case EDialogueNodeType::MultiLine:
		case EDialogueNodeType::TagMutation:
		case EDialogueNodeType::RelationshipMutation:
		case EDialogueNodeType::FactionMutation:
		case EDialogueNodeType::Route:
			LinkPinToNode(SourceNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameNext()), RuntimeNode.NextNodeId);
			break;
		case EDialogueNodeType::Bool:
			LinkPinToNode(SourceNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameTrue()), RuntimeNode.TrueNodeId);
			LinkPinToNode(SourceNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameFalse()), RuntimeNode.FalseNodeId);
			break;
		case EDialogueNodeType::Choice:
			for (const FDialogueCompiledChoiceBranch& Branch : RuntimeNode.ChoiceBranches)
			{
				LinkPinToNode(SourceNode->GetChoiceOutputPin(Branch.ChoiceBranchId), Branch.NextNodeId);
			}
			LinkPinToNode(SourceNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameFallback()), RuntimeNode.FallbackNodeId);
			break;
		case EDialogueNodeType::SwitchOnTagsByPriority:
			for (const FDialogueCompiledSwitchBranch& Branch : RuntimeNode.SwitchBranches)
			{
				LinkPinToNode(SourceNode->GetSwitchOutputPin(Branch.BranchId), Branch.NextNodeId);
			}
			if (RuntimeNode.bSwitchHasDefaultOutput)
			{
				LinkPinToNode(SourceNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameSwitchDefault()), RuntimeNode.SwitchDefaultNodeId);
			}
			break;
		case EDialogueNodeType::Random:
			for (const FDialogueCompiledRandomBranch& Branch : RuntimeNode.RandomBranches)
			{
				LinkPinToNode(SourceNode->GetRandomOutputPin(Branch.BranchId), Branch.NextNodeId);
			}
			break;
		case EDialogueNodeType::Sequence:
			for (const FDialogueCompiledSequenceBranch& Branch : RuntimeNode.SequenceBranches)
			{
				LinkPinToNode(SourceNode->GetSequenceOutputPin(Branch.BranchId), Branch.NextNodeId);
			}
			break;
		default:
			break;
		}
	}

	if (Graph->Nodes.IsEmpty())
	{
		if (const UEdGraphSchema* Schema = Graph->GetSchema())
		{
			Schema->CreateDefaultNodesForGraph(*Graph);
		}
	}

	Graph->NotifyGraphChanged();
}

bool SDialogueConversationGraphEditorPanel::CompileEditorGraphToRuntime(UARDialogueConversationAsset* ConversationAsset, FDialogueValidationReport& OutValidationReport) const
{
	OutValidationReport = FDialogueValidationReport();
	if (!ConversationAsset)
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("No conversation selected for compile."));
		return false;
	}

	UARDialogueEdGraph* Graph = Cast<UARDialogueEdGraph>(ConversationAsset->EditorGraph);
	if (!Graph)
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("Conversation has no editor graph."));
		return false;
	}

	TArray<UARDialogueEdGraphNode*> EditorNodes;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		if (UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode))
		{
			DialogueNode->EnsureStableIds(false, false);
			EditorNodes.Add(DialogueNode);
		}
	}

	auto SpawnEnterNode = [Graph]() -> UARDialogueEdGraphNode*
	{
		if (!Graph)
		{
			return nullptr;
		}

		Graph->Modify();
		UARDialogueEdGraphNode* EnterNode = NewObject<UARDialogueEdGraphNode>(Graph);
		EnterNode->SetFlags(RF_Transactional);
		EnterNode->InitializeForNodeType(EDialogueNodeType::Enter);
		EnterNode->NodePosX = -300;
		EnterNode->NodePosY = 0;
		EnterNode->CreateNewGuid();
		EnterNode->AllocateDefaultPins();
		Graph->AddNode(EnterNode, true, false);
		return EnterNode;
	};

	TArray<UARDialogueEdGraphNode*> EnterNodes;
	for (UARDialogueEdGraphNode* Node : EditorNodes)
	{
		if (Node && Node->RuntimeNode.NodeType == EDialogueNodeType::Enter)
		{
			EnterNodes.Add(Node);
		}
	}

	if (EnterNodes.IsEmpty())
	{
		if (UARDialogueEdGraphNode* NewEnterNode = SpawnEnterNode())
		{
			NewEnterNode->EnsureStableIds(false, false);
			EditorNodes.Add(NewEnterNode);
			AddValidationIssue(
				OutValidationReport,
				EDialogueValidationSeverity::Warning,
				NewEnterNode->RuntimeNode.NodeId,
				TEXT("Missing Enter node was automatically re-created during compile."));
			Graph->NotifyGraphChanged();
			ConversationAsset->MarkPackageDirty();
		}
	}
	else if (EnterNodes.Num() > 1)
	{
		UARDialogueEdGraphNode* PrimaryEnterNode = EnterNodes[0];
		Graph->Modify();
		for (int32 Index = 1; Index < EnterNodes.Num(); ++Index)
		{
			if (UARDialogueEdGraphNode* DuplicateEnterNode = EnterNodes[Index])
			{
				EditorNodes.RemoveSingleSwap(DuplicateEnterNode);
				DuplicateEnterNode->Modify();
				DuplicateEnterNode->DestroyNode();
			}
		}

		AddValidationIssue(
			OutValidationReport,
			EDialogueValidationSeverity::Warning,
			PrimaryEnterNode ? PrimaryEnterNode->RuntimeNode.NodeId : FGuid(),
			TEXT("Multiple Enter nodes were found; extras were removed automatically during compile."));
		Graph->NotifyGraphChanged();
		ConversationAsset->MarkPackageDirty();
	}

	if (EditorNodes.IsEmpty())
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("Editor graph has no dialogue nodes."));
		return false;
	}

	auto ResolveLinkedNodeId = [&OutValidationReport](const UARDialogueEdGraphNode* SourceNode, const UEdGraphPin* SourcePin, const FString& PinLabel) -> FGuid
	{
		if (!SourcePin || SourcePin->Direction != EGPD_Output || SourcePin->LinkedTo.IsEmpty())
		{
			return FGuid();
		}

		if (SourcePin->LinkedTo.Num() > 1)
		{
			AddValidationIssue(
				OutValidationReport,
				EDialogueValidationSeverity::Warning,
				SourceNode ? SourceNode->RuntimeNode.NodeId : FGuid(),
				FString::Printf(TEXT("Pin '%s' has %d links; compile uses the first link."),
					*PinLabel,
					SourcePin->LinkedTo.Num()));
		}

		const UEdGraphPin* LinkedPin = SourcePin->LinkedTo[0];
		const UARDialogueEdGraphNode* LinkedNode = LinkedPin ? Cast<UARDialogueEdGraphNode>(LinkedPin->GetOwningNode()) : nullptr;
		if (!LinkedNode)
		{
			AddValidationIssue(
				OutValidationReport,
				EDialogueValidationSeverity::Error,
				SourceNode ? SourceNode->RuntimeNode.NodeId : FGuid(),
				FString::Printf(TEXT("Pin '%s' is linked to a non-dialogue node."), *PinLabel));
			return FGuid();
		}

		return LinkedNode->RuntimeNode.NodeId;
	};

	FDialogueCompiledConversationData CompiledData;
	int32 EnterCount = 0;

	for (UARDialogueEdGraphNode* EditorNode : EditorNodes)
	{
		if (!EditorNode)
		{
			continue;
		}

		FDialogueCompiledNode CompiledNode = EditorNode->RuntimeNode;
		CompiledNode.NextNodeId.Invalidate();
		CompiledNode.TrueNodeId.Invalidate();
		CompiledNode.FalseNodeId.Invalidate();
		CompiledNode.FallbackNodeId.Invalidate();
		CompiledNode.SwitchDefaultNodeId.Invalidate();
		for (FDialogueCompiledChoiceBranch& Branch : CompiledNode.ChoiceBranches)
		{
			Branch.NextNodeId.Invalidate();
		}
		for (FDialogueCompiledSwitchBranch& Branch : CompiledNode.SwitchBranches)
		{
			Branch.NextNodeId.Invalidate();
		}
		for (FDialogueCompiledRandomBranch& Branch : CompiledNode.RandomBranches)
		{
			Branch.NextNodeId.Invalidate();
		}
		for (FDialogueCompiledSequenceBranch& Branch : CompiledNode.SequenceBranches)
		{
			Branch.NextNodeId.Invalidate();
		}

		switch (CompiledNode.NodeType)
		{
		case EDialogueNodeType::Enter:
			CompiledNode.NextNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameNext()), TEXT("Next"));
			++EnterCount;
			if (!CompiledData.EnterNodeId.IsValid())
			{
				CompiledData.EnterNodeId = CompiledNode.NodeId;
			}
			break;
		case EDialogueNodeType::Line:
		case EDialogueNodeType::MultiLine:
		case EDialogueNodeType::TagMutation:
		case EDialogueNodeType::RelationshipMutation:
		case EDialogueNodeType::FactionMutation:
		case EDialogueNodeType::Route:
			CompiledNode.NextNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameNext()), TEXT("Next"));
			break;
		case EDialogueNodeType::Choice:
			for (FDialogueCompiledChoiceBranch& Branch : CompiledNode.ChoiceBranches)
			{
				CompiledNode.CompletedChoicePolicy = EditorNode->RuntimeNode.CompletedChoicePolicy;
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetChoiceOutputPin(Branch.ChoiceBranchId),
					Branch.ChoiceText.ToString());
			}
			CompiledNode.FallbackNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameFallback()), TEXT("Fallback"));
			break;
		case EDialogueNodeType::Bool:
			CompiledNode.TrueNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameTrue()), TEXT("True"));
			CompiledNode.FalseNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameFalse()), TEXT("False"));
			break;
		case EDialogueNodeType::SwitchOnTagsByPriority:
			for (FDialogueCompiledSwitchBranch& Branch : CompiledNode.SwitchBranches)
			{
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetSwitchOutputPin(Branch.BranchId),
					Branch.Label.ToString());
			}
			if (CompiledNode.bSwitchHasDefaultOutput)
			{
				CompiledNode.SwitchDefaultNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UARDialogueEdGraphNode::GetPinNameSwitchDefault()), TEXT("Default"));
			}
			break;
		case EDialogueNodeType::Random:
			for (FDialogueCompiledRandomBranch& Branch : CompiledNode.RandomBranches)
			{
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetRandomOutputPin(Branch.BranchId),
					FString::Printf(TEXT("Random %.2f"), Branch.Weight));
			}
			break;
		case EDialogueNodeType::Sequence:
			for (int32 BranchIndex = 0; BranchIndex < CompiledNode.SequenceBranches.Num(); ++BranchIndex)
			{
				FDialogueCompiledSequenceBranch& Branch = CompiledNode.SequenceBranches[BranchIndex];
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetSequenceOutputPin(Branch.BranchId),
					FString::Printf(TEXT("Then %d"), BranchIndex + 1));
			}
			break;
		default:
			break;
		}

		CompiledData.Nodes.Add(MoveTemp(CompiledNode));
	}

	if (EnterCount != 1)
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("Compiled graph must have exactly one Enter node."));
	}

	ConversationAsset->Modify();
	ConversationAsset->CompiledData = MoveTemp(CompiledData);
	ConversationAsset->MarkPackageDirty();

	FDialogueValidationReport RuntimeValidation;
	bool bRuntimeValid = false;
	if (UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemForValidation())
	{
		bRuntimeValid = DialogueSubsystem->ValidateConversation(ConversationAsset, RuntimeValidation);
	}
	else
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("Dialogue validation subsystem is unavailable."));
	}

	for (const FDialogueValidationIssue& Issue : RuntimeValidation.Issues)
	{
		OutValidationReport.Issues.Add(Issue);
	}

	return bRuntimeValid && !OutValidationReport.HasErrors();
}

void SDialogueConversationGraphEditorPanel::ApplyValidationToEditorNodes(UARDialogueConversationAsset* ConversationAsset, const FDialogueValidationReport& ValidationReport) const
{
	if (!ConversationAsset)
	{
		return;
	}

	UARDialogueEdGraph* Graph = Cast<UARDialogueEdGraph>(ConversationAsset->EditorGraph);
	if (!Graph)
	{
		return;
	}

	TMap<FGuid, EDialogueValidationSeverity> HighestSeverityByNode;
	TMap<FGuid, FString> MessageByNode;

	for (const FDialogueValidationIssue& Issue : ValidationReport.Issues)
	{
		if (!Issue.NodeId.IsValid())
		{
			continue;
		}

		const EDialogueValidationSeverity ExistingSeverity = HighestSeverityByNode.FindRef(Issue.NodeId);
		if (!HighestSeverityByNode.Contains(Issue.NodeId) || static_cast<uint8>(Issue.Severity) > static_cast<uint8>(ExistingSeverity))
		{
			HighestSeverityByNode.Add(Issue.NodeId, Issue.Severity);
			MessageByNode.Add(Issue.NodeId, Issue.Message.ToString());
		}
	}

	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
		if (!DialogueNode)
		{
			continue;
		}

		if (const EDialogueValidationSeverity* Severity = HighestSeverityByNode.Find(DialogueNode->RuntimeNode.NodeId))
		{
			DialogueNode->ApplyValidation(*Severity, MessageByNode.FindRef(DialogueNode->RuntimeNode.NodeId));
		}
		else
		{
			DialogueNode->ClearValidation();
		}
	}

	Graph->NotifyGraphChanged();
}
