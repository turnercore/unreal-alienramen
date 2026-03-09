#include "SARDialogueInlineGraphNode.h"

#include "ARDialogueEdGraphNode.h"
#include "ARFactionSettings.h"
#include "ARDialogueTypes.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "InputCoreTypes.h"
#include "Misc/DefaultValueHelper.h"
#include "SGameplayTagCombo.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float ChoiceMinWidth = 320.0f;
	constexpr float CompactMinWidth = 260.0f;
	constexpr float RandomMinWidth = 140.0f;
	constexpr float MutationMinWidth = 200.0f;
	constexpr float SequenceMinWidth = 72.0f;
	constexpr float SwitchMinWidth = 180.0f;

	class FARDialogueBranchDragDropOp final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FARDialogueBranchDragDropOp, FDecoratedDragDropOp)

		static TSharedRef<FARDialogueBranchDragDropOp> New(const EDialogueNodeType InBranchNodeType, const FGuid InBranchId)
		{
			TSharedRef<FARDialogueBranchDragDropOp> Op = MakeShared<FARDialogueBranchDragDropOp>();
			Op->BranchNodeType = InBranchNodeType;
			Op->BranchId = InBranchId;
			Op->DefaultHoverText = FText::FromString(TEXT("Reorder Branch"));
			Op->Construct();
			return Op;
		}

		EDialogueNodeType BranchNodeType = EDialogueNodeType::Line;
		FGuid BranchId;
	};

	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnDialogueBranchDropped, EDialogueNodeType, FGuid, FGuid);

	class SARDialogueBranchDragRow final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SARDialogueBranchDragRow) {}
			SLATE_ARGUMENT(EDialogueNodeType, BranchNodeType)
			SLATE_ARGUMENT(FGuid, BranchId)
			SLATE_EVENT(FOnDialogueBranchDropped, OnBranchDropped)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			BranchNodeType = InArgs._BranchNodeType;
			BranchId = InArgs._BranchId;
			OnBranchDropped = InArgs._OnBranchDropped;

			ChildSlot
			[
				InArgs._Content.Widget
			];
		}

		virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			(void)MyGeometry;
			const TSharedPtr<FARDialogueBranchDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FARDialogueBranchDragDropOp>();
			if (DragOperation.IsValid() && DragOperation->BranchNodeType == BranchNodeType)
			{
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			(void)MyGeometry;
			const TSharedPtr<FARDialogueBranchDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FARDialogueBranchDragDropOp>();
			if (!DragOperation.IsValid() || DragOperation->BranchNodeType != BranchNodeType || DragOperation->BranchId == BranchId)
			{
				return FReply::Unhandled();
			}

			if (OnBranchDropped.IsBound() && OnBranchDropped.Execute(BranchNodeType, DragOperation->BranchId, BranchId))
			{
				return FReply::Handled();
			}

			return FReply::Unhandled();
		}

	private:
		EDialogueNodeType BranchNodeType = EDialogueNodeType::Line;
		FGuid BranchId;
		FOnDialogueBranchDropped OnBranchDropped;
	};

	class SARDialogueBranchDragHandle final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SARDialogueBranchDragHandle) {}
			SLATE_ARGUMENT(EDialogueNodeType, BranchNodeType)
			SLATE_ARGUMENT(FGuid, BranchId)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			BranchNodeType = InArgs._BranchNodeType;
			BranchId = InArgs._BranchId;

			ChildSlot
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("::")))
				.ToolTipText(FText::FromString(TEXT("Drag to reorder this branch.")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f, 1.0f)))
			];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton);
			}
			return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
		}

		virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			(void)MyGeometry;
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				return FReply::Handled().BeginDragDrop(FARDialogueBranchDragDropOp::New(BranchNodeType, BranchId));
			}
			return FReply::Unhandled();
		}

	private:
		EDialogueNodeType BranchNodeType = EDialogueNodeType::Line;
		FGuid BranchId;
	};

	class FARDialogueInlineGraphNodeFactory final : public FGraphPanelNodeFactory
	{
	public:
		virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override
		{
			UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(InNode);
			if (!DialogueNode)
			{
				return nullptr;
			}

			switch (DialogueNode->RuntimeNode.NodeType)
			{
			case EDialogueNodeType::Choice:
			case EDialogueNodeType::SwitchOnTagsByPriority:
			case EDialogueNodeType::Random:
			case EDialogueNodeType::Sequence:
			case EDialogueNodeType::RelationshipMutation:
			case EDialogueNodeType::FactionMutation:
				return SNew(SARDialogueInlineGraphNode, DialogueNode);
			default:
				return nullptr;
			}
		}
	};
}

