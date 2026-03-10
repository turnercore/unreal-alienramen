#include "SARDialogueLineGraphNode.h"

#include "ARDialogueConversationAsset.h"
#include "ARDialogueEdGraphNode.h"
#include "ARDialogueSettings.h"
#include "ARDialogueTypes.h"
#include "TagContentResolverSubsystem.h"
#include "TagContentResolverEditorHelpers.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagsManager.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "ScopedTransaction.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float PortraitSize = 46.0f;
	constexpr float LineWrapWidth = 280.0f;
	constexpr float MultiLineIndexWidth = 28.0f;

	static const FARDialogueSpeakerRow* ResolveSpeakerRowForTag(const FGameplayTag SpeakerTag)
	{
		if (!SpeakerTag.IsValid())
		{
			return nullptr;
		}

		const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
		if (!DialogueSettings || !DialogueSettings->SpeakerDefinitionRootTag.IsValid())
		{
			return nullptr;
		}

		UDataTable* SpeakerTable = nullptr;
		FString LookupError;
		if (!FTagContentResolverEditorHelpers::TryResolveDataTableForRootTag(DialogueSettings->SpeakerDefinitionRootTag, SpeakerTable, LookupError))
		{
			return nullptr;
		}

		if (!SpeakerTable || SpeakerTable->GetRowStruct() != FARDialogueSpeakerRow::StaticStruct())
		{
			return nullptr;
		}

		FGameplayTag Candidate = SpeakerTag;
		while (Candidate.IsValid())
		{
			const FString TagString = Candidate.ToString();
			int32 DotIndex = INDEX_NONE;
			if (!TagString.FindLastChar(TEXT('.'), DotIndex) || DotIndex + 1 >= TagString.Len())
			{
				break;
			}

			const FName RowName(*TagString.Mid(DotIndex + 1));
			if (const FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(RowName, TEXT("DialogueLineGraphNode"), false))
			{
				return Row;
			}

			const FString ParentTagPath = TagString.Left(DotIndex);
			Candidate = UGameplayTagsManager::Get().RequestGameplayTag(FName(*ParentTagPath), false);
		}

		return nullptr;
	}

	static bool ContainsTagExact(const TArray<FGameplayTag>& Tags, const FGameplayTag Tag)
	{
		return Tags.ContainsByPredicate([Tag](const FGameplayTag Existing)
		{
			return Existing.MatchesTagExact(Tag);
		});
	}

	static FString GetTagLeaf(const FGameplayTag Tag)
	{
		const FString TagString = Tag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (TagString.FindLastChar(TEXT('.'), DotIndex) && DotIndex + 1 < TagString.Len())
		{
			return TagString.Mid(DotIndex + 1);
		}
		return TagString;
	}

	class FARDialogueLineEntryDragDropOp final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FARDialogueLineEntryDragDropOp, FDecoratedDragDropOp)

		static TSharedRef<FARDialogueLineEntryDragDropOp> New(const FGuid InEntryId)
		{
			TSharedRef<FARDialogueLineEntryDragDropOp> Op = MakeShared<FARDialogueLineEntryDragDropOp>();
			Op->EntryId = InEntryId;
			Op->DefaultHoverText = FText::FromString(TEXT("Reorder Line"));
			Op->Construct();
			return Op;
		}

		FGuid EntryId;
	};

	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnDialogueLineEntryDropped, FGuid, FGuid);

	class SARDialogueLineEntryDragRow final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SARDialogueLineEntryDragRow) {}
			SLATE_ARGUMENT(FGuid, EntryId)
			SLATE_EVENT(FOnDialogueLineEntryDropped, OnEntryDropped)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			EntryId = InArgs._EntryId;
			OnEntryDropped = InArgs._OnEntryDropped;
			ChildSlot
			[
				InArgs._Content.Widget
			];
		}

		virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			(void)MyGeometry;
			const TSharedPtr<FARDialogueLineEntryDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FARDialogueLineEntryDragDropOp>();
			return DragOperation.IsValid() ? FReply::Handled() : FReply::Unhandled();
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			(void)MyGeometry;
			const TSharedPtr<FARDialogueLineEntryDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FARDialogueLineEntryDragDropOp>();
			if (!DragOperation.IsValid() || !DragOperation->EntryId.IsValid() || !EntryId.IsValid() || DragOperation->EntryId == EntryId)
			{
				return FReply::Unhandled();
			}

			if (OnEntryDropped.IsBound() && OnEntryDropped.Execute(DragOperation->EntryId, EntryId))
			{
				return FReply::Handled();
			}

			return FReply::Unhandled();
		}

	private:
		FGuid EntryId;
		FOnDialogueLineEntryDropped OnEntryDropped;
	};

	class SARDialogueLineEntryDragHandle final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SARDialogueLineEntryDragHandle) {}
			SLATE_ARGUMENT(FGuid, EntryId)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			EntryId = InArgs._EntryId;
			ChildSlot
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("::")))
				.ToolTipText(FText::FromString(TEXT("Drag to reorder this line.")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f, 1.0f)))
			];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && EntryId.IsValid())
			{
				return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton);
			}
			return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
		}

		virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			(void)MyGeometry;
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && EntryId.IsValid())
			{
				return FReply::Handled().BeginDragDrop(FARDialogueLineEntryDragDropOp::New(EntryId));
			}
			return FReply::Unhandled();
		}

	private:
		FGuid EntryId;
	};

	class FARDialogueLineGraphNodeFactory final : public FGraphPanelNodeFactory
	{
	public:
		virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override
		{
			UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(InNode);
			if (!DialogueNode
				|| (DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::Line
					&& DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::MultiLine
					&& DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::SplitLine))
			{
				return nullptr;
			}

			return SNew(SARDialogueLineGraphNode, DialogueNode);
		}
	};
}

