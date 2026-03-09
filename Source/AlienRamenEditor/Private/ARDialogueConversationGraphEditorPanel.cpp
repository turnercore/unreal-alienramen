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
#include "GameplayTagsManager.h"
#include "GraphEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Misc/DefaultValueHelper.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
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

	static UARDialogueSubsystem* GetDialogueSubsystemForValidationAndPreview()
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

	static UARDialogueEdGraphNode* GetLinkedDialogueNode(const UEdGraphPin* OutputPin)
	{
		if (!OutputPin || OutputPin->Direction != EGPD_Output || OutputPin->LinkedTo.IsEmpty())
		{
			return nullptr;
		}

		return Cast<UARDialogueEdGraphNode>(OutputPin->LinkedTo[0]->GetOwningNode());
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
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Refresh"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleRefresh)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Save"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleSave)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Validate"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleValidate)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Compile Runtime Graph"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleCompile)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Focus Enter Node"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleFocusEnterNode)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Auto Layout"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandleAutoLayout)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Preview Conversation"))).OnClicked(this, &SDialogueConversationGraphEditorPanel::HandlePreviewConversation)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Conversation Asset")))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
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
					+ SVerticalBox::Slot().FillHeight(0.58f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						DetailsView.ToSharedRef()
					]
					+ SVerticalBox::Slot().FillHeight(0.42f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Preview Inputs")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SCheckBox)
								.IsChecked_Lambda([this]() { return bPreviewAsBrother ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) { bPreviewAsBrother = (NewState == ECheckBoxState::Checked); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Active Character = Brother (unchecked: Sister)")))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SCheckBox)
								.IsChecked_Lambda([this]() { return bPreviewCompletedByPlayer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) { bPreviewCompletedByPlayer = (NewState == ECheckBoxState::Checked); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Completed By Player")))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SCheckBox)
								.IsChecked_Lambda([this]() { return bPreviewCompletedByGame ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) { bPreviewCompletedByGame = (NewState == ECheckBoxState::Checked); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Completed By Game")))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SCheckBox)
								.IsChecked_Lambda([this]() { return bPreviewSeenByPlayer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) { bPreviewSeenByPlayer = (NewState == ECheckBoxState::Checked); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Seen By Player")))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SCheckBox)
								.IsChecked_Lambda([this]() { return bPreviewSeenByGame ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) { bPreviewSeenByGame = (NewState == ECheckBoxState::Checked); })
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Seen By Game")))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(0.0f)
								.MaxValue(10000.0f)
								.Value_Lambda([this]() { return PreviewRelationshipPoints; })
								.OnValueChanged_Lambda([this](const float NewValue) { PreviewRelationshipPoints = NewValue; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Relationship Points")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0)
								.MaxValue(1000000)
								.Value_Lambda([this]() { return PreviewPlayerKills; })
								.OnValueChanged_Lambda([this](const int32 NewValue) { PreviewPlayerKills = NewValue; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Player Kills")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(0.0f)
								.MaxValue(1000000.0f)
								.Value_Lambda([this]() { return PreviewTimePlayed; })
								.OnValueChanged_Lambda([this](const float NewValue) { PreviewTimePlayed = NewValue; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Time Played (s)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SAssignNew(PreviewCombinedTagsTextBox, SEditableTextBox)
								.HintText(FText::FromString(TEXT("Combined tags (comma-separated)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SAssignNew(PreviewPlayerTagsTextBox, SEditableTextBox)
								.HintText(FText::FromString(TEXT("Player tags (comma-separated)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SAssignNew(PreviewGameTagsTextBox, SEditableTextBox)
								.HintText(FText::FromString(TEXT("Game tags (comma-separated)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SAssignNew(PreviewTransientTagsTextBox, SEditableTextBox)
								.HintText(FText::FromString(TEXT("Transient tags (comma-separated)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SAssignNew(PreviewLoadoutTagsTextBox, SEditableTextBox)
								.HintText(FText::FromString(TEXT("Loadout tags (comma-separated)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Injected Variables (one per line: Name=Value or Name:Type=Value)")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
							[
								SAssignNew(PreviewInjectedVariablesTextBox, SMultiLineEditableTextBox)
								.HintText(FText::FromString(TEXT("Example:\nIsHungry:Bool=true\nMoodTag:Tag=Dialogue.Mood.Happy\nAttempts:Int=2\nShopMultiplier:Float=1.25\nGreeting=Hello there")))
								.AutoWrapText(false)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return FText::FromString(ValidationOutput + TEXT("\n\n") + PreviewOutput); })
								.AutoWrapText(true)
							]
						]
					]
				]
			]
		]
	];

	RebuildGraphEditorWidget(nullptr);
	LoadPendingConversationRequest();
}

void SDialogueConversationGraphEditorPanel::RequestOpenConversation(UARDialogueConversationAsset* Asset)
{
	GPendingConversationToEdit = Asset;
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
		return;
	}

	if (!EnsureConversationEditorGraph(Asset))
	{
		AppendLogLine(TEXT("Failed to initialize editor graph for selected conversation."));
		return;
	}

	SelectedEditorGraph = Cast<UARDialogueEdGraph>(Asset->EditorGraph);
	RebuildGraphEditorWidget(SelectedEditorGraph.Get());
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Asset);
	}
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
	}
	else
	{
		LoadPendingConversationRequest();
	}
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleSave()
{
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		return FReply::Handled();
	}

	FDialogueValidationReport ValidationReport;
	CompileEditorGraphToRuntime(Conversation, ValidationReport);
	Conversation->LastCompileValidation = ValidationReport;
	Conversation->bLastCompileSucceeded = !ValidationReport.HasErrors();
	ApplyValidationToEditorNodes(Conversation, ValidationReport);

	Conversation->MarkPackageDirty();
	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(Conversation->GetOutermost());
	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
	AppendLogLine(FString::Printf(TEXT("Saved '%s'."), *Conversation->GetName()));
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleValidate()
{
	ValidationOutput.Empty();
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
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
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleCompile()
{
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		AppendLogLine(TEXT("No conversation selected."));
		return FReply::Handled();
	}

	FDialogueValidationReport ValidationReport;
	const bool bCompiled = CompileEditorGraphToRuntime(Conversation, ValidationReport);

	Conversation->LastCompileValidation = ValidationReport;
	Conversation->bLastCompileSucceeded = bCompiled;
	Conversation->CompileVersion += 1;
	Conversation->MarkPackageDirty();

	ApplyValidationToEditorNodes(Conversation, ValidationReport);
	AppendLogLine(FString::Printf(TEXT("Compile Runtime Graph: %s (Version %d)"),
		bCompiled ? TEXT("SUCCESS") : TEXT("FAILED"),
		Conversation->CompileVersion));
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleFocusEnterNode()
{
	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph || !GraphEditorWidget.IsValid())
	{
		return FReply::Handled();
	}

	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
		if (!DialogueNode || DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::Enter)
		{
			continue;
		}

		GraphEditorWidget->ClearSelectionSet();
		GraphEditorWidget->SetNodeSelection(DialogueNode, true);
		GraphEditorWidget->JumpToNode(DialogueNode, false);
		break;
	}

	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandleAutoLayout()
{
	UARDialogueEdGraph* Graph = SelectedEditorGraph.Get();
	if (!Graph)
	{
		return FReply::Handled();
	}

	TMap<FGuid, UARDialogueEdGraphNode*> NodeById;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
	{
		if (UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode))
		{
			DialogueNode->EnsureStableIds(false, false);
			NodeById.Add(DialogueNode->RuntimeNode.NodeId, DialogueNode);
		}
	}

	FGuid EnterNodeId;
	for (const TPair<FGuid, UARDialogueEdGraphNode*>& Pair : NodeById)
	{
		if (Pair.Value && Pair.Value->RuntimeNode.NodeType == EDialogueNodeType::Enter)
		{
			EnterNodeId = Pair.Key;
			break;
		}
	}

	TMap<FGuid, int32> DepthByNode;
	TArray<FGuid> Queue;
	if (EnterNodeId.IsValid())
	{
		DepthByNode.Add(EnterNodeId, 0);
		Queue.Add(EnterNodeId);
	}

	while (!Queue.IsEmpty())
	{
		const FGuid CurrentId = Queue[0];
		Queue.RemoveAt(0, 1, EAllowShrinking::No);

		UARDialogueEdGraphNode* CurrentNode = NodeById.FindRef(CurrentId);
		if (!CurrentNode)
		{
			continue;
		}

		const int32 CurrentDepth = DepthByNode.FindRef(CurrentId);
		for (UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			UARDialogueEdGraphNode* LinkedNode = GetLinkedDialogueNode(Pin);
			if (!LinkedNode)
			{
				continue;
			}

			const FGuid LinkedId = LinkedNode->RuntimeNode.NodeId;
			if (!DepthByNode.Contains(LinkedId))
			{
				DepthByNode.Add(LinkedId, CurrentDepth + 1);
				Queue.Add(LinkedId);
			}
		}
	}

	TMap<int32, int32> RowByDepth;
	TArray<TPair<FGuid, UARDialogueEdGraphNode*>> SortedNodes;
	for (const TPair<FGuid, UARDialogueEdGraphNode*>& Pair : NodeById)
	{
		SortedNodes.Add(Pair);
	}
	SortedNodes.Sort([&DepthByNode](const TPair<FGuid, UARDialogueEdGraphNode*>& Lhs, const TPair<FGuid, UARDialogueEdGraphNode*>& Rhs)
	{
		const int32 LDepth = DepthByNode.FindRef(Lhs.Key);
		const int32 RDepth = DepthByNode.FindRef(Rhs.Key);
		if (LDepth != RDepth)
		{
			return LDepth < RDepth;
		}
		return Lhs.Key.ToString() < Rhs.Key.ToString();
	});

	constexpr int32 XSpacing = 420;
	constexpr int32 YSpacing = 220;
	constexpr int32 UnreachableDepth = 8;

	Graph->Modify();
	for (const TPair<FGuid, UARDialogueEdGraphNode*>& Pair : SortedNodes)
	{
		UARDialogueEdGraphNode* Node = Pair.Value;
		if (!Node)
		{
			continue;
		}

		const bool bReachable = DepthByNode.Contains(Pair.Key);
		const int32 Depth = bReachable ? DepthByNode.FindRef(Pair.Key) : UnreachableDepth;
		int32& Row = RowByDepth.FindOrAdd(Depth);

		Node->Modify();
		Node->NodePosX = Depth * XSpacing;
		Node->NodePosY = Row * YSpacing;
		++Row;
	}

	Graph->NotifyGraphChanged();
	if (UARDialogueConversationAsset* Conversation = SelectedConversation.Get())
	{
		Conversation->MarkPackageDirty();
	}

	AppendLogLine(TEXT("Auto Layout applied."));
	return FReply::Handled();
}

FReply SDialogueConversationGraphEditorPanel::HandlePreviewConversation()
{
	PreviewOutput.Empty();
	UARDialogueConversationAsset* Conversation = SelectedConversation.Get();
	if (!Conversation)
	{
		PreviewOutput = TEXT("No conversation selected.");
		return FReply::Handled();
	}

	FDialogueValidationReport ValidationReport;
	CompileEditorGraphToRuntime(Conversation, ValidationReport);

	FDialogueRuntimeContext PreviewContext;
	PreviewContext.ConversationTag = Conversation->Header.ConversationTag;
	PreviewContext.PrimarySpeakerTag = Conversation->Header.PrimarySpeakerTag;
	PreviewContext.RelationshipPointsForPrimarySpeaker = PreviewRelationshipPoints;
	PreviewContext.PlayerKills = PreviewPlayerKills;
	PreviewContext.TimePlayed = PreviewTimePlayed;
	PreviewContext.bSeenByPlayer = bPreviewSeenByPlayer;
	PreviewContext.bSeenByGame = bPreviewSeenByGame;
	PreviewContext.bCompletedByGame = bPreviewCompletedByGame;
	PreviewContext.bCompletedByPlayer = bPreviewCompletedByPlayer;
	PreviewContext.ResolvedPlayerSpeakerTag = bPreviewAsBrother
		? UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Dialogue.Speaker.Brother"), false)
		: UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Dialogue.Speaker.Sister"), false);

	ParseTagList(PreviewCombinedTagsTextBox.IsValid() ? PreviewCombinedTagsTextBox->GetText().ToString() : FString(), PreviewContext.CombinedProgressionTags);
	ParseTagList(PreviewPlayerTagsTextBox.IsValid() ? PreviewPlayerTagsTextBox->GetText().ToString() : FString(), PreviewContext.PlayerOnlyProgressionTags);
	ParseTagList(PreviewGameTagsTextBox.IsValid() ? PreviewGameTagsTextBox->GetText().ToString() : FString(), PreviewContext.GameOnlyProgressionTags);
	ParseTagList(PreviewTransientTagsTextBox.IsValid() ? PreviewTransientTagsTextBox->GetText().ToString() : FString(), PreviewContext.TransientConversationTags);
	ParseTagList(PreviewLoadoutTagsTextBox.IsValid() ? PreviewLoadoutTagsTextBox->GetText().ToString() : FString(), PreviewContext.LoadoutView.LoadoutTags);
	PreviewContext.CombinedProgressionTags = PreviewContext.PlayerOnlyProgressionTags;
	PreviewContext.CombinedProgressionTags.AppendTags(PreviewContext.GameOnlyProgressionTags);

	FString InjectedParseError;
	if (!ParseInjectedVariables(
		PreviewInjectedVariablesTextBox.IsValid() ? PreviewInjectedVariablesTextBox->GetText().ToString() : FString(),
		PreviewContext.InjectedVariables,
		InjectedParseError))
	{
		PreviewOutput = FString::Printf(TEXT("Injected variable parse error: %s"), *InjectedParseError);
		return FReply::Handled();
	}

	TArray<FDialogueClientView> PreviewTrace;
	TArray<FGuid> AutoChoiceSelections;
	bool bEndedCompleted = false;
	FDialogueValidationReport PreviewReport;
	bool bPreviewReturnedTrace = false;
	if (UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemForValidationAndPreview())
	{
		bPreviewReturnedTrace = DialogueSubsystem->PreviewConversationTrace(
			Conversation,
			PreviewContext,
			128,
			PreviewTrace,
			AutoChoiceSelections,
			bEndedCompleted,
			PreviewReport);
	}

	if (!bPreviewReturnedTrace)
	{
		PreviewOutput = TEXT("Preview failed (trace execution could not complete safely).");
		return FReply::Handled();
	}

	PreviewOutput = FString::Printf(TEXT("Preview Steps: %d\nEnded Completed: %s\nPlayerKills: %d\nTimePlayed: %.2f\nCombinedTags: %d | PlayerTags: %d | GameTags: %d | TransientTags: %d | LoadoutTags: %d | InjectedVars: %d"),
		PreviewTrace.Num(),
		bEndedCompleted ? TEXT("Yes") : TEXT("No"),
		PreviewPlayerKills,
		PreviewTimePlayed,
		PreviewContext.CombinedProgressionTags.Num(),
		PreviewContext.PlayerOnlyProgressionTags.Num(),
		PreviewContext.GameOnlyProgressionTags.Num(),
		PreviewContext.TransientConversationTags.Num(),
		PreviewContext.LoadoutView.LoadoutTags.Num(),
		PreviewContext.InjectedVariables.Num());

	int32 AutoChoiceCursor = 0;
	for (int32 StepIndex = 0; StepIndex < PreviewTrace.Num(); ++StepIndex)
	{
		const FDialogueClientView& StepView = PreviewTrace[StepIndex];
		PreviewOutput += FString::Printf(
			TEXT("\n\nStep %d\nNode: %s\nSpeaker: %s\nLine: %s\nWaitingChoice: %s"),
			StepIndex + 1,
			*StepView.CurrentNodeId.ToString(EGuidFormats::DigitsWithHyphensInBraces),
			*StepView.SpeakerTag.ToString(),
			*StepView.LineText.ToString(),
			StepView.bWaitingForChoice ? TEXT("Yes") : TEXT("No"));

		if (StepView.bWaitingForChoice)
		{
			for (const FDialogueChoiceView& Choice : StepView.Choices)
			{
				PreviewOutput += FString::Printf(
					TEXT("\n  Choice %s | CanChoose=%s | Important=%s | %s"),
					*Choice.ChoiceBranchId.ToString(EGuidFormats::DigitsWithHyphensInBraces),
					Choice.bCanChoose ? TEXT("Y") : TEXT("N"),
					Choice.bImportant ? TEXT("Y") : TEXT("N"),
					*Choice.ChoiceText.ToString());
			}

			if (AutoChoiceSelections.IsValidIndex(AutoChoiceCursor))
			{
				PreviewOutput += FString::Printf(
					TEXT("\n  AutoSelected: %s"),
					*AutoChoiceSelections[AutoChoiceCursor].ToString(EGuidFormats::DigitsWithHyphensInBraces));
				++AutoChoiceCursor;
			}
		}
	}

	for (const FDialogueValidationIssue& Issue : PreviewReport.Issues)
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
		PreviewOutput += FString::Printf(TEXT("\n[%s] %s"), SeverityLabel, *Issue.Message.ToString());
	}

	return FReply::Handled();
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

	if (EnterCount == 0)
	{
		AddValidationIssue(OutValidationReport, EDialogueValidationSeverity::Error, FGuid(), TEXT("Compiled graph has no Enter node."));
	}

	ConversationAsset->Modify();
	ConversationAsset->CompiledData = MoveTemp(CompiledData);
	ConversationAsset->MarkPackageDirty();

	FDialogueValidationReport RuntimeValidation;
	bool bRuntimeValid = false;
	if (UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemForValidationAndPreview())
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

bool SDialogueConversationGraphEditorPanel::ParseInjectedVariables(const FString& SourceText, TMap<FName, FDialogueInjectedValue>& OutVariables, FString& OutError) const
{
	OutVariables.Reset();
	OutError.Empty();

	TArray<FString> Lines;
	SourceText.ParseIntoArrayLines(Lines, false);
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		FString Line = Lines[LineIndex].TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		const int32 EqualsIndex = Line.Find(TEXT("="));
		if (EqualsIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Line %d is missing '=': %s"), LineIndex + 1, *Line);
			return false;
		}

		FString NamePart = Line.Left(EqualsIndex).TrimStartAndEnd();
		const FString ValuePart = Line.Mid(EqualsIndex + 1).TrimStartAndEnd();
		if (NamePart.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Line %d has an empty variable name."), LineIndex + 1);
			return false;
		}

		FString TypePart;
		int32 TypeSeparatorIndex = INDEX_NONE;
		if (NamePart.FindLastChar(TEXT(':'), TypeSeparatorIndex) && TypeSeparatorIndex > 0 && TypeSeparatorIndex < NamePart.Len() - 1)
		{
			TypePart = NamePart.Mid(TypeSeparatorIndex + 1).TrimStartAndEnd().ToLower();
			NamePart = NamePart.Left(TypeSeparatorIndex).TrimStartAndEnd();
		}

		if (NamePart.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Line %d has an invalid variable name."), LineIndex + 1);
			return false;
		}

		FDialogueInjectedValue ParsedValue;
		auto ParseAsBool = [&ValuePart, &ParsedValue]() -> bool
		{
			if (ValuePart.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValuePart.Equals(TEXT("1")))
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Bool;
				ParsedValue.BoolValue = true;
				return true;
			}
			if (ValuePart.Equals(TEXT("false"), ESearchCase::IgnoreCase) || ValuePart.Equals(TEXT("0")))
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Bool;
				ParsedValue.BoolValue = false;
				return true;
			}
			return false;
		};

		auto ParseAsInt = [&ValuePart, &ParsedValue]() -> bool
		{
			int32 OutInt = 0;
			if (FDefaultValueHelper::ParseInt(ValuePart, OutInt))
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Integer;
				ParsedValue.IntValue = OutInt;
				return true;
			}
			return false;
		};

		auto ParseAsFloat = [&ValuePart, &ParsedValue]() -> bool
		{
			float OutFloat = 0.0f;
			if (FDefaultValueHelper::ParseFloat(ValuePart, OutFloat))
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Float;
				ParsedValue.FloatValue = OutFloat;
				return true;
			}
			return false;
		};

		auto ParseAsTag = [&ValuePart, &ParsedValue]() -> bool
		{
			const FGameplayTag ParsedTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*ValuePart), false);
			if (ParsedTag.IsValid())
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Tag;
				ParsedValue.TagValue = ParsedTag;
				return true;
			}
			return false;
		};

		const bool bHasExplicitType = !TypePart.IsEmpty();
		bool bParsed = false;
		if (bHasExplicitType)
		{
			if (TypePart == TEXT("bool") || TypePart == TEXT("boolean"))
			{
				bParsed = ParseAsBool();
			}
			else if (TypePart == TEXT("int") || TypePart == TEXT("integer"))
			{
				bParsed = ParseAsInt();
			}
			else if (TypePart == TEXT("float"))
			{
				bParsed = ParseAsFloat();
			}
			else if (TypePart == TEXT("tag"))
			{
				bParsed = ParseAsTag();
			}
			else if (TypePart == TEXT("text") || TypePart == TEXT("string"))
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Text;
				ParsedValue.TextValue = FText::FromString(ValuePart);
				bParsed = true;
			}
			else
			{
				OutError = FString::Printf(TEXT("Line %d has unsupported type '%s'."), LineIndex + 1, *TypePart);
				return false;
			}

			if (!bParsed)
			{
				OutError = FString::Printf(TEXT("Line %d value '%s' is invalid for type '%s'."), LineIndex + 1, *ValuePart, *TypePart);
				return false;
			}
		}
		else
		{
			if (ParseAsBool() || ParseAsInt() || ParseAsFloat() || ParseAsTag())
			{
				bParsed = true;
			}
			else
			{
				ParsedValue.ValueType = EDialogueInjectedValueType::Text;
				ParsedValue.TextValue = FText::FromString(ValuePart);
				bParsed = true;
			}
		}

		if (bParsed)
		{
			OutVariables.Add(FName(*NamePart), ParsedValue);
		}
	}

	return true;
}

void SDialogueConversationGraphEditorPanel::ParseTagList(const FString& SourceText, FGameplayTagContainer& OutContainer) const
{
	OutContainer.Reset();
	TArray<FString> Parts;
	SourceText.ParseIntoArray(Parts, TEXT(","), true);
	for (FString& Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.IsEmpty())
		{
			continue;
		}

		const FGameplayTag ParsedTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Part), false);
		if (ParsedTag.IsValid())
		{
			OutContainer.AddTag(ParsedTag);
		}
	}
}