void SARDialogueInlineGraphNode::Construct(const FArguments& InArgs, UARDialogueEdGraphNode* InNode)
{
	(void)InArgs;
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

void SARDialogueInlineGraphNode::UpdateGraphNode()
{
	InputPins.Reset();
	OutputPins.Reset();
	LeftNodeBox.Reset();
	RightNodeBox.Reset();

	ContentScale.Bind(this, &SGraphNode::GetContentScale);
	GetOrAddSlot(ENodeZone::Center)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Graph.Node.Body")))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("Graph.Node.TitleBackground")))
					.BorderBackgroundColor(this, &SARDialogueInlineGraphNode::GetTitleColor)
					.Padding(FMargin(6.0f, 2.0f))
					[
						SNew(STextBlock)
						.Text(this, &SARDialogueInlineGraphNode::GetNodeTitleText)
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Graph.Node.TitleGloss")))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f)))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(6.0f, 0.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(GetInlineContentMinWidth())
					[
						BuildInlineContent()
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
	];

	CreatePinWidgets();
	AddDynamicPinButtonIfSupported();
}

void SARDialogueInlineGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	if (PinToAdd->GetDirection() == EGPD_Input)
	{
		InputPins.Add(PinToAdd);
		if (LeftNodeBox.IsValid())
		{
			LeftNodeBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				PinToAdd
			];
		}
		return;
	}

	OutputPins.Add(PinToAdd);
	if (RightNodeBox.IsValid())
	{
		RightNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			PinToAdd
		];
	}
}

const UARDialogueEdGraphNode* SARDialogueInlineGraphNode::GetDialogueNode() const
{
	return Cast<UARDialogueEdGraphNode>(GraphNode);
}

