#include "SARDialogueLineGraphNode.h"

#include "ARContentLookupSettings.h"
#include "ARDialogueConversationAsset.h"
#include "ARDialogueEdGraphNode.h"
#include "ARDialogueSettings.h"
#include "ARDialogueTypes.h"
#include "ContentLookupSubsystem.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagsManager.h"
#include "Internationalization/Text.h"
#include "ScopedTransaction.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float PortraitSize = 46.0f;
	constexpr float LineWrapWidth = 280.0f;

	static const FARDialogueSpeakerRow* ResolveSpeakerRowForTag(const FGameplayTag SpeakerTag)
	{
		if (!SpeakerTag.IsValid())
		{
			return nullptr;
		}

		const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
		const UARContentLookupSettings* LookupSettings = GetDefault<UARContentLookupSettings>();
		if (!DialogueSettings
			|| !LookupSettings
			|| !DialogueSettings->SpeakerDefinitionRootTag.IsValid()
			|| LookupSettings->RegistryAsset.IsNull())
		{
			return nullptr;
		}

		UContentLookupRegistry* Registry = LookupSettings->RegistryAsset.LoadSynchronous();
		if (!Registry)
		{
			return nullptr;
		}

		const FContentLookupRoute* SpeakerRoute = Registry->Routes.FindByPredicate([DialogueSettings](const FContentLookupRoute& Route)
		{
			return Route.RootTag.MatchesTagExact(DialogueSettings->SpeakerDefinitionRootTag);
		});
		if (!SpeakerRoute)
		{
			return nullptr;
		}

		UDataTable* SpeakerTable = SpeakerRoute->DataTable.LoadSynchronous();
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

	class FARDialogueLineGraphNodeFactory final : public FGraphPanelNodeFactory
	{
	public:
		virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override
		{
			UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(InNode);
			if (!DialogueNode || DialogueNode->RuntimeNode.NodeType != EDialogueNodeType::Line)
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
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Graph.Node.TitleBackground")))
				.BorderBackgroundColor(this, &SARDialogueLineGraphNode::GetTitleColor)
				.Padding(FMargin(6.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(this, &SARDialogueLineGraphNode::GetNodeTitleText)
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
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ToolTipText(FText::FromString(TEXT("Click to cycle line speaker (conversation participants first).")))
							.OnClicked(this, &SARDialogueLineGraphNode::HandlePortraitClicked)
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
											.Image(this, &SARDialogueLineGraphNode::GetPortraitBrush)
										]
										+ SOverlay::Slot()
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Text(this, &SARDialogueLineGraphNode::GetSpeakerInitialsText)
											.Visibility(this, &SARDialogueLineGraphNode::GetSpeakerInitialsVisibility)
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
							.Text(this, &SARDialogueLineGraphNode::GetSpeakerTagText)
							.AutoWrapText(true)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(LineWrapWidth)
						[
							SAssignNew(LineTextBox, SMultiLineEditableTextBox)
							.Text(GetDialogueNode() && GetDialogueNode()->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>()
								? GetDialogueNode()->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>()->Line.Text
								: FText::GetEmpty())
							.HintText(this, &SARDialogueLineGraphNode::GetLineEditHintText)
							.AutoWrapText(true)
							.OnTextCommitted(this, &SARDialogueLineGraphNode::HandleLineTextCommitted)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Inline line edit; use details panel for advanced line options.")))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)))
						.WrapTextAt(LineWrapWidth)
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

FReply SARDialogueLineGraphNode::HandlePortraitClicked()
{
	const TArray<FGameplayTag> SpeakerChoices = BuildQuickSpeakerCycleList();
	if (SpeakerChoices.IsEmpty())
	{
		return FReply::Handled();
	}

	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>() : nullptr;
	if (!LineData)
	{
		return FReply::Handled();
	}

	const int32 CurrentIndex = SpeakerChoices.IndexOfByPredicate([&LineData](const FGameplayTag Candidate)
	{
		return Candidate.MatchesTagExact(LineData->Line.SpeakerTag);
	});
	const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % SpeakerChoices.Num();
	SetLineSpeakerTag(SpeakerChoices[NextIndex]);

	return FReply::Handled();
}

void SARDialogueLineGraphNode::HandleLineTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	(void)CommitType;
	CommitLineText(NewText);
}

FText SARDialogueLineGraphNode::GetNodeTitleText() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? DialogueNode->GetNodeTitle(ENodeTitleType::ListView) : FText::FromString(TEXT("Line"));
}

FText SARDialogueLineGraphNode::GetSpeakerTagText() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>() : nullptr;
	if (!LineData || !LineData->Line.SpeakerTag.IsValid())
	{
		return FText::FromString(TEXT("Speaker: <unset>"));
	}

	return FText::FromString(FString::Printf(TEXT("Speaker: %s"), *LineData->Line.SpeakerTag.ToString()));
}

FText SARDialogueLineGraphNode::GetSpeakerInitialsText() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>() : nullptr;
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
	return bHasPortraitTexture ? EVisibility::Collapsed : EVisibility::Visible;
}

FSlateColor SARDialogueLineGraphNode::GetTitleColor() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	return DialogueNode ? FSlateColor(DialogueNode->GetNodeTitleColor()) : FSlateColor(FLinearColor::Black);
}

const FSlateBrush* SARDialogueLineGraphNode::GetPortraitBrush() const
{
	RefreshPortraitBrush();
	return bHasPortraitTexture
		? &PortraitBrush
		: FAppStyle::GetBrush(TEXT("Graph.StateNode.Icon"));
}

void SARDialogueLineGraphNode::RefreshPortraitBrush() const
{
	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>() : nullptr;
	const FGameplayTag SpeakerTag = LineData ? LineData->Line.SpeakerTag : FGameplayTag();
	if (CachedPortraitTag.MatchesTagExact(SpeakerTag))
	{
		return;
	}

	CachedPortraitTag = SpeakerTag;
	bHasPortraitTexture = false;
	PortraitBrush = FSlateBrush();
	PortraitBrush.DrawAs = ESlateBrushDrawType::Image;
	PortraitBrush.ImageSize = FVector2D(PortraitSize - 6.0f, PortraitSize - 6.0f);

	const FARDialogueSpeakerRow* SpeakerRow = ResolveSpeakerRowForTag(SpeakerTag);
	if (!SpeakerRow)
	{
		return;
	}

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

TArray<FGameplayTag> SARDialogueLineGraphNode::BuildQuickSpeakerCycleList() const
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

	const UARDialogueEdGraphNode* DialogueNode = GetDialogueNode();
	const FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>() : nullptr;
	if (LineData && LineData->Line.SpeakerTag.IsValid() && !ContainsTagExact(Result, LineData->Line.SpeakerTag))
	{
		Result.Insert(LineData->Line.SpeakerTag, 0);
	}

	return Result;
}

void SARDialogueLineGraphNode::SetLineSpeakerTag(const FGameplayTag& NewSpeakerTag)
{
	if (!NewSpeakerTag.IsValid())
	{
		return;
	}

	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>() : nullptr;
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
}

void SARDialogueLineGraphNode::CommitLineText(const FText& NewText)
{
	UARDialogueEdGraphNode* DialogueNode = GetDialogueNodeMutable();
	FDialogueLineNodeData* LineData = DialogueNode ? DialogueNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>() : nullptr;
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

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueLineGraphNodeFactory()
{
	return MakeShared<FARDialogueLineGraphNodeFactory>();
}
