#include "ParleyDialogueConversationGraphEditorPanel.h"

#include "ParleyConversationAsset.h"
#include "ParleyDialogueConditionCompileUtils.h"
#include "ParleyDialogueEdGraph.h"
#include "ParleyDialogueEdGraphNode.h"
#include "ParleyDialogueEdGraphSchema.h"
#include "ParleyDialogueSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
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
#include "GameplayTagsManager.h"
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
	static UParleyDialogueSubsystem* GetDialogueSubsystemFromPIE()
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

		return PIEContext->OwningGameInstance->GetSubsystem<UParleyDialogueSubsystem>();
	}

	static UParleyDialogueSubsystem* GetDialogueSubsystemForValidation()
	{
		if (UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemFromPIE())
		{
			return DialogueSubsystem;
		}

		static TWeakObjectPtr<UParleyDialogueSubsystem> Cached;
		if (!Cached.IsValid())
		{
			Cached = NewObject<UParleyDialogueSubsystem>(GetTransientPackage());
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

	static bool ContainsTagExact(const TArray<FGameplayTag>& Tags, const FGameplayTag Tag)
	{
		return Tags.ContainsByPredicate([Tag](const FGameplayTag Existing)
		{
			return Existing.MatchesTagExact(Tag);
		});
	}

	static bool EnsureConversationParticipantTags(UParleyConversationAsset* ConversationAsset)
	{
		if (!ConversationAsset)
		{
			return false;
		}

		TArray<FGameplayTag> UpdatedTags = ConversationAsset->Header.ParticipatingSpeakerTags;
		auto AddUniqueTag = [&UpdatedTags](const FGameplayTag Tag)
		{
			if (Tag.IsValid() && !ContainsTagExact(UpdatedTags, Tag))
			{
				UpdatedTags.Add(Tag);
			}
		};

		AddUniqueTag(ConversationAsset->Header.PrimarySpeakerTag);
		if (const FGameplayTag RequesterTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Speaker.Requester"), false); RequesterTag.IsValid())
		{
			AddUniqueTag(RequesterTag);
		}

		for (const FDialogueCompiledNode& Node : ConversationAsset->CompiledData.Nodes)
		{
			if (Node.NodeType == EDialogueNodeType::Line)
			{
				if (const FDialogueLineNodeData* LineData = Node.NodeData.GetPtr<FDialogueLineNodeData>())
				{
					AddUniqueTag(LineData->Line.SpeakerTag);
				}
			}
			else if (Node.NodeType == EDialogueNodeType::MultiLine || Node.NodeType == EDialogueNodeType::SplitLine)
			{
				if (const FDialogueMultiLineNodeData* MultiLineData = Node.NodeData.GetPtr<FDialogueMultiLineNodeData>())
				{
					for (const FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
					{
						AddUniqueTag(Entry.LineData.Line.SpeakerTag);
					}
				}
			}
			else if (Node.NodeType == EDialogueNodeType::RouteByCharacter)
			{
				for (const FDialogueCompiledCharacterRouteBranch& Branch : Node.CharacterRouteBranches)
				{
					AddUniqueTag(Branch.SpeakerTag);
				}
			}
		}

		const bool bDifferentCount = UpdatedTags.Num() != ConversationAsset->Header.ParticipatingSpeakerTags.Num();
		const bool bDifferentTags = !bDifferentCount
			&& UpdatedTags.ContainsByPredicate([ConversationAsset](const FGameplayTag Tag)
			{
				return !ContainsTagExact(ConversationAsset->Header.ParticipatingSpeakerTags, Tag);
			});

		if (!bDifferentCount && !bDifferentTags)
		{
			return false;
		}

		ConversationAsset->Modify();
		ConversationAsset->Header.ParticipatingSpeakerTags = MoveTemp(UpdatedTags);
		ConversationAsset->MarkPackageDirty();
		return true;
	}

	static void SyncDialogueGraphPins(UParleyDialogueEdGraph* Graph)
	{
		if (!Graph)
		{
			return;
		}

		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
			if (!DialogueNode)
			{
				continue;
			}

			DialogueNode->EnsureStableIds(false, false);
			DialogueNode->ReconstructNode();
		}

		Graph->NotifyGraphChanged();
	}

	static TWeakObjectPtr<UParleyConversationAsset> GPendingConversationToEdit;
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
				.AllowedClass(UParleyConversationAsset::StaticClass())
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

void SDialogueConversationGraphEditorPanel::RequestOpenConversation(UParleyConversationAsset* Asset)
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

void SDialogueConversationGraphEditorPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	LoadPendingConversationRequest();
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

void SDialogueConversationGraphEditorPanel::SetSelectedConversation(UParleyConversationAsset* Asset)
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

	SelectedEditorGraph = Cast<UParleyDialogueEdGraph>(Asset->EditorGraph);
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
	GraphEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleVerifyNodeTextCommit);
	GraphEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &SDialogueConversationGraphEditorPanel::HandleNodeTextCommitted);
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

	SetSelectedConversation(Cast<UParleyConversationAsset>(AssetData.GetAsset()));
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

bool SDialogueConversationGraphEditorPanel::HandleVerifyNodeTextCommit(
	const FText& NewText,
	UEdGraphNode* Node,
	FText& OutErrorMessage) const
{
	(void)Node;
	if (NewText.ToString().TrimStartAndEnd().IsEmpty())
	{
		OutErrorMessage = FText::FromString(TEXT("Name cannot be empty."));
		return false;
	}

	return true;
}

void SDialogueConversationGraphEditorPanel::HandleNodeTextCommitted(
	const FText& NewText,
	const ETextCommit::Type CommitType,
	UEdGraphNode* Node)
{
	(void)CommitType;
	if (!Node)
	{
		return;
	}

	Node->Modify();
	Node->OnRenameNode(NewText.ToString());
	if (UParleyConversationAsset* Conversation = SelectedConversation.Get())
	{
		Conversation->MarkPackageDirty();
	}
}

FReply SDialogueConversationGraphEditorPanel::HandleSpawnNodeByShortcut(FInputChord InChord, const FVector2f& Location)
{
	if (InChord.Key != EKeys::R && InChord.Key != EKeys::C)
	{
		return FReply::Unhandled();
	}

	UParleyDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph)
	{
		return FReply::Unhandled();
	}

	if (InChord.Key == EKeys::R)
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Add Dialogue Route Node")));
		Graph->Modify();

		UParleyDialogueEdGraphNode* RouteNode = NewObject<UParleyDialogueEdGraphNode>(Graph);
		RouteNode->SetFlags(RF_Transactional);
		RouteNode->InitializeForNodeType(EDialogueEditorNodeType::Route);
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
		if (UParleyConversationAsset* Conversation = SelectedConversation.Get())
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
	UParleyDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph)
	{
		return;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Add Dialogue Comment")));
	Graph->Modify();

	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();
	CommentTemplate->SetFlags(RF_Transactional);
	CommentTemplate->bCommentBubbleVisible_InDetailsPanel = false;
	CommentTemplate->bColorCommentBubble = false;
	CommentTemplate->NodeComment = TEXT("Comment");
	CommentTemplate->NodeWidth = 360;
	CommentTemplate->NodeHeight = 180;

	UEdGraphNode_Comment* CommentNode =
		Cast<UEdGraphNode_Comment>(
			FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(
				Graph,
				CommentTemplate,
				Location,
				true));
	if (!CommentNode)
	{
		return;
	}

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
	if (UParleyConversationAsset* Conversation = SelectedConversation.Get())
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
	UParleyConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		SetStatusMessage(TEXT("Save failed: no conversation selected."), EEditorStatusType::Error);
		return FReply::Handled();
	}

	ValidationOutput.Empty();

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
	UParleyConversationAsset* Conversation = SelectedConversation.Get();
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
	UParleyConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		SetStatusMessage(TEXT("Compile failed: no conversation selected."), EEditorStatusType::Error);
		return FReply::Handled();
	}

	ValidationOutput.Empty();

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

		const UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
		if (DialogueNode && DialogueNode->EditorNodeType == EDialogueEditorNodeType::Enter)
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

		const UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
		if (DialogueNode && DialogueNode->EditorNodeType == EDialogueEditorNodeType::Enter)
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
	UParleyDialogueEdGraph* Graph = SelectedEditorGraph.Get();
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
	UParleyDialogueEdGraph* Graph = SelectedEditorGraph.Get();
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

		if (UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(PastedNode))
		{
			if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::Enter)
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
	if (UParleyConversationAsset* Conversation = SelectedConversation.Get())
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
	UParleyDialogueEdGraph* Graph = SelectedEditorGraph.Get();
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

		if (UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
			DialogueNode && DialogueNode->EditorNodeType == EDialogueEditorNodeType::Enter)
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
		if (UParleyConversationAsset* Conversation = SelectedConversation.Get())
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

		const UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
		if (!DialogueNode || DialogueNode->EditorNodeType != EDialogueEditorNodeType::Enter)
		{
			return true;
		}
	}

	return false;
}