UARDialogueEdGraphNode* SARDialogueInlineGraphNode::GetDialogueNodeMutable() const
{
	return Cast<UARDialogueEdGraphNode>(GraphNode);
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	if (!DialogueNode)
	{
		return SNew(STextBlock).Text(FText::FromString(TEXT("Invalid dialogue node.")));
	}

	switch (DialogueNode->RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Choice:
		return BuildChoiceInlineContent();
	case EDialogueNodeType::SwitchOnTagsByPriority:
		return BuildSwitchInlineContent();
	case EDialogueNodeType::Random:
		return BuildRandomInlineContent();
	case EDialogueNodeType::Sequence:
		return BuildSequenceInlineContent();
	case EDialogueNodeType::RelationshipMutation:
		return BuildRelationshipInlineContent();
	case EDialogueNodeType::FactionMutation:
		return BuildFactionInlineContent();
	default:
		return SNew(STextBlock).Text(FText::FromString(TEXT("Inline editing unavailable for this node type.")));
	}
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildChoiceInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const TArray<FDialogueCompiledChoiceBranch>* ChoiceBranches = DialogueNode ? &DialogueNode->RuntimeNode.ChoiceBranches : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (!ChoiceBranches || ChoiceBranches->IsEmpty())
	{
		// Intentionally empty: keep compact footprint when no choices are authored yet.
	}
	else
	{
		for (int32 Index = 0; Index < ChoiceBranches->Num(); ++Index)
		{
			const FDialogueCompiledChoiceBranch& Branch = (*ChoiceBranches)[Index];
			Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SARDialogueBranchDragRow)
				.BranchNodeType(EDialogueNodeType::Choice)
				.BranchId(Branch.ChoiceBranchId)
				.OnBranchDropped(FOnDialogueBranchDropped::CreateSP(this, &SARDialogueInlineGraphNode::HandleBranchRowDropped))
				[
					SNew(SBorder)
					.Padding(FMargin(2.0f, 1.0f))
					.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SARDialogueBranchDragHandle)
							.BranchNodeType(EDialogueNodeType::Choice)
							.BranchId(Branch.ChoiceBranchId)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%d."), Index + 1)))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SEditableTextBox)
							.Text(Branch.ChoiceText)
							.HintText(FText::FromString(TEXT("Choice text")))
							.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleChoiceTextCommitted, Branch.ChoiceBranchId)
						]
					]
				]
			];
		}
	}

	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		SNew(SEditableTextBox)
		.Text_Lambda([this]()
		{
			const UARDialogueEdGraphNode* Node = GetDialogueNode();
			return Node ? Node->RuntimeNode.FallbackChoiceText : FText::GetEmpty();
		})
		.HintText(FText::FromString(TEXT("Fallback text")))
		.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleFallbackTextCommitted)
	];

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildSwitchInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const TArray<FDialogueCompiledSwitchBranch>* SwitchBranches = DialogueNode ? &DialogueNode->RuntimeNode.SwitchBranches : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (!SwitchBranches || SwitchBranches->IsEmpty())
	{
		// Intentionally empty: keep compact footprint when no branches are authored yet.
	}
	else
	{
		for (int32 Index = 0; Index < SwitchBranches->Num(); ++Index)
		{
			const FDialogueCompiledSwitchBranch& Branch = (*SwitchBranches)[Index];
			Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SARDialogueBranchDragRow)
				.BranchNodeType(EDialogueNodeType::SwitchOnTagsByPriority)
				.BranchId(Branch.BranchId)
				.OnBranchDropped(FOnDialogueBranchDropped::CreateSP(this, &SARDialogueInlineGraphNode::HandleBranchRowDropped))
				[
					SNew(SBorder)
					.Padding(FMargin(2.0f, 1.0f))
					.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SARDialogueBranchDragHandle)
							.BranchNodeType(EDialogueNodeType::SwitchOnTagsByPriority)
							.BranchId(Branch.BranchId)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%d."), Index + 1)))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SEditableTextBox)
							.Text(Branch.Label)
							.HintText(FText::FromString(TEXT("Branch label")))
							.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleSwitchLabelCommitted, Branch.BranchId)
						]
					]
				]
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildRandomInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const TArray<FDialogueCompiledRandomBranch>* RandomBranches = DialogueNode ? &DialogueNode->RuntimeNode.RandomBranches : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (!RandomBranches || RandomBranches->IsEmpty())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No random branches yet. Use Add pin to create one.")))
		];
	}
	else
	{
		for (int32 Index = 0; Index < RandomBranches->Num(); ++Index)
		{
			const FDialogueCompiledRandomBranch& Branch = (*RandomBranches)[Index];
			Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SARDialogueBranchDragRow)
				.BranchNodeType(EDialogueNodeType::Random)
				.BranchId(Branch.BranchId)
				.OnBranchDropped(FOnDialogueBranchDropped::CreateSP(this, &SARDialogueInlineGraphNode::HandleBranchRowDropped))
				[
					SNew(SBorder)
					.Padding(FMargin(2.0f, 1.0f))
					.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SARDialogueBranchDragHandle)
							.BranchNodeType(EDialogueNodeType::Random)
							.BranchId(Branch.BranchId)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%d."), Index + 1)))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(76.0f)
							[
								SNew(SEditableTextBox)
								.Text(FText::AsNumber(Branch.Weight))
								.HintText(FText::FromString(TEXT("1.0")))
								.OnTextCommitted_Lambda([this, BranchId = Branch.BranchId](const FText& NewText, const ETextCommit::Type CommitType)
								{
									(void)CommitType;
									float ParsedValue = 0.0f;
									if (!FDefaultValueHelper::ParseFloat(NewText.ToString(), ParsedValue))
									{
										return;
									}

									if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
									{
										DialogueNode->SetRandomBranchWeight(BranchId, ParsedValue);
									}
								})
							]
						]
					]
				]
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildSequenceInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const TArray<FDialogueCompiledSequenceBranch>* SequenceBranches = DialogueNode ? &DialogueNode->RuntimeNode.SequenceBranches : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (!SequenceBranches || SequenceBranches->IsEmpty())
	{
		// No helper text for sequence by design.
	}
	else
	{
		for (int32 Index = 0; Index < SequenceBranches->Num(); ++Index)
		{
			Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Then %d"), Index + 1)))
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildMultiLineInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const FDialogueMultiLineNodeData* MultiLineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>() : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (!MultiLineData || MultiLineData->Lines.IsEmpty())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No lines yet. Add a line to author this node.")))
		];
	}
	else
	{
		const bool bCanDeleteEntries = MultiLineData->Lines.Num() > 1;
		for (int32 Index = 0; Index < MultiLineData->Lines.Num(); ++Index)
		{
			const FDialogueMultiLineEntry& Entry = MultiLineData->Lines[Index];
			Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SARDialogueBranchDragRow)
				.BranchNodeType(EDialogueNodeType::MultiLine)
				.BranchId(Entry.EntryId)
				.OnBranchDropped(FOnDialogueBranchDropped::CreateSP(this, &SARDialogueInlineGraphNode::HandleBranchRowDropped))
				[
					SNew(SBorder)
					.Padding(FMargin(2.0f, 1.0f))
					.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SARDialogueBranchDragHandle)
							.BranchNodeType(EDialogueNodeType::MultiLine)
							.BranchId(Entry.EntryId)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%d."), Index + 1)))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(180.0f)
							[
								SNew(SGameplayTagCombo)
								.Filter(TEXT("Dialogue.Speaker"))
								.Tag(Entry.LineData.Line.SpeakerTag)
								.OnTagChanged(this, &SARDialogueInlineGraphNode::HandleMultiLineSpeakerTagChanged, Entry.EntryId)
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SEditableTextBox)
							.Text(Entry.LineData.Line.Text)
							.HintText(FText::FromString(TEXT("Line text")))
							.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleMultiLineTextCommitted, Entry.EntryId)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("X")))
							.ToolTipText(FText::FromString(TEXT("Delete this line entry.")))
							.IsEnabled(bCanDeleteEntries)
							.OnClicked(this, &SARDialogueInlineGraphNode::HandleDeleteMultiLineEntryClicked, Entry.EntryId)
						]
					]
				]
			];
		}
	}

	Content->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Right)
	.Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		SNew(SButton)
		.ToolTipText(FText::FromString(TEXT("Append a new line entry to this multi-line node.")))
		.OnClicked(this, &SARDialogueInlineGraphNode::HandleAddMultiLineEntryClicked)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Add Line +")))
		]
	];

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildRelationshipInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(200.0f)
			[
				SNew(SGameplayTagCombo)
				.Filter(TEXT("Dialogue.Speaker"))
				.Tag_Lambda([this]()
				{
					const UARDialogueEdGraphNode* Node = GetDialogueNode();
					const FDialogueRelationshipMutationNodeData* Data = Node ? Node->RuntimeNode.NodeData.GetPtr<FDialogueRelationshipMutationNodeData>() : nullptr;
					return Data ? Data->TargetSpeakerTag : FGameplayTag();
				})
				.OnTagChanged(this, &SARDialogueInlineGraphNode::HandleRelationshipSpeakerTagChanged)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(84.0f)
			[
				SNew(SEditableTextBox)
				.Text(this, &SARDialogueInlineGraphNode::GetRelationshipDeltaText)
				.HintText(FText::FromString(TEXT("Delta")))
				.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleRelationshipDeltaTextCommitted)
			]
		];
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildFactionInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(200.0f)
			[
				SNew(SGameplayTagCombo)
				.Filter(GetFactionTagFilter())
				.Tag_Lambda([this]()
				{
					const UARDialogueEdGraphNode* Node = GetDialogueNode();
					const FDialogueFactionMutationNodeData* Data = Node ? Node->RuntimeNode.NodeData.GetPtr<FDialogueFactionMutationNodeData>() : nullptr;
					return Data ? Data->FactionTag : FGameplayTag();
				})
				.OnTagChanged(this, &SARDialogueInlineGraphNode::HandleFactionTagChanged)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(84.0f)
			[
				SNew(SEditableTextBox)
				.Text(this, &SARDialogueInlineGraphNode::GetFactionDeltaText)
				.HintText(FText::FromString(TEXT("Delta")))
				.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleFactionDeltaTextCommitted)
			]
		];
}

