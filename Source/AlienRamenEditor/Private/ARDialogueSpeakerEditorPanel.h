#pragma once

#include "CoreMinimal.h"
#include "ARDialogueTypes.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class SEditableTextBox;
class STableViewBase;
class UARDialogueConversationAsset;
class UDataTable;

template <typename ItemType>
class SListView;

class SDialogueSpeakerEditorPanel final : public SCompoundWidget
{
public:
	struct FSpeakerEntry
	{
		FName RowName = NAME_None;
		FDialogueSpeakerRow Row;
		int32 ConversationCount = 0;
		FString ThresholdSummary;
	};

	SLATE_BEGIN_ARGS(SDialogueSpeakerEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FConversationEntry
	{
		FString Label;
		FString RelationshipBandLabel;
		FGameplayTag ConversationTag;
		FGameplayTag PrimarySpeakerTag;
		int32 Priority = 0;
		float MinimumRelationshipPoints = 0.0f;
		bool bImportant = false;
		bool bRepeatable = false;
		int32 RelationshipBand = 0;
		TWeakObjectPtr<UARDialogueConversationAsset> Asset;
	};

	struct FPortraitEntry
	{
		int32 PortraitIndex = INDEX_NONE;
		FString Label;
	};

	enum class ESpeakerSortMode : uint8
	{
		DisplayName = 0,
		SpeakerTag,
		ConversationCount
	};

	bool ResolveSpeakerDataTable(UDataTable*& OutTable, FString& OutError) const;
	void RefreshData();
	void RefreshConversationMap();
	void RefreshPortraitList();
	void ApplySpeakerFilterAndSort();
	bool ValidateConversationWithBestAvailable(UARDialogueConversationAsset* Conversation, FDialogueValidationReport& OutReport) const;
	bool ValidateSpeakerWithBestAvailable(const FDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const;
	int32 ComputeRelationshipBand(float MinimumRelationshipPoints, const TArray<float>& Thresholds) const;
	FString BuildRelationshipBandLabel(int32 BandIndex, const TArray<float>& Thresholds) const;
	FString BuildThresholdSummary(const TArray<float>& Thresholds) const;
	bool ParseThresholdsText(const FString& SourceText, TArray<float>& OutThresholds, FString& OutError) const;
	bool CommitEditedSpeakerRow(FString& OutError);
	void AppendLogLine(const FString& Message);
	void SyncSpeakerFieldsFromSelection();
	void SetSortMode(ESpeakerSortMode NewMode);
	void SetSelectedSpeakerRow(FName RowName);

	FReply HandleRefresh();
	FReply HandleNewSpeaker();
	FReply HandleDuplicateSpeaker();
	FReply HandleDeleteSpeaker();
	FReply HandleValidateSpeaker();
	FReply HandleSaveSpeaker();
	FReply HandleCreateConversation();
	FReply HandleOpenConversation();
	FReply HandleFindBrokenConversations();
	FReply HandleSortByName();
	FReply HandleSortByTag();
	FReply HandleSortByConversationCount();
	FReply HandleResetThresholds();
	FReply HandleAddPortrait();
	FReply HandleRemovePortrait();

	TSharedRef<ITableRow> OnGenerateSpeakerRow(TSharedPtr<FSpeakerEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateConversationRow(TSharedPtr<FConversationEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGeneratePortraitRow(TSharedPtr<FPortraitEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSpeakerSelectionChanged(TSharedPtr<FSpeakerEntry> Item, ESelectInfo::Type SelectInfo);
	void OnConversationDoubleClicked(TSharedPtr<FConversationEntry> Item);
	void OnPortraitSelectionChanged(TSharedPtr<FPortraitEntry> Item, ESelectInfo::Type SelectInfo);

	TSharedPtr<SListView<TSharedPtr<FSpeakerEntry>>> SpeakerListView;
	TSharedPtr<SListView<TSharedPtr<FConversationEntry>>> ConversationListView;
	TSharedPtr<SListView<TSharedPtr<FPortraitEntry>>> PortraitListView;
	TSharedPtr<SEditableTextBox> SearchTextBox;
	TSharedPtr<SEditableTextBox> FactionFilterTextBox;
	TSharedPtr<SEditableTextBox> DisplayNameTextBox;
	TSharedPtr<SEditableTextBox> DescriptionTextBox;
	TSharedPtr<SEditableTextBox> SpeakerTagTextBox;
	TSharedPtr<SEditableTextBox> FactionTagTextBox;
	TSharedPtr<SEditableTextBox> ThresholdsTextBox;
	TSharedPtr<SEditableTextBox> PortraitTagTextBox;
	TSharedPtr<SEditableTextBox> PortraitTexturePathTextBox;

	TArray<TSharedPtr<FSpeakerEntry>> AllSpeakerEntries;
	TArray<TSharedPtr<FSpeakerEntry>> FilteredSpeakerEntries;
	TArray<TSharedPtr<FConversationEntry>> ConversationEntries;
	TArray<TSharedPtr<FPortraitEntry>> PortraitEntries;
	TWeakObjectPtr<UDataTable> SpeakerDataTable;
	FName SelectedSpeakerRowName = NAME_None;
	int32 SelectedPortraitIndex = INDEX_NONE;
	ESpeakerSortMode CurrentSortMode = ESpeakerSortMode::DisplayName;
	bool bSortAscending = true;
	FString ValidationOutput;
};