bool SDialogueConversationGraphEditorPanel::EnsureConversationEditorGraph(UParleyConversationAsset* ConversationAsset)
{
	if (!ConversationAsset)
	{
		return false;
	}

	UParleyDialogueEdGraph* ExistingGraph = Cast<UParleyDialogueEdGraph>(ConversationAsset->EditorGraph);
	if (ExistingGraph && ExistingGraph->Schema == UParleyDialogueEdGraphSchema::StaticClass())
	{
		if (ExistingGraph->Nodes.IsEmpty())
		{
			if (const UEdGraphSchema* Schema = ExistingGraph->GetSchema())
			{
				ExistingGraph->Modify();
				Schema->CreateDefaultNodesForGraph(*ExistingGraph);
				ExistingGraph->NotifyGraphChanged();
				ConversationAsset->MarkPackageDirty();
			}
		}
		return true;
	}

	ConversationAsset->Modify();
	UParleyDialogueEdGraph* NewGraph = NewObject<UParleyDialogueEdGraph>(ConversationAsset, NAME_None, RF_Transactional);
	NewGraph->Schema = UParleyDialogueEdGraphSchema::StaticClass();
	ConversationAsset->EditorGraph = NewGraph;
	if (const UEdGraphSchema* Schema = NewGraph->GetSchema())
	{
		Schema->CreateDefaultNodesForGraph(*NewGraph);
		NewGraph->NotifyGraphChanged();
	}
	ConversationAsset->MarkPackageDirty();
	return true;
}