float SARDialogueInlineGraphNode::GetInlineContentMinWidth() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	if (!DialogueNode)
	{
		return CompactMinWidth;
	}

	switch (DialogueNode->RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Choice:
		return ChoiceMinWidth;
	case EDialogueNodeType::Random:
		return RandomMinWidth;
	case EDialogueNodeType::SwitchOnTagsByPriority:
		return SwitchMinWidth;
	case EDialogueNodeType::RelationshipMutation:
	case EDialogueNodeType::FactionMutation:
		return MutationMinWidth;
	case EDialogueNodeType::Sequence:
		return SequenceMinWidth;
	default:
		return CompactMinWidth;
	}
}

void SARDialogueInlineGraphNode::AddDynamicPinButtonIfSupported()
{
	if (!RightNodeBox.IsValid())
	{
		return;
	}

	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	if (!DialogueNode || !DialogueNode->SupportsDynamicBranchPins())
	{
		return;
	}

	RightNodeBox->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Right)
	.Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(2.0f, 1.0f))
		.ToolTipText(FText::FromString(TEXT("Add another output branch pin.")))
		.OnClicked(this, &SARDialogueInlineGraphNode::HandleAddBranchPinClicked)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Add")))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Plus")))
			]
		]
	];
}

FReply SARDialogueInlineGraphNode::HandleAddBranchPinClicked() const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->AddDynamicBranchPin();
		RefreshNodeWidget();
	}

	return FReply::Handled();
}

void SARDialogueInlineGraphNode::RefreshNodeWidget() const
{
	const_cast<SARDialogueInlineGraphNode*>(this)->UpdateGraphNode();
}

bool SARDialogueInlineGraphNode::HandleBranchRowDropped(
	const EDialogueNodeType BranchNodeType,
	const FGuid DraggedBranchId,
	const FGuid TargetBranchId) const
{
	if (!DraggedBranchId.IsValid() || !TargetBranchId.IsValid() || DraggedBranchId == TargetBranchId)
	{
		return false;
	}

	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode)
	{
		return false;
	}

	bool bReordered = false;
	switch (BranchNodeType)
	{
	case EDialogueNodeType::Choice:
		bReordered = DialogueNode->ReorderChoiceBranch(DraggedBranchId, TargetBranchId);
		break;
	case EDialogueNodeType::SwitchOnTagsByPriority:
		bReordered = DialogueNode->ReorderSwitchBranch(DraggedBranchId, TargetBranchId);
		break;
	case EDialogueNodeType::Random:
		bReordered = DialogueNode->ReorderRandomBranch(DraggedBranchId, TargetBranchId);
		break;
	case EDialogueNodeType::MultiLine:
		bReordered = DialogueNode->ReorderMultiLineEntry(DraggedBranchId, TargetBranchId);
		break;
	default:
		break;
	}

	if (bReordered)
	{
		RefreshNodeWidget();
	}

	return bReordered;
}

void SARDialogueInlineGraphNode::HandleChoiceTextCommitted(const FText& NewText, const ETextCommit::Type CommitType, const FGuid ChoiceBranchId) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetChoiceBranchText(ChoiceBranchId, NewText);
	}
}

void SARDialogueInlineGraphNode::HandleFallbackTextCommitted(const FText& NewText, const ETextCommit::Type CommitType) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetChoiceFallbackText(NewText);
	}
}

void SARDialogueInlineGraphNode::HandleSwitchLabelCommitted(const FText& NewText, const ETextCommit::Type CommitType, const FGuid BranchId) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetSwitchBranchLabel(BranchId, NewText);
	}
}

void SARDialogueInlineGraphNode::HandleRandomWeightCommitted(const float NewValue, const ETextCommit::Type CommitType, const FGuid BranchId) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetRandomBranchWeight(BranchId, NewValue);
	}
}

FReply SARDialogueInlineGraphNode::HandleAddMultiLineEntryClicked() const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->AddMultiLineEntry();
		RefreshNodeWidget();
	}

	return FReply::Handled();
}

FReply SARDialogueInlineGraphNode::HandleDeleteMultiLineEntryClicked(const FGuid EntryId) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->RemoveMultiLineEntry(EntryId);
		RefreshNodeWidget();
	}

	return FReply::Handled();
}

void SARDialogueInlineGraphNode::HandleMultiLineSpeakerTagChanged(const FGameplayTag NewTag, const FGuid EntryId) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetMultiLineEntrySpeakerTag(EntryId, NewTag);
	}
}

void SARDialogueInlineGraphNode::HandleMultiLineTextCommitted(const FText& NewText, const ETextCommit::Type CommitType, const FGuid EntryId) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetMultiLineEntryText(EntryId, NewText);
	}
}

void SARDialogueInlineGraphNode::HandleRelationshipSpeakerTagChanged(const FGameplayTag NewTag) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetRelationshipTargetSpeakerTag(NewTag);
	}
}

void SARDialogueInlineGraphNode::HandleRelationshipDeltaTextCommitted(const FText& NewText, const ETextCommit::Type CommitType) const
{
	(void)CommitType;
	float ParsedValue = 0.0f;
	if (!FDefaultValueHelper::ParseFloat(NewText.ToString(), ParsedValue))
	{
		return;
	}

	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetRelationshipDeltaPoints(ParsedValue);
	}
}

void SARDialogueInlineGraphNode::HandleFactionTagChanged(const FGameplayTag NewTag) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetFactionTag(NewTag);
	}
}

void SARDialogueInlineGraphNode::HandleFactionDeltaTextCommitted(const FText& NewText, const ETextCommit::Type CommitType) const
{
	(void)CommitType;
	float ParsedValue = 0.0f;
	if (!FDefaultValueHelper::ParseFloat(NewText.ToString(), ParsedValue))
	{
		return;
	}

	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetFactionDeltaPopularity(ParsedValue);
	}
}

FText SARDialogueInlineGraphNode::GetNodeTitleText() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? DialogueNode->GetNodeTitle(ENodeTitleType::ListView) : FText::FromString(TEXT("Dialogue Node"));
}

FSlateColor SARDialogueInlineGraphNode::GetTitleColor() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? FSlateColor(DialogueNode->GetNodeTitleColor()) : FSlateColor(FLinearColor::Black);
}

FString SARDialogueInlineGraphNode::GetFactionTagFilter() const
{
	const UARFactionSettings* FactionSettings = GetDefault<UARFactionSettings>();
	if (!FactionSettings || !FactionSettings->FactionDefinitionRootTag.IsValid())
	{
		return FString();
	}

	return FactionSettings->FactionDefinitionRootTag.ToString();
}

FText SARDialogueInlineGraphNode::GetRelationshipDeltaText() const
{
	const UARDialogueEdGraphNode* Node = GetDialogueNode();
	const FDialogueRelationshipMutationNodeData* Data = Node ? Node->RuntimeNode.NodeData.GetPtr<FDialogueRelationshipMutationNodeData>() : nullptr;
	return FText::AsNumber(Data ? Data->DeltaPoints : 0.0f);
}

FText SARDialogueInlineGraphNode::GetFactionDeltaText() const
{
	const UARDialogueEdGraphNode* Node = GetDialogueNode();
	const FDialogueFactionMutationNodeData* Data = Node ? Node->RuntimeNode.NodeData.GetPtr<FDialogueFactionMutationNodeData>() : nullptr;
	return FText::AsNumber(Data ? Data->DeltaPopularity : 0.0f);
}

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueInlineGraphNodeFactory()
{
	return MakeShared<FARDialogueInlineGraphNodeFactory>();
}