void SARDialogueLineGraphNode::Construct(const FArguments& InArgs, UARDialogueEdGraphNode* InNode)
{
	(void)InArgs;
	this->GraphNode = InNode;
	this->SetCursor(EMouseCursor::CardinalCross);
	this->UpdateGraphNode();
}

void SARDialogueLineGraphNode::UpdateGraphNode()
{
	InputPins.Reset();
	OutputPins.Reset();
	LeftNodeBox.Reset();
	RightNodeBox.Reset();

	TSharedRef<SVerticalBox> CenterContent = SNew(SVerticalBox);
	if (IsMultiLineNode())
	{
		const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
		const FDialogueMultiLineNodeData* MultiLineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>() : nullptr;
		if (MultiLineData)
		{
			for (int32 EntryIndex = 0; EntryIndex < MultiLineData->Lines.Num(); ++EntryIndex)
			{
				const FDialogueMultiLineEntry& Entry = MultiLineData->Lines[EntryIndex];
				CenterContent->AddSlot()
				.AutoHeight()
				.Padding(0.0f, EntryIndex > 0 ? 6.0f : 0.0f, 0.0f, 0.0f)
				[
					BuildLineEntryWidget(Entry.EntryId, EntryIndex + 1, true)
				];
			}
		}

		CenterContent->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ToolTipText(FText::FromString(TEXT("Add another line entry to this multi-line node.")))
			.OnClicked(this, &SARDialogueLineGraphNode::HandleAddMultiLineEntryClicked)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Add Line +")))
			]
		];
	}
	else
	{
		CenterContent->AddSlot()
		.AutoHeight()
		[
			BuildLineEntryWidget(FGuid(), INDEX_NONE, false)
		];
	}

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
	this->GetOrAddSlot(ENodeZone::Center)
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
					.BorderBackgroundColor(this, &SARDialogueLineGraphNode::GetTitleColor)
					.Padding(FMargin(6.0f, 2.0f))
					[
						SNew(STextBlock)
						.Text(this, &SARDialogueLineGraphNode::GetNodeTitleText)
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
					CenterContent
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
}

void SARDialogueLineGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
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

const UARDialogueEdGraphNode* SARDialogueLineGraphNode::GetDialogueNode() const
{
	return Cast<UARDialogueEdGraphNode>(GraphNode);
}

UARDialogueEdGraphNode* SARDialogueLineGraphNode::GetDialogueNodeMutable() const
{
	return Cast<UARDialogueEdGraphNode>(GraphNode);
}

const UARDialogueConversationAsset* SARDialogueLineGraphNode::GetOwningConversationAsset() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const UEdGraph* Graph = DialogueNode ? DialogueNode->GetGraph() : nullptr;
	return Graph ? Cast<UARDialogueConversationAsset>(Graph->GetOuter()) : nullptr;
}

bool SARDialogueLineGraphNode::IsMultiLineNode() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode
		&& (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::MultiLine
			|| DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::SplitLine);
}

FReply SARDialogueLineGraphNode::HandlePortraitClicked()
{
	return HandlePortraitClickedForEntry(FGuid());
}

