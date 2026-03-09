#include "SARDialogueInlineGraphNode.h"

#include "ARDialogueEdGraphNode.h"
#include "ARFactionSettings.h"
#include "ARDialogueTypes.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "SGameplayTagCombo.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
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
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Graph.Node.TitleBackground")))
				.BorderBackgroundColor(this, &SARDialogueInlineGraphNode::GetTitleColor)
				.Padding(FMargin(6.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(this, &SARDialogueInlineGraphNode::GetNodeTitleText)
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
					BuildInlineContent()
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
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Choices")))
	];

	if (!ChoiceBranches || ChoiceBranches->IsEmpty())
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No choices yet. Use Add pin to create one.")))
		];
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
				SNew(SHorizontalBox)
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
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("X")))
					.ToolTipText(FText::FromString(TEXT("Remove this choice branch pin.")))
					.OnClicked(this, &SARDialogueInlineGraphNode::HandleRemoveChoiceBranchClicked, Branch.ChoiceBranchId)
				]
			];
		}
	}

	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Fallback Choice Text")))
	];

	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 2.0f, 0.0f, 0.0f)
	[
		SNew(SEditableTextBox)
		.Text_Lambda([this]()
		{
			const UARDialogueEdGraphNode* Node = GetDialogueNode();
			return Node ? Node->RuntimeNode.FallbackChoiceText : FText::GetEmpty();
		})
		.HintText(FText::FromString(TEXT("Fallback text shown for fallback output")))
		.OnTextCommitted(this, &SARDialogueInlineGraphNode::HandleFallbackTextCommitted)
	];

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildSwitchInlineContent() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const TArray<FDialogueCompiledSwitchBranch>* SwitchBranches = DialogueNode ? &DialogueNode->RuntimeNode.SwitchBranches : nullptr;

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Switch Branch Labels")))
	];

	if (!SwitchBranches || SwitchBranches->IsEmpty())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No switch branches yet. Use Add pin to create one.")))
		];
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
				SNew(SHorizontalBox)
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
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("X")))
					.ToolTipText(FText::FromString(TEXT("Remove this switch branch pin.")))
					.OnClicked(this, &SARDialogueInlineGraphNode::HandleRemoveSwitchBranchClicked, Branch.BranchId)
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
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Random Branch Weights")))
	];

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
				SNew(SHorizontalBox)
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
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(1000000.0f)
					.Delta(0.1f)
					.Value(Branch.Weight)
					.OnValueCommitted(this, &SARDialogueInlineGraphNode::HandleRandomWeightCommitted, Branch.BranchId)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("X")))
					.ToolTipText(FText::FromString(TEXT("Remove this random branch pin.")))
					.OnClicked(this, &SARDialogueInlineGraphNode::HandleRemoveRandomBranchClicked, Branch.BranchId)
				]
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildRelationshipInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Target Speaker")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
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
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Relationship Delta Points")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSpinBox<float>)
			.MinValue(-1000000.0f)
			.MaxValue(1000000.0f)
			.Delta(1.0f)
			.Value_Lambda([this]()
			{
				const UARDialogueEdGraphNode* Node = GetDialogueNode();
				const FDialogueRelationshipMutationNodeData* Data = Node ? Node->RuntimeNode.NodeData.GetPtr<FDialogueRelationshipMutationNodeData>() : nullptr;
				return Data ? Data->DeltaPoints : 0.0f;
			})
			.OnValueCommitted(this, &SARDialogueInlineGraphNode::HandleRelationshipDeltaCommitted)
		];
}

TSharedRef<SWidget> SARDialogueInlineGraphNode::BuildFactionInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Faction Tag")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
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
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Faction Popularity Delta")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSpinBox<float>)
			.MinValue(-1000000.0f)
			.MaxValue(1000000.0f)
			.Delta(1.0f)
			.Value_Lambda([this]()
			{
				const UARDialogueEdGraphNode* Node = GetDialogueNode();
				const FDialogueFactionMutationNodeData* Data = Node ? Node->RuntimeNode.NodeData.GetPtr<FDialogueFactionMutationNodeData>() : nullptr;
				return Data ? Data->DeltaPopularity : 0.0f;
			})
			.OnValueCommitted(this, &SARDialogueInlineGraphNode::HandleFactionDeltaCommitted)
		];
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
		.Text(FText::FromString(TEXT("Add pin +")))
		.ToolTipText(FText::FromString(TEXT("Add another output branch pin.")))
		.OnClicked(this, &SARDialogueInlineGraphNode::HandleAddBranchPinClicked)
	];
}

FReply SARDialogueInlineGraphNode::HandleAddBranchPinClicked() const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->AddDynamicBranchPin();
	}

	return FReply::Handled();
}

FReply SARDialogueInlineGraphNode::HandleRemoveChoiceBranchClicked(const FGuid ChoiceBranchId) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->RemoveDynamicBranchPinByName(UARDialogueEdGraphNode::MakeChoicePinName(ChoiceBranchId));
	}

	return FReply::Handled();
}

FReply SARDialogueInlineGraphNode::HandleRemoveSwitchBranchClicked(const FGuid BranchId) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->RemoveDynamicBranchPinByName(UARDialogueEdGraphNode::MakeSwitchPinName(BranchId));
	}

	return FReply::Handled();
}

FReply SARDialogueInlineGraphNode::HandleRemoveRandomBranchClicked(const FGuid BranchId) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->RemoveDynamicBranchPinByName(UARDialogueEdGraphNode::MakeRandomPinName(BranchId));
	}

	return FReply::Handled();
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

void SARDialogueInlineGraphNode::HandleRelationshipSpeakerTagChanged(const FGameplayTag NewTag) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetRelationshipTargetSpeakerTag(NewTag);
	}
}

void SARDialogueInlineGraphNode::HandleRelationshipDeltaCommitted(const float NewValue, const ETextCommit::Type CommitType) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetRelationshipDeltaPoints(NewValue);
	}
}

void SARDialogueInlineGraphNode::HandleFactionTagChanged(const FGameplayTag NewTag) const
{
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetFactionTag(NewTag);
	}
}

void SARDialogueInlineGraphNode::HandleFactionDeltaCommitted(const float NewValue, const ETextCommit::Type CommitType) const
{
	(void)CommitType;
	if (UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable())
	{
		DialogueNode->SetFactionDeltaPopularity(NewValue);
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

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueInlineGraphNodeFactory()
{
	return MakeShared<FARDialogueInlineGraphNodeFactory>();
}