bool SDialogueConversationGraphEditorPanel::CompileEditorGraphToRuntime(UParleyConversationAsset* ConversationAsset, FDialogueValidationReport& OutValidationReport) const
{
	OutValidationReport = FDialogueValidationReport();
	if (!ConversationAsset)
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("No conversation selected for compile."));
		return false;
	}

	UParleyDialogueEdGraph* Graph = Cast<UParleyDialogueEdGraph>(ConversationAsset->EditorGraph);
	if (!Graph)
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("Conversation has no editor graph."));
		return false;
	}

	TArray<UParleyDialogueEdGraphNode*> EditorNodes;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		if (UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode))
		{
			DialogueNode->EnsureStableIds(false, false);
			EditorNodes.Add(DialogueNode);
		}
	}

	auto SpawnEnterNode = [Graph]() -> UParleyDialogueEdGraphNode*
	{
		if (!Graph)
		{
			return nullptr;
		}

		Graph->Modify();
		UParleyDialogueEdGraphNode* EnterNode = NewObject<UParleyDialogueEdGraphNode>(Graph);
		EnterNode->SetFlags(RF_Transactional);
		EnterNode->InitializeForNodeType(EDialogueEditorNodeType::Enter);
		EnterNode->NodePosX = -300;
		EnterNode->NodePosY = 0;
		EnterNode->CreateNewGuid();
		EnterNode->AllocateDefaultPins();
		Graph->AddNode(EnterNode, true, false);
		return EnterNode;
	};

	TArray<UParleyDialogueEdGraphNode*> EnterNodes;
	for (UParleyDialogueEdGraphNode* Node : EditorNodes)
	{
		if (Node && Node->EditorNodeType == EDialogueEditorNodeType::Enter)
		{
			EnterNodes.Add(Node);
		}
	}

	if (EnterNodes.IsEmpty())
	{
		if (UParleyDialogueEdGraphNode* NewEnterNode = SpawnEnterNode())
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
		UParleyDialogueEdGraphNode* PrimaryEnterNode = EnterNodes[0];
		Graph->Modify();
		for (int32 Index = 1; Index < EnterNodes.Num(); ++Index)
		{
			if (UParleyDialogueEdGraphNode* DuplicateEnterNode = EnterNodes[Index])
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

	auto ResolveLinkedNodeId = [&OutValidationReport](const UParleyDialogueEdGraphNode* SourceNode, const UEdGraphPin* SourcePin, const FString& PinLabel) -> FGuid
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
		const UParleyDialogueEdGraphNode* LinkedNode = LinkedPin ? Cast<UParleyDialogueEdGraphNode>(LinkedPin->GetOwningNode()) : nullptr;
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

	const auto AddConditionCompileIssues = [&OutValidationReport](const FGuid NodeId, const TArray<FString>& Warnings, const TArray<FString>& Errors)
	{
		for (const FString& Warning : Warnings)
		{
			AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Warning, NodeId, Warning);
		}
		for (const FString& Error : Errors)
		{
			AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, NodeId, Error);
		}
	};

	FDialogueCompiledConversationData CompiledData;
	int32 EnterCount = 0;

	for (UParleyDialogueEdGraphNode* EditorNode : EditorNodes)
	{
		if (!EditorNode)
		{
			continue;
		}

		if (ParleyDialogueConditionCompile::IsConditionSourceNodeType(EditorNode->EditorNodeType))
		{
			continue;
		}

		FDialogueCompiledNode CompiledNode = EditorNode->RuntimeNode;
		CompiledNode.NodeType = UParleyDialogueEdGraphNode::MakeRuntimeNodeTypeFromEditor(EditorNode->EditorNodeType);
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
		for (FDialogueCompiledCharacterRouteBranch& Branch : CompiledNode.CharacterRouteBranches)
		{
			Branch.NextNodeId.Invalidate();
		}

		switch (EditorNode->EditorNodeType)
		{
		case EDialogueEditorNodeType::Enter:
			CompiledNode.NextNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameNext()), TEXT("Next"));
			++EnterCount;
			if (!CompiledData.EnterNodeId.IsValid())
			{
				CompiledData.EnterNodeId = CompiledNode.NodeId;
			}
			break;
		case EDialogueEditorNodeType::Line:
		case EDialogueEditorNodeType::MultiLine:
		case EDialogueEditorNodeType::SplitLine:
		case EDialogueEditorNodeType::TagMutation:
		case EDialogueEditorNodeType::RelationshipMutation:
		case EDialogueEditorNodeType::FactionMutation:
		case EDialogueEditorNodeType::Signal:
		case EDialogueEditorNodeType::Route:
			CompiledNode.NextNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameNext()), TEXT("Next"));
			break;
		case EDialogueEditorNodeType::Choice:
			for (FDialogueCompiledChoiceBranch& Branch : CompiledNode.ChoiceBranches)
			{
				CompiledNode.CompletedChoicePolicy = EditorNode->RuntimeNode.CompletedChoicePolicy;
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetChoiceOutputPin(Branch.ChoiceBranchId),
					Branch.ChoiceText.ToString());
			}
			CompiledNode.FallbackNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameFallback()), TEXT("Fallback"));
			break;
		case EDialogueEditorNodeType::Branch:
		{
			CompiledNode.NodeType = EDialogueNodeType::SwitchOnTagsByPriority;
			CompiledNode.NodeData.Reset();
			CompiledNode.SwitchBranches.Reset();
			CompiledNode.bSwitchHasDefaultOutput = true;

			FDialogueConditionGroup ConditionGroup;
			TArray<FString> ConditionWarnings;
			TArray<FString> ConditionErrors;
			if (ParleyDialogueConditionCompile::BuildConditionGroupFromBranchNode(EditorNode, ConditionGroup, ConditionWarnings, ConditionErrors))
			{
				FDialogueCompiledSwitchBranch& TrueBranch = CompiledNode.SwitchBranches.AddDefaulted_GetRef();
				TrueBranch.BranchId = FGuid::NewGuid();
				TrueBranch.Label = FText::FromString(TEXT("True"));
				TrueBranch.LockedConditions = ConditionGroup;
				TrueBranch.NextNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameTrue()), TEXT("True"));
			}
			AddConditionCompileIssues(EditorNode->RuntimeNode.NodeId, ConditionWarnings, ConditionErrors);
			CompiledNode.SwitchDefaultNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameFalse()), TEXT("False"));
			break;
		}
		case EDialogueEditorNodeType::SwitchOnTagsByPriority:
			for (FDialogueCompiledSwitchBranch& Branch : CompiledNode.SwitchBranches)
			{
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetSwitchOutputPin(Branch.BranchId),
					Branch.Label.ToString());
			}
			if (CompiledNode.bSwitchHasDefaultOutput)
			{
				CompiledNode.SwitchDefaultNodeId = ResolveLinkedNodeId(EditorNode, EditorNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameSwitchDefault()), TEXT("Default"));
			}
			break;
		case EDialogueEditorNodeType::Random:
			for (FDialogueCompiledRandomBranch& Branch : CompiledNode.RandomBranches)
			{
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetRandomOutputPin(Branch.BranchId),
					FString::Printf(TEXT("Random %.2f"), Branch.Weight));
			}
			break;
		case EDialogueEditorNodeType::Sequence:
			for (int32 BranchIndex = 0; BranchIndex < CompiledNode.SequenceBranches.Num(); ++BranchIndex)
			{
				FDialogueCompiledSequenceBranch& Branch = CompiledNode.SequenceBranches[BranchIndex];
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetSequenceOutputPin(Branch.BranchId),
					FString::Printf(TEXT("Then %d"), BranchIndex + 1));
			}
			break;
		case EDialogueEditorNodeType::RouteByCharacter:
			for (FDialogueCompiledCharacterRouteBranch& Branch : CompiledNode.CharacterRouteBranches)
			{
				Branch.NextNodeId = ResolveLinkedNodeId(
					EditorNode,
					EditorNode->GetCharacterRouteOutputPin(Branch.BranchId),
					Branch.SpeakerTag.ToString());
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
	EnsureConversationParticipantTags(ConversationAsset);
	ConversationAsset->MarkPackageDirty();

	FDialogueValidationReport RuntimeValidation;
	bool bRuntimeValid = false;
	if (UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemForValidation())
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

void SDialogueConversationGraphEditorPanel::ApplyValidationToEditorNodes(UParleyConversationAsset* ConversationAsset, const FDialogueValidationReport& ValidationReport) const
{
	if (!ConversationAsset)
	{
		return;
	}

	UParleyDialogueEdGraph* Graph = Cast<UParleyDialogueEdGraph>(ConversationAsset->EditorGraph);
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
		UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
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