FReply SARDialogueLineGraphNode::HandlePortraitClickedForEntry(const FGuid EntryId)
{
	const TArray<FGameplayTag> SpeakerChoices = BuildQuickSpeakerCycleList(EntryId);
	if (SpeakerChoices.IsEmpty())
	{
		return FReply::Handled();
	}

	const FGameplayTag CurrentSpeakerTag = GetSpeakerTagForEntry(EntryId);
	const int32 CurrentIndex = SpeakerChoices.IndexOfByPredicate([CurrentSpeakerTag](const FGameplayTag Candidate)
	{
		return Candidate.MatchesTagExact(CurrentSpeakerTag);
	});
	const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % SpeakerChoices.Num();
	SetLineSpeakerTagForEntry(EntryId, SpeakerChoices[NextIndex]);

	return FReply::Handled();
}

void SARDialogueLineGraphNode::HandleLineTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	(void)CommitType;
	CommitLineTextForEntry(FGuid(), NewText);
}

void SARDialogueLineGraphNode::HandleLineTextCommittedForEntry(const FText& NewText, ETextCommit::Type CommitType, const FGuid EntryId)
{
	(void)CommitType;
	CommitLineTextForEntry(EntryId, NewText);
}

FReply SARDialogueLineGraphNode::HandleAddMultiLineEntryClicked()
{
	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (DialogueNode
		&& (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::MultiLine
			|| DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::SplitLine))
	{
		DialogueNode->AddMultiLineEntry();
		UpdateGraphNode();
	}

	return FReply::Handled();
}

bool SARDialogueLineGraphNode::HandleMultiLineRowDropped(const FGuid DraggedEntryId, const FGuid TargetEntryId)
{
	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode
		|| (DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::MultiLine
			&& DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::SplitLine))
	{
		return false;
	}

	if (!DialogueNode->ReorderMultiLineEntry(DraggedEntryId, TargetEntryId))
	{
		return false;
	}

	UpdateGraphNode();
	return true;
}

FText SARDialogueLineGraphNode::GetNodeTitleText() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? DialogueNode->GetNodeTitle(ENodeTitleType::ListView) : FText::FromString(TEXT("Line"));
}

FText SARDialogueLineGraphNode::GetSpeakerTagText() const
{
	return GetSpeakerTagTextForEntry(FGuid());
}

FText SARDialogueLineGraphNode::GetSpeakerTagTextForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	if (!LineData || !LineData->Line.SpeakerTag.IsValid())
	{
		return FText::FromString(TEXT("Speaker: <unset>"));
	}

	return FText::FromString(FString::Printf(TEXT("Speaker: %s"), *LineData->Line.SpeakerTag.ToString()));
}

FText SARDialogueLineGraphNode::GetSpeakerInitialsText() const
{
	return GetSpeakerInitialsTextForEntry(FGuid());
}

FText SARDialogueLineGraphNode::GetSpeakerInitialsTextForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	if (!LineData || !LineData->Line.SpeakerTag.IsValid())
	{
		return FText::FromString(TEXT("?"));
	}

	const FString Leaf = GetTagLeaf(LineData->Line.SpeakerTag);
	return FText::FromString(Leaf.Left(2).ToUpper());
}

FText SARDialogueLineGraphNode::GetLineEditHintText() const
{
	return FText::FromString(TEXT("Line text..."));
}

EVisibility SARDialogueLineGraphNode::GetSpeakerInitialsVisibility() const
{
	return GetSpeakerInitialsVisibilityForEntry(FGuid());
}

EVisibility SARDialogueLineGraphNode::GetSpeakerInitialsVisibilityForEntry(const FGuid EntryId) const
{
	const FGameplayTag SpeakerTag = GetSpeakerTagForEntry(EntryId);
	if (!SpeakerTag.IsValid())
	{
		return EVisibility::Visible;
	}

	RefreshPortraitBrushForSpeaker(SpeakerTag);
	return SpeakersWithPortrait.Contains(SpeakerTag.GetTagName()) ? EVisibility::Collapsed : EVisibility::Visible;
}

FSlateColor SARDialogueLineGraphNode::GetTitleColor() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? FSlateColor(DialogueNode->GetNodeTitleColor()) : FSlateColor(FLinearColor::Black);
}

const FSlateBrush* SARDialogueLineGraphNode::GetPortraitBrush() const
{
	return GetPortraitBrushForEntry(FGuid());
}

const FSlateBrush* SARDialogueLineGraphNode::GetPortraitBrushForEntry(const FGuid EntryId) const
{
	const FGameplayTag SpeakerTag = GetSpeakerTagForEntry(EntryId);
	if (!SpeakerTag.IsValid())
	{
		return FAppStyle::GetBrush(TEXT("Graph.StateNode.Icon"));
	}

	RefreshPortraitBrushForSpeaker(SpeakerTag);
	const FSlateBrush* CachedBrush = PortraitBrushesBySpeaker.Find(SpeakerTag.GetTagName());
	if (CachedBrush && SpeakersWithPortrait.Contains(SpeakerTag.GetTagName()))
	{
		return CachedBrush;
	}

	return FAppStyle::GetBrush(TEXT("Graph.StateNode.Icon"));
}

