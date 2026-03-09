#pragma once

#include "CoreMinimal.h"
#include "ARDialogueTypes.h"
#include "GameplayTagContainer.h"
#include "SGraphNode.h"

struct FGraphPanelNodeFactory;
class SGameplayTagCombo;
class SVerticalBox;
class UARDialogueEdGraphNode;
struct FGeometry;
struct FPointerEvent;

class SARDialogueInlineGraphNode final : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SARDialogueInlineGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UARDialogueEdGraphNode* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

private:
	const UARDialogueEdGraphNode* GetDialogueNode() const;
	UARDialogueEdGraphNode* GetDialogueNodeMutable() const;

	TSharedRef<SWidget> BuildInlineContent() const;
	TSharedRef<SWidget> BuildChoiceInlineContent() const;
	TSharedRef<SWidget> BuildSwitchInlineContent() const;
	TSharedRef<SWidget> BuildRandomInlineContent() const;
	TSharedRef<SWidget> BuildSequenceInlineContent() const;
	TSharedRef<SWidget> BuildRelationshipInlineContent() const;
	TSharedRef<SWidget> BuildFactionInlineContent() const;
	void AddDynamicPinButtonIfSupported();
	float GetInlineContentMinWidth() const;
	void RefreshNodeWidget() const;
	bool HandleBranchRowDropped(EDialogueNodeType BranchNodeType, FGuid DraggedBranchId, FGuid TargetBranchId) const;

	FReply HandleAddBranchPinClicked() const;

	void HandleChoiceTextCommitted(const FText& NewText, ETextCommit::Type CommitType, FGuid ChoiceBranchId) const;
	void HandleFallbackTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;
	void HandleSwitchLabelCommitted(const FText& NewText, ETextCommit::Type CommitType, FGuid BranchId) const;
	void HandleRandomWeightCommitted(float NewValue, ETextCommit::Type CommitType, FGuid BranchId) const;
	void HandleRelationshipSpeakerTagChanged(FGameplayTag NewTag) const;
	void HandleRelationshipDeltaTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;
	void HandleFactionTagChanged(FGameplayTag NewTag) const;
	void HandleFactionDeltaTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;

	FText GetNodeTitleText() const;
	FSlateColor GetTitleColor() const;
	FString GetFactionTagFilter() const;
	FText GetRelationshipDeltaText() const;
	FText GetFactionDeltaText() const;

	TSharedPtr<SVerticalBox> LeftNodeBox;
	TSharedPtr<SVerticalBox> RightNodeBox;
};

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueInlineGraphNodeFactory();
