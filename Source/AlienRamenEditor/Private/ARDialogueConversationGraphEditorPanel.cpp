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
#include "EdGraph/EdGraphPin.h"
#include "Engine/Engine.h"
#include "FileHelpers.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "GraphEditor.h"
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
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleDeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &SDialogueConversationGraphEditorPanel::CanDeleteSelectedNodes));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
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
				.Text(FText::FromString(TEXT("Compile Runtime Graph")))
				.ToolTipText(FText::FromString(TEXT("Builds compile-managed runtime node/link data from editor graph pins and writes it back to the conversation asset.")))
				.OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleCompile)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SBox)
			]
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
	if ((InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace) && !InKeyEvent.IsControlDown() && !InKeyEvent.IsCommandDown())
	{
		if (CanDeleteSelectedNodes())
		{
			HandleDeleteSelectedNodes();
			return FReply::Handled();
		}
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
			if (Cast<UARDialogueEdGraphNode>(SelectedObject))
			{
				DetailsView->SetObject(SelectedObject);
				return;
			}
		}
	}

	DetailsView->SetObject(SelectedConversation.Get());
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
	AppendLogLine(FString::Printf(TEXT("Compile Runtime Graph: %s (Version %d)"),
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
		UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(SelectedObject);
		if (!DialogueNode)
		{
			continue;
		}

		if (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
		{
			++SkippedEnterCount;
			continue;
		}

		if (DialogueNode->CanUserDeleteNode())
		{
			DialogueNode->Modify();
			DialogueNode->DestroyNode();
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
		const UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(SelectedObject);
		if (!DialogueNode || !DialogueNode->CanUserDeleteNode())
		{
			continue;
		}

		if (DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::Enter)
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
		case EDialogueNodeType::TagMutation:
		case EDialogueNodeType::RelationshipMutation:
		case EDialogueNodeType::FactionMutation:
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
		case EDialogueNodeType::TagMutation:
		case EDialogueNodeType::RelationshipMutation:
		case EDialogueNodeType::FactionMutation:
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