void SARDialogueLineGraphNode::RefreshPortraitBrushForSpeaker(const FGameplayTag& SpeakerTag) const
{
	const FName TagName = SpeakerTag.IsValid() ? SpeakerTag.GetTagName() : NAME_None;
	if (PortraitBrushesBySpeaker.Contains(TagName))
	{
		return;
	}

	FSlateBrush PortraitBrush;
	PortraitBrush.DrawAs = ESlateBrushDrawType::Image;
	PortraitBrush.ImageSize = FVector2D(PortraitSize - 6.0f, PortraitSize - 6.0f);
	bool bHasPortraitTexture = false;

	const FARDialogueSpeakerRow* SpeakerRow = ResolveSpeakerRowForTag(SpeakerTag);
	if (SpeakerRow)
	{
		UTexture2D* PortraitTexture = SpeakerRow->DefaultPortrait.PortraitTexture.LoadSynchronous();
		if (!PortraitTexture)
		{
			for (const FSpeakerPortraitEntry& PortraitEntry : SpeakerRow->Portraits)
			{
				if (!PortraitEntry.PortraitTag.IsValid() || !PortraitEntry.PortraitTag.MatchesTagExact(SpeakerTag))
				{
					continue;
				}

				PortraitTexture = PortraitEntry.Portrait.PortraitTexture.LoadSynchronous();
				if (PortraitTexture)
				{
					break;
				}
			}
		}

		if (PortraitTexture)
		{
			PortraitBrush.SetResourceObject(PortraitTexture);
			bHasPortraitTexture = true;
		}
	}

	if (bHasPortraitTexture)
	{
		SpeakersWithPortrait.Add(TagName);
	}

	PortraitBrushesBySpeaker.Add(TagName, MoveTemp(PortraitBrush));
}

TArray<FGameplayTag> SARDialogueLineGraphNode::BuildQuickSpeakerCycleList(const FGuid EntryId) const
{
	TArray<FGameplayTag> Result;

	const UARDialogueConversationAsset* Conversation = GetOwningConversationAsset();
	if (Conversation)
	{
		auto TryAdd = [&Result](const FGameplayTag CandidateTag)
		{
			if (CandidateTag.IsValid() && !ContainsTagExact(Result, CandidateTag))
			{
				Result.Add(CandidateTag);
			}
		};

		TryAdd(Conversation->Header.PrimarySpeakerTag);
		for (const FGameplayTag ParticipantTag : Conversation->Header.ParticipatingSpeakerTags)
		{
			TryAdd(ParticipantTag);
		}
	}

	if (Result.IsEmpty())
	{
		const FGameplayTag SpeakerRoot = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Dialogue.Speaker"), false);
		if (SpeakerRoot.IsValid())
		{
			const FGameplayTagContainer SpeakerChildren = UGameplayTagsManager::Get().RequestGameplayTagChildrenInDictionary(SpeakerRoot);
			TArray<FGameplayTag> CandidateTags;
			SpeakerChildren.GetGameplayTagArray(CandidateTags);
			CandidateTags.Sort([](const FGameplayTag Lhs, const FGameplayTag Rhs)
			{
				return Lhs.ToString() < Rhs.ToString();
			});

			for (const FGameplayTag Candidate : CandidateTags)
			{
				if (!Candidate.MatchesTagExact(SpeakerRoot) && !ContainsTagExact(Result, Candidate))
				{
					Result.Add(Candidate);
				}
			}
		}
	}

	const FGameplayTag CurrentSpeakerTag = GetSpeakerTagForEntry(EntryId);
	if (CurrentSpeakerTag.IsValid() && !ContainsTagExact(Result, CurrentSpeakerTag))
	{
		Result.Insert(CurrentSpeakerTag, 0);
	}

	return Result;
}

void SARDialogueLineGraphNode::SetLineSpeakerTagForEntry(const FGuid EntryId, const FGameplayTag& NewSpeakerTag)
{
	if (!NewSpeakerTag.IsValid())
	{
		return;
	}

	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode)
	{
		return;
	}

	if (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::MultiLine
		|| DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::SplitLine)
	{
		if (EntryId.IsValid() && DialogueNode->SetMultiLineEntrySpeakerTag(EntryId, NewSpeakerTag))
		{
			UpdateGraphNode();
		}
		return;
	}

	FDialogueLineNodeData* LineData = DialogueNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>();
	if (!LineData || LineData->Line.SpeakerTag.MatchesTagExact(NewSpeakerTag))
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ARDialogue", "CycleLineSpeaker", "Cycle Dialogue Line Speaker"));
	DialogueNode->Modify();
	LineData->Line.SpeakerTag = NewSpeakerTag;

	if (UEdGraph* Graph = DialogueNode->GetGraph())
	{
		Graph->NotifyGraphChanged();
	}

	UpdateGraphNode();
}

