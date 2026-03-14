#pragma once

#include "CoreMinimal.h"
#include "ParleyDialogueTypes.h"
#include "GameplayTagContainer.h"
#include "SGraphNode.h"

struct FGraphPanelNodeFactory;
class SGameplayTagCombo;
class SVerticalBox;
class UParleyDialogueEdGraphNode;
struct FGeometry;
struct FPointerEvent;

class SParleyDialogueInlineGraphNode final : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SParleyDialogueInlineGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UParleyDialogueEdGraphNode* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

private:
	const UParleyDialogueEdGraphNode* GetDialogueNode() const;
	UParleyDialogueEdGraphNode* GetDialogueNodeMutable() const;

	TSharedRef<SWidget> BuildInlineContent() const;
	TSharedRef<SWidget> BuildChoiceInlineContent() const;
	TSharedRef<SWidget> BuildBranchInlineContent() const;
	TSharedRef<SWidget> BuildConditionSourceInlineContent() const;
	TSharedRef<SWidget> BuildSwitchInlineContent() const;
	TSharedRef<SWidget> BuildRandomInlineContent() const;
	TSharedRef<SWidget> BuildSequenceInlineContent() const;
	TSharedRef<SWidget> BuildCharacterRouteInlineContent() const;
	TSharedRef<SWidget> BuildMultiLineInlineContent() const;
	TSharedRef<SWidget> BuildRelationshipInlineContent() const;
	TSharedRef<SWidget> BuildFactionInlineContent() const;
	void AddDynamicPinButtonIfSupported();
	float GetInlineContentMinWidth() const;
	void RefreshNodeWidget() const;
	bool HandleBranchRowDropped(EDialogueNodeType BranchNodeType, FGuid DraggedBranchId, FGuid TargetBranchId) const;

	FReply HandleAddBranchPinClicked() const;
	FReply HandleSetBranchAllClicked() const;
	FReply HandleSetBranchAnyClicked() const;

	void HandleChoiceTextCommitted(const FText& NewText, ETextCommit::Type CommitType, FGuid ChoiceBranchId) const;
	void HandleFallbackTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;
	void HandleSwitchLabelCommitted(const FText& NewText, ETextCommit::Type CommitType, FGuid BranchId) const;
	void HandleCharacterRouteTagChanged(FGameplayTag NewTag, FGuid BranchId) const;
	void HandleRandomWeightCommitted(float NewValue, ETextCommit::Type CommitType, FGuid BranchId) const;
	FReply HandleAddMultiLineEntryClicked() const;
	FReply HandleDeleteMultiLineEntryClicked(FGuid EntryId) const;
	void HandleMultiLineSpeakerTagChanged(FGameplayTag NewTag, FGuid EntryId) const;
	void HandleMultiLineTextCommitted(const FText& NewText, ETextCommit::Type CommitType, FGuid EntryId) const;
	void HandleRelationshipSpeakerTagChanged(FGameplayTag NewTag) const;
	void HandleRelationshipDeltaTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;
	void HandleFactionTagChanged(FGameplayTag NewTag) const;
	void HandleFactionDeltaTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;

	FText GetNodeTitleText() const;
	FSlateColor GetTitleColor() const;
	FString GetFactionTagFilter() const;
	const FSlateBrush* GetCharacterRoutePortraitBrush(FGameplayTag SpeakerTag) const;
	void RefreshCharacterRoutePortraitBrush(FGameplayTag SpeakerTag) const;
	FText GetRelationshipDeltaText() const;
	FText GetFactionDeltaText() const;

	TSharedPtr<SVerticalBox> LeftNodeBox;
	TSharedPtr<SVerticalBox> RightNodeBox;
	mutable TMap<FName, FSlateBrush> CharacterRoutePortraitBrushesBySpeaker;
	mutable TSet<FName> CharacterRouteSpeakersWithPortrait;
};

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueInlineGraphNodeFactory();
