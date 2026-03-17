#include "SParleyDialogueLineGraphNode.h"

#include "ParleyConversationAsset.h"
#include "ParleyDialogueEdGraphNode.h"
#include "ParleyDialogueSettings.h"
#include "ParleyDialogueTypes.h"
#include "TagKeySubsystem.h"
#include "TagKeyEditorHelpers.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameplayTagsManager.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "ScopedTransaction.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
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
	constexpr float InlineLengthWidth = 78.0f;

	static const FParleySpeakerRow* ResolveSpeakerRowForTag_LineNode(const FGameplayTag SpeakerTag)
	{
		if (!SpeakerTag.IsValid())
		{
			return nullptr;
		}

		const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
		if (!DialogueSettings || !DialogueSettings->SpeakerDefinitionRootTag.IsValid())
		{
			return nullptr;
		}

		UDataTable* SpeakerTable = nullptr;
		FString LookupError;
		if (!FTagKeyEditorHelpers::TryResolveDataTableForRootTag(DialogueSettings->SpeakerDefinitionRootTag, SpeakerTable, LookupError))
		{
			return nullptr;
		}

		if (!SpeakerTable || SpeakerTable->GetRowStruct() != FParleySpeakerRow::StaticStruct())
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
			if (const FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(RowName, TEXT("DialogueLineGraphNode"), false))
			{
				return Row;
			}

			const FString ParentTagPath = TagString.Left(DotIndex);
			Candidate = UGameplayTagsManager::Get().RequestGameplayTag(FName(*ParentTagPath), false);
		}

		return nullptr;
	}

	static FGameplayTag NormalizeSpeakerTagForCycle(const FGameplayTag SpeakerTag)
	{
		if (!SpeakerTag.IsValid())
		{
			return FGameplayTag();
		}

		if (const FParleySpeakerRow* SpeakerRow = ResolveSpeakerRowForTag_LineNode(SpeakerTag))
		{
			if (SpeakerRow->SpeakerTag.IsValid())
			{
				return SpeakerRow->SpeakerTag;
			}
		}

		return SpeakerTag;
	}

	static bool ContainsTagExact_LineNode(const TArray<FGameplayTag>& Tags, const FGameplayTag Tag)
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

	class FParleyDialogueLineEntryDragDropOp final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FParleyDialogueLineEntryDragDropOp, FDecoratedDragDropOp)

		static TSharedRef<FParleyDialogueLineEntryDragDropOp> New(const FGuid InEntryId)
		{
			TSharedRef<FParleyDialogueLineEntryDragDropOp> Op = MakeShared<FParleyDialogueLineEntryDragDropOp>();
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
			const TSharedPtr<FParleyDialogueLineEntryDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FParleyDialogueLineEntryDragDropOp>();
			return DragOperation.IsValid() ? FReply::Handled() : FReply::Unhandled();
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			(void)MyGeometry;
			const TSharedPtr<FParleyDialogueLineEntryDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FParleyDialogueLineEntryDragDropOp>();
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
				return FReply::Handled().BeginDragDrop(FParleyDialogueLineEntryDragDropOp::New(EntryId));
			}
			return FReply::Unhandled();
		}

	private:
		FGuid EntryId;
	};

	class FParleyDialogueLineGraphNodeFactory final : public FGraphPanelNodeFactory
	{
	public:
		virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override
		{
			UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(InNode);
			if (!DialogueNode
				|| (DialogueNode->EditorNodeType != EDialogueEditorNodeType::Line
					&& DialogueNode->EditorNodeType != EDialogueEditorNodeType::MultiLine
					&& DialogueNode->EditorNodeType != EDialogueEditorNodeType::SplitLine))
			{
				return nullptr;
			}

			return SNew(SParleyDialogueLineGraphNode, DialogueNode);
		}
	};
}

void SParleyDialogueLineGraphNode::Construct(const FArguments& InArgs, UParleyDialogueEdGraphNode* InNode)
{
	(void)InArgs;
	this->GraphNode = InNode;
	this->SetCursor(EMouseCursor::CardinalCross);
	this->UpdateGraphNode();
}

void SParleyDialogueLineGraphNode::UpdateGraphNode()
{
	InputPins.Reset();
	OutputPins.Reset();
	LeftNodeBox.Reset();
	RightNodeBox.Reset();

	TSharedRef<SVerticalBox> CenterContent = SNew(SVerticalBox);
	if (IsMultiLineNode())
	{
		const UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNode();
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
			.OnClicked(this, &SParleyDialogueLineGraphNode::HandleAddMultiLineEntryClicked)
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
		.ToolTipText_Lambda([this]()
		{
			const UParleyDialogueEdGraphNode* Node = GetDialogueNode();
			return Node ? Node->GetTooltipText() : FText::FromString(TEXT("Dialogue node."));
		})
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
					.BorderBackgroundColor(this, &SParleyDialogueLineGraphNode::GetTitleColor)
					.Padding(FMargin(6.0f, 2.0f))
					[
						SNew(STextBlock)
						.Text(this, &SParleyDialogueLineGraphNode::GetNodeTitleText)
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

void SParleyDialogueLineGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
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

const UParleyDialogueEdGraphNode* SParleyDialogueLineGraphNode::GetDialogueNode() const
{
	return Cast<UParleyDialogueEdGraphNode>(GraphNode);
}

UParleyDialogueEdGraphNode* SParleyDialogueLineGraphNode::GetDialogueNodeMutable() const
{
	return Cast<UParleyDialogueEdGraphNode>(GraphNode);
}

const UParleyConversationAsset* SParleyDialogueLineGraphNode::GetOwningConversationAsset() const
{
	const UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const UEdGraph* Graph = DialogueNode ? DialogueNode->GetGraph() : nullptr;
	return Graph ? Cast<UParleyConversationAsset>(Graph->GetOuter()) : nullptr;
}

bool SParleyDialogueLineGraphNode::IsMultiLineNode() const
{
	const UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode
		&& (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
			|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine);
}

FReply SParleyDialogueLineGraphNode::HandlePortraitClicked()
{
	return HandlePortraitClickedForEntry(FGuid());
}

FReply SParleyDialogueLineGraphNode::HandlePortraitClickedForEntry(const FGuid EntryId)
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

FReply SParleyDialogueLineGraphNode::HandlePortraitMouseButtonDownForEntry(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, const FGuid EntryId)
{
	(void)MyGeometry;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return HandlePortraitClickedForEntry(EntryId);
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OpenEmotionPickerMenuForEntry(EntryId, MouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SParleyDialogueLineGraphNode::HandleLineTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	(void)CommitType;
	CommitLineTextForEntry(FGuid(), NewText);
}

void SParleyDialogueLineGraphNode::HandleLineTextCommittedForEntry(const FText& NewText, ETextCommit::Type CommitType, const FGuid EntryId)
{
	(void)CommitType;
	CommitLineTextForEntry(EntryId, NewText);
}

void SParleyDialogueLineGraphNode::HandleLineLengthCommitted(const float NewLengthSeconds, const ETextCommit::Type CommitType)
{
	(void)CommitType;
	CommitLineLengthSecondsForEntry(FGuid(), NewLengthSeconds);
}

void SParleyDialogueLineGraphNode::HandleLineLengthCommittedForEntry(
	const float NewLengthSeconds,
	const ETextCommit::Type CommitType,
	const FGuid EntryId)
{
	(void)CommitType;
	CommitLineLengthSecondsForEntry(EntryId, NewLengthSeconds);
}

FReply SParleyDialogueLineGraphNode::HandleAddMultiLineEntryClicked()
{
	UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (DialogueNode
		&& (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
			|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine))
	{
		DialogueNode->AddMultiLineEntry();
		UpdateGraphNode();
	}

	return FReply::Handled();
}

bool SParleyDialogueLineGraphNode::HandleMultiLineRowDropped(const FGuid DraggedEntryId, const FGuid TargetEntryId)
{
	UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode
		|| (DialogueNode->EditorNodeType != EDialogueEditorNodeType::MultiLine
			&& DialogueNode->EditorNodeType != EDialogueEditorNodeType::SplitLine))
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

FText SParleyDialogueLineGraphNode::GetNodeTitleText() const
{
	const UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? DialogueNode->GetNodeTitle(ENodeTitleType::ListView) : FText::FromString(TEXT("Line"));
}

FText SParleyDialogueLineGraphNode::GetSpeakerTagText() const
{
	return GetSpeakerTagTextForEntry(FGuid());
}

FText SParleyDialogueLineGraphNode::GetSpeakerTagTextForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	if (!LineData || !LineData->Line.SpeakerTag.IsValid())
	{
		return FText::FromString(TEXT("Speaker: <unset>"));
	}

	return FText::FromString(FString::Printf(TEXT("Speaker: %s"), *LineData->Line.SpeakerTag.ToString()));
}

FText SParleyDialogueLineGraphNode::GetSpeakerInitialsText() const
{
	return GetSpeakerInitialsTextForEntry(FGuid());
}

FText SParleyDialogueLineGraphNode::GetSpeakerInitialsTextForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	if (!LineData || !LineData->Line.SpeakerTag.IsValid())
	{
		return FText::FromString(TEXT("?"));
	}

	const FString Leaf = GetTagLeaf(LineData->Line.SpeakerTag);
	return FText::FromString(Leaf.Left(2).ToUpper());
}

FText SParleyDialogueLineGraphNode::GetLineEditHintText() const
{
	return FText::FromString(TEXT("Line text..."));
}

TOptional<float> SParleyDialogueLineGraphNode::GetLineLengthSeconds() const
{
	return GetLineLengthSecondsForEntry(FGuid());
}

TOptional<float> SParleyDialogueLineGraphNode::GetLineLengthSecondsForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	return LineData ? TOptional<float>(LineData->Line.LengthSeconds) : TOptional<float>();
}

EVisibility SParleyDialogueLineGraphNode::GetSpeakerInitialsVisibility() const
{
	return GetSpeakerInitialsVisibilityForEntry(FGuid());
}

EVisibility SParleyDialogueLineGraphNode::GetSpeakerInitialsVisibilityForEntry(const FGuid EntryId) const
{
	const FGameplayTag SpeakerTag = GetSpeakerTagForEntry(EntryId);
	if (!SpeakerTag.IsValid())
	{
		return EVisibility::Visible;
	}

	RefreshPortraitBrushForSpeaker(SpeakerTag);
	return SpeakersWithPortrait.Contains(SpeakerTag.GetTagName()) ? EVisibility::Collapsed : EVisibility::Visible;
}

FSlateColor SParleyDialogueLineGraphNode::GetTitleColor() const
{
	const UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? FSlateColor(DialogueNode->GetNodeTitleColor()) : FSlateColor(FLinearColor::Black);
}

const FSlateBrush* SParleyDialogueLineGraphNode::GetPortraitBrush() const
{
	return GetPortraitBrushForEntry(FGuid());
}

const FSlateBrush* SParleyDialogueLineGraphNode::GetPortraitBrushForEntry(const FGuid EntryId) const
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

void SParleyDialogueLineGraphNode::RefreshPortraitBrushForSpeaker(const FGameplayTag& SpeakerTag) const
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

	const FParleySpeakerRow* SpeakerRow = ResolveSpeakerRowForTag_LineNode(SpeakerTag);
	if (SpeakerRow)
	{
		UTexture2D* PortraitTexture = nullptr;
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

		if (!PortraitTexture && SpeakerRow->SpeakerTag.IsValid())
		{
			const FString DefaultTagPath = FString::Printf(TEXT("%s.Default"), *SpeakerRow->SpeakerTag.ToString());
			const FGameplayTag DefaultPortraitTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*DefaultTagPath), false);
			if (DefaultPortraitTag.IsValid())
			{
				for (const FSpeakerPortraitEntry& PortraitEntry : SpeakerRow->Portraits)
				{
					if (!PortraitEntry.PortraitTag.IsValid() || !PortraitEntry.PortraitTag.MatchesTagExact(DefaultPortraitTag))
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
		}

		if (!PortraitTexture)
		{
			PortraitTexture = SpeakerRow->DefaultPortrait.PortraitTexture.LoadSynchronous();
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

TArray<FGameplayTag> SParleyDialogueLineGraphNode::BuildQuickSpeakerCycleList(const FGuid EntryId) const
{
	TArray<FGameplayTag> Result;

	const UParleyConversationAsset* Conversation = GetOwningConversationAsset();
	if (Conversation)
	{
		auto TryAdd = [&Result](const FGameplayTag CandidateTag)
		{
			const FGameplayTag NormalizedTag = NormalizeSpeakerTagForCycle(CandidateTag);
			if (NormalizedTag.IsValid() && !ContainsTagExact_LineNode(Result, NormalizedTag))
			{
				Result.Add(NormalizedTag);
			}
		};

		TryAdd(Conversation->Header.PrimarySpeakerTag);
		for (const FGameplayTag ParticipantTag : Conversation->Header.ParticipatingSpeakerTags)
		{
			TryAdd(ParticipantTag);
		}

		const UEdGraph* ConversationGraph = Cast<UEdGraph>(Conversation->EditorGraph);
		if (ConversationGraph)
		{
			for (const UEdGraphNode* CandidateNode : ConversationGraph->Nodes)
			{
				const UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(CandidateNode);
				if (!DialogueNode)
				{
					continue;
				}

				if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::Line)
				{
					if (const FDialogueLineNodeData* LineData = DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>())
					{
						TryAdd(LineData->Line.SpeakerTag);
					}
				}
				else if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
					|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine)
				{
					if (const FDialogueMultiLineNodeData* MultiLineData = DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>())
					{
						for (const FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
						{
							TryAdd(Entry.LineData.Line.SpeakerTag);
						}
					}
				}
			}
		}
	}

	if (Result.IsEmpty())
	{
		const FGameplayTag SpeakerRoot = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Speaker"), false);
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
					const FGameplayTag NormalizedCandidate = NormalizeSpeakerTagForCycle(Candidate);
					if (!NormalizedCandidate.IsValid()
						|| NormalizedCandidate.MatchesTagExact(SpeakerRoot)
						|| ContainsTagExact_LineNode(Result, NormalizedCandidate))
					{
						continue;
					}
					Result.Add(NormalizedCandidate);
				}
			}
		}

	const FGameplayTag CurrentSpeakerTag = NormalizeSpeakerTagForCycle(GetSpeakerTagForEntry(EntryId));
	if (CurrentSpeakerTag.IsValid() && !ContainsTagExact_LineNode(Result, CurrentSpeakerTag))
	{
		Result.Insert(CurrentSpeakerTag, 0);
	}

	return Result;
}

