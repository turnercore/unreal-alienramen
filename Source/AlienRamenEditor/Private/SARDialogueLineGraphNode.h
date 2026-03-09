#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SGraphNode.h"

struct FGraphPanelNodeFactory;
class SMultiLineEditableTextBox;
class SVerticalBox;
class UARDialogueConversationAsset;
class UARDialogueEdGraphNode;

class SARDialogueLineGraphNode final : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SARDialogueLineGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UARDialogueEdGraphNode* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

private:
	const UARDialogueEdGraphNode* GetDialogueNode() const;
	UARDialogueEdGraphNode* GetDialogueNodeMutable() const;
	const UARDialogueConversationAsset* GetOwningConversationAsset() const;

	FReply HandlePortraitClicked();
	void HandleLineTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	FText GetNodeTitleText() const;
	FText GetSpeakerTagText() const;
	FText GetSpeakerInitialsText() const;
	FText GetLineEditHintText() const;
	EVisibility GetSpeakerInitialsVisibility() const;
	FSlateColor GetTitleColor() const;

	const FSlateBrush* GetPortraitBrush() const;
	void RefreshPortraitBrush() const;
	TArray<FGameplayTag> BuildQuickSpeakerCycleList() const;
	void SetLineSpeakerTag(const FGameplayTag& NewSpeakerTag);
	void CommitLineText(const FText& NewText);

	TSharedPtr<SVerticalBox> LeftNodeBox;
	TSharedPtr<SVerticalBox> RightNodeBox;
	TSharedPtr<SMultiLineEditableTextBox> LineTextBox;

	mutable FSlateBrush PortraitBrush;
	mutable FGameplayTag CachedPortraitTag;
	mutable bool bHasPortraitTexture = false;
};

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueLineGraphNodeFactory();