void SARDialogueLineGraphNode::CommitLineTextForEntry(const FGuid EntryId, const FText& NewText)
{
	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode)
	{
		return;
	}

	if (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::MultiLine
		|| DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::SplitLine)
	{
		DialogueNode->SetMultiLineEntryText(EntryId, NewText);
		return;
	}

	FDialogueLineNodeData* LineData = DialogueNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>();
	if (!LineData || LineData->Line.Text.EqualTo(NewText))
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ARDialogue", "InlineEditLineText", "Inline Edit Dialogue Line Text"));
	DialogueNode->Modify();
	LineData->Line.Text = NewText;

	if (UEdGraph* Graph = DialogueNode->GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
}

FGameplayTag SARDialogueLineGraphNode::GetSpeakerTagForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	return LineData ? LineData->Line.SpeakerTag : FGameplayTag();
}

const FDialogueLineNodeData* SARDialogueLineGraphNode::GetLineDataForEntry(const FGuid EntryId) const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	if (!DialogueNode)
	{
		return nullptr;
	}

	if (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::MultiLine
		|| DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::SplitLine)
	{
		const FDialogueMultiLineNodeData* MultiLineData = DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
		if (!MultiLineData || MultiLineData->Lines.IsEmpty())
		{
			return nullptr;
		}

		if (!EntryId.IsValid())
		{
			return &MultiLineData->Lines[0].LineData;
		}

		const FDialogueMultiLineEntry* FoundEntry = MultiLineData->Lines.FindByPredicate([EntryId](const FDialogueMultiLineEntry& Entry)
		{
			return Entry.EntryId == EntryId;
		});
		return FoundEntry ? &FoundEntry->LineData : nullptr;
	}

	return DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>();
}

TSharedRef<SWidget> SARDialogueLineGraphNode::BuildLineEntryWidget(const FGuid EntryId, const int32 DisplayIndex, const bool bShowDragHandle)
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	const FText InitialText = LineData ? LineData->Line.Text : FText::GetEmpty();

	TSharedRef<SWidget> RowContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ToolTipText(FText::FromString(TEXT("Click to cycle line speaker (conversation participants first).")))
				.OnClicked(this, &SARDialogueLineGraphNode::HandlePortraitClickedForEntry, EntryId)
				.ContentPadding(0.0f)
				[
					SNew(SBox)
					.WidthOverride(PortraitSize)
					.HeightOverride(PortraitSize)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.Body")))
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SImage)
								.Image_Lambda([this, EntryId]()
								{
									return GetPortraitBrushForEntry(EntryId);
								})
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this, EntryId]()
								{
									return GetSpeakerInitialsTextForEntry(EntryId);
								})
								.Visibility_Lambda([this, EntryId]()
								{
									return GetSpeakerInitialsVisibilityForEntry(EntryId);
								})
							]
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(6.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this, EntryId]()
				{
					return GetSpeakerTagTextForEntry(EntryId);
				})
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(LineWrapWidth)
			[
				SNew(SMultiLineEditableTextBox)
				.Text(InitialText)
				.HintText(this, &SARDialogueLineGraphNode::GetLineEditHintText)
				.AutoWrapText(true)
				.OnTextCommitted(this, &SARDialogueLineGraphNode::HandleLineTextCommittedForEntry, EntryId)
			]
		];

	if (!bShowDragHandle)
	{
		return RowContent;
	}

	TSharedRef<SWidget> WithDragChrome = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 2.0f, 6.0f, 0.0f)
		.VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SARDialogueLineEntryDragHandle)
				.EntryId(EntryId)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(MultiLineIndexWidth)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d."), DisplayIndex)))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			RowContent
		];

	return SNew(SARDialogueLineEntryDragRow)
		.EntryId(EntryId)
		.OnEntryDropped(FOnDialogueLineEntryDropped::CreateSP(this, &SARDialogueLineGraphNode::HandleMultiLineRowDropped))
		[
			WithDragChrome
		];
}

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueLineGraphNodeFactory()
{
	return MakeShared<FARDialogueLineGraphNodeFactory>();
}