TArray<FGameplayTag> SParleyDialogueLineGraphNode::BuildEmotionTagListForEntry(const FGuid EntryId) const
{
	TArray<FGameplayTag> Result;
	const FGameplayTag CurrentSpeakerTag = GetSpeakerTagForEntry(EntryId);
	if (!CurrentSpeakerTag.IsValid())
	{
		return Result;
	}

	const FGameplayTag BaseSpeakerTag = NormalizeSpeakerTagForCycle(CurrentSpeakerTag);
	if (!BaseSpeakerTag.IsValid())
	{
		return Result;
	}

	const FGameplayTagContainer ChildTags = UGameplayTagsManager::Get().RequestGameplayTagChildrenInDictionary(BaseSpeakerTag);
	TArray<FGameplayTag> ChildTagArray;
	ChildTags.GetGameplayTagArray(ChildTagArray);
	for (const FGameplayTag ChildTag : ChildTagArray)
	{
		if (!ChildTag.IsValid() || ChildTag.MatchesTagExact(BaseSpeakerTag) || !ChildTag.MatchesTag(BaseSpeakerTag))
		{
			continue;
		}
		if (!ContainsTagExact_LineNode(Result, ChildTag))
		{
			Result.Add(ChildTag);
		}
	}

	if (CurrentSpeakerTag.IsValid() && !CurrentSpeakerTag.MatchesTagExact(BaseSpeakerTag) && !ContainsTagExact_LineNode(Result, CurrentSpeakerTag))
	{
		Result.Add(CurrentSpeakerTag);
	}

	Result.Sort([](const FGameplayTag Lhs, const FGameplayTag Rhs)
	{
		return Lhs.ToString() < Rhs.ToString();
	});
	return Result;
}

void SParleyDialogueLineGraphNode::OpenEmotionPickerMenuForEntry(const FGuid EntryId, const FVector2D& ScreenPosition)
{
	const TArray<FGameplayTag> EmotionTags = BuildEmotionTagListForEntry(EntryId);
	if (EmotionTags.IsEmpty())
	{
		return;
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	for (const FGameplayTag EmotionTag : EmotionTags)
	{
		const FString TagLabel = EmotionTag.ToString();
		MenuBuilder.AddMenuEntry(
			FText::FromString(TagLabel),
			FText::FromString(FString::Printf(TEXT("Set line speaker tag to '%s'."), *TagLabel)),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SParleyDialogueLineGraphNode::SetLineSpeakerTagForEntry, EntryId, EmotionTag)));
	}

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		ScreenPosition,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void SParleyDialogueLineGraphNode::EnsureConversationParticipantsIncludeSpeaker(const FGameplayTag& SpeakerTag)
{
	const FGameplayTag NormalizedSpeakerTag = NormalizeSpeakerTagForCycle(SpeakerTag);
	if (!NormalizedSpeakerTag.IsValid())
	{
		return;
	}

	UParleyConversationAsset* Conversation = const_cast<UParleyConversationAsset*>(GetOwningConversationAsset());
	if (!Conversation)
	{
		return;
	}

	if (ContainsTagExact_LineNode(Conversation->Header.ParticipatingSpeakerTags, NormalizedSpeakerTag))
	{
		return;
	}

	Conversation->Modify();
	Conversation->Header.ParticipatingSpeakerTags.Add(NormalizedSpeakerTag);
	Conversation->MarkPackageDirty();
}

