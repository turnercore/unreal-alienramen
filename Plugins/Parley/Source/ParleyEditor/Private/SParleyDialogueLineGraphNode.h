#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SGraphNode.h"

struct FGraphPanelNodeFactory;
class SMultiLineEditableTextBox;
class SVerticalBox;
class UParleyConversationAsset;
class UParleyDialogueEdGraphNode;
struct FDialogueLineNodeData;

class SParleyDialogueLineGraphNode final : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SParleyDialogueLineGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UParleyDialogueEdGraphNode* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

private:
	const UParleyDialogueEdGraphNode* GetDialogueNode() const;
	UParleyDialogueEdGraphNode* GetDialogueNodeMutable() const;
	const UParleyConversationAsset* GetOwningConversationAsset() const;
	bool IsMultiLineNode() const;

	FReply HandlePortraitClicked();
	FReply HandlePortraitClickedForEntry(FGuid EntryId);
	FReply HandlePortraitMouseButtonDownForEntry(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FGuid EntryId);
	void HandleLineTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void HandleLineTextCommittedForEntry(const FText& NewText, ETextCommit::Type CommitType, FGuid EntryId);
	void HandleLineLengthCommitted(float NewLengthSeconds, ETextCommit::Type CommitType);
	void HandleLineLengthCommittedForEntry(float NewLengthSeconds, ETextCommit::Type CommitType, FGuid EntryId);
	FReply HandleAddMultiLineEntryClicked();
	bool HandleMultiLineRowDropped(FGuid DraggedEntryId, FGuid TargetEntryId);

	FText GetNodeTitleText() const;
	FText GetSpeakerTagText() const;
	FText GetSpeakerInitialsText() const;
	FText GetSpeakerTagTextForEntry(FGuid EntryId) const;
	FText GetSpeakerInitialsTextForEntry(FGuid EntryId) const;
	FText GetLineEditHintText() const;
	TOptional<float> GetLineLengthSeconds() const;
	TOptional<float> GetLineLengthSecondsForEntry(FGuid EntryId) const;
	EVisibility GetSpeakerInitialsVisibility() const;
	EVisibility GetSpeakerInitialsVisibilityForEntry(FGuid EntryId) const;
	FSlateColor GetTitleColor() const;

	const FSlateBrush* GetPortraitBrush() const;
	const FSlateBrush* GetPortraitBrushForEntry(FGuid EntryId) const;
	void RefreshPortraitBrushForSpeaker(const FGameplayTag& SpeakerTag) const;
	TArray<FGameplayTag> BuildQuickSpeakerCycleList(FGuid EntryId) const;
	TArray<FGameplayTag> BuildEmotionTagListForEntry(FGuid EntryId) const;
	void OpenEmotionPickerMenuForEntry(FGuid EntryId, const FVector2D& ScreenPosition);
	void EnsureConversationParticipantsIncludeSpeaker(const FGameplayTag& SpeakerTag);
	void SetLineSpeakerTagForEntry(FGuid EntryId, FGameplayTag NewSpeakerTag);
	void CommitLineTextForEntry(FGuid EntryId, const FText& NewText);
	void CommitLineLengthSecondsForEntry(FGuid EntryId, float NewLengthSeconds);
	FGameplayTag GetSpeakerTagForEntry(FGuid EntryId) const;
	const FDialogueLineNodeData* GetLineDataForEntry(FGuid EntryId) const;
	TSharedRef<SWidget> BuildLineEntryWidget(FGuid EntryId, int32 DisplayIndex, bool bShowDragHandle);

	TSharedPtr<SVerticalBox> LeftNodeBox;
	TSharedPtr<SVerticalBox> RightNodeBox;

	mutable TMap<FName, FSlateBrush> PortraitBrushesBySpeaker;
	mutable TSet<FName> SpeakersWithPortrait;
};

TSharedRef<FGraphPanelNodeFactory> CreateARDialogueLineGraphNodeFactory();