void SParleyDialogueLineGraphNode::SetLineSpeakerTagForEntry(const FGuid EntryId, FGameplayTag NewSpeakerTag)
{
	if (!NewSpeakerTag.IsValid())
	{
		return;
	}

	UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode)
	{
		return;
	}

	if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
		|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine)
	{
		if (EntryId.IsValid() && DialogueNode->SetMultiLineEntrySpeakerTag(EntryId, NewSpeakerTag))
		{
			EnsureConversationParticipantsIncludeSpeaker(NewSpeakerTag);
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

	EnsureConversationParticipantsIncludeSpeaker(NewSpeakerTag);
	UpdateGraphNode();
}

void SParleyDialogueLineGraphNode::CommitLineTextForEntry(const FGuid EntryId, const FText& NewText)
{
	UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode)
	{
		return;
	}

	if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
		|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine)
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

void SParleyDialogueLineGraphNode::CommitLineLengthSecondsForEntry(const FGuid EntryId, const float NewLengthSeconds)
{
	UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	if (!DialogueNode)
	{
		return;
	}

	if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
		|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine)
	{
		DialogueNode->SetMultiLineEntryLengthSeconds(EntryId, NewLengthSeconds);
		return;
	}

	DialogueNode->SetLineLengthSeconds(NewLengthSeconds);
}

FGameplayTag SParleyDialogueLineGraphNode::GetSpeakerTagForEntry(const FGuid EntryId) const
{
	const FDialogueLineNodeData* LineData = GetLineDataForEntry(EntryId);
	return LineData ? LineData->Line.SpeakerTag : FGameplayTag();
}

const FDialogueLineNodeData* SParleyDialogueLineGraphNode::GetLineDataForEntry(const FGuid EntryId) const
{
	const UParleyDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	if (!DialogueNode)
	{
		return nullptr;
	}

	if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::MultiLine
		|| DialogueNode->EditorNodeType == EDialogueEditorNodeType::SplitLine)
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

TSharedRef<SWidget> SParleyDialogueLineGraphNode::BuildLineEntryWidget(const FGuid EntryId, const int32 DisplayIndex, const bool bShowDragHandle)
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
					SNew(SBorder)
					.ToolTipText(FText::FromString(TEXT("Left-click: cycle speaker. Right-click: pick emotion tag for current speaker.")))
					.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
					.OnMouseButtonDown(this, &SParleyDialogueLineGraphNode::HandlePortraitMouseButtonDownForEntry, EntryId)
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
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.WidthOverride(LineWrapWidth)
				[
					SNew(SMultiLineEditableTextBox)
					.Text(InitialText)
					.HintText(this, &SParleyDialogueLineGraphNode::GetLineEditHintText)
					.AutoWrapText(true)
					.OnTextCommitted(this, &SParleyDialogueLineGraphNode::HandleLineTextCommittedForEntry, EntryId)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Sec")))
					.ToolTipText(FText::FromString(TEXT("Authored line length in seconds when no native audio determines timing.")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(InlineLengthWidth)
					[
						SNew(SNumericEntryBox<float>)
						.AllowSpin(true)
						.MinValue(0.0f)
						.MinSliderValue(0.0f)
						.Delta(0.1f)
						.Value_Lambda([this, EntryId]()
						{
							return GetLineLengthSecondsForEntry(EntryId);
						})
						.OnValueCommitted(this, &SParleyDialogueLineGraphNode::HandleLineLengthCommittedForEntry, EntryId)
					]
				]
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
		.OnEntryDropped(FOnDialogueLineEntryDropped::CreateSP(this, &SParleyDialogueLineGraphNode::HandleMultiLineRowDropped))
		[
			WithDragChrome
		];
}

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueLineGraphNodeFactory()
{
	return MakeShared<FParleyDialogueLineGraphNodeFactory>();
}

