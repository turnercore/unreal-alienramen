#pragma once

#include "CoreMinimal.h"
#include "ARDialogueTypes.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class SEditableTextBox;
class SObjectPropertyEntryBox;
class STableViewBase;
class UARDialogueConversationAsset;
class UDataTable;
class UTexture2D;
struct FAssetData;

template <typename ItemType>
class SListView;

class SDialogueSpeakerEditorPanel final : public SCompoundWidget
{
public:
	struct FSpeakerEntry
	{
		FName RowName = NAME_None;
		FARDialogueSpeakerRow Row;
		int32 ConversationCount = 0;
		FString ThresholdSummary;
	};

	SLATE_BEGIN_ARGS(SDialogueSpeakerEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FConversationEntry
	{
		bool bIsBandHeader = false;
		FString DisplayTitle;
		FString Label;
		FString RelationshipBandLabel;
		FString GateSummary;
		FString MutationSummary;
		FString ChainSummary;
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

	struct FThresholdEntry
	{
		int32 ThresholdIndex = INDEX_NONE;
		float Value = 0.0f;
	};

	enum class ESpeakerSortMode : uint8
	{
		DisplayName = 0,
		ConversationCount
	};

	bool ResolveSpeakerDataTable(UDataTable*& OutTable, FString& OutError) const;
	void RefreshData();
	void RefreshConversationMap();
	void RefreshPortraitList();
	void ApplySpeakerFilterAndSort();
	bool ValidateConversationWithBestAvailable(UARDialogueConversationAsset* Conversation, FDialogueValidationReport& OutReport) const;
	bool ValidateSpeakerWithBestAvailable(const FARDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const;
	int32 ComputeRelationshipBand(float MinimumRelationshipPoints, const TArray<float>& Thresholds) const;
	FString BuildRelationshipBandLabel(int32 BandIndex, const TArray<float>& Thresholds) const;
	FString BuildThresholdSummary(const TArray<float>& Thresholds) const;
	bool CommitEditedSpeakerRow(FString& OutError);
	FGameplayTag GetEditedSpeakerTag() const;
	void OnEditedSpeakerTagChanged(FGameplayTag NewTag);
	FGameplayTag GetEditedFactionTag() const;
	void OnEditedFactionTagChanged(FGameplayTag NewTag);
	FString GetEditedDefaultPortraitTexturePath() const;
	void OnEditedDefaultPortraitTextureChanged(const FAssetData& AssetData);
	FGameplayTag GetEditedPortraitTag() const;
	void OnEditedPortraitTagChanged(FGameplayTag NewTag);
	FString GetEditedPortraitTexturePath() const;
	void OnEditedPortraitTextureChanged(const FAssetData& AssetData);
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
	FReply HandleSortByConversationCount();
	FReply HandleResetThresholds();
	FReply HandleAddThreshold();
	FReply HandleRemoveThreshold();
	FReply HandleMoveThresholdUp();
	FReply HandleMoveThresholdDown();
	FReply HandleAddPortrait();
	FReply HandleRemovePortrait();

	TSharedRef<ITableRow> OnGenerateSpeakerRow(TSharedPtr<FSpeakerEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateConversationRow(TSharedPtr<FConversationEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGeneratePortraitRow(TSharedPtr<FPortraitEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSpeakerSelectionChanged(TSharedPtr<FSpeakerEntry> Item, ESelectInfo::Type SelectInfo);
	void OnConversationDoubleClicked(TSharedPtr<FConversationEntry> Item);
	void OnPortraitSelectionChanged(TSharedPtr<FPortraitEntry> Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> OnGenerateThresholdRow(TSharedPtr<FThresholdEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnThresholdSelectionChanged(TSharedPtr<FThresholdEntry> Item, ESelectInfo::Type SelectInfo);
	void RefreshThresholdList();
	void SetEditedThresholdValue(float NewValue);

	TSharedPtr<SListView<TSharedPtr<FSpeakerEntry>>> SpeakerListView;
	TSharedPtr<SListView<TSharedPtr<FConversationEntry>>> ConversationListView;
	TSharedPtr<SListView<TSharedPtr<FPortraitEntry>>> PortraitListView;
	TSharedPtr<SListView<TSharedPtr<FThresholdEntry>>> ThresholdListView;
	TSharedPtr<SEditableTextBox> SearchTextBox;
	TSharedPtr<SEditableTextBox> DisplayNameTextBox;
	TSharedPtr<SEditableTextBox> DescriptionTextBox;
	TSharedPtr<SObjectPropertyEntryBox> DefaultPortraitTexturePicker;
	TSharedPtr<SObjectPropertyEntryBox> PortraitTexturePicker;

	TArray<TSharedPtr<FSpeakerEntry>> AllSpeakerEntries;
	TArray<TSharedPtr<FSpeakerEntry>> FilteredSpeakerEntries;
	TArray<TSharedPtr<FConversationEntry>> ConversationEntries;
	TArray<TSharedPtr<FPortraitEntry>> PortraitEntries;
	TWeakObjectPtr<UDataTable> SpeakerDataTable;
	FGameplayTag EditedSpeakerTag;
	FGameplayTag EditedFactionTag;
	TSoftObjectPtr<UTexture2D> EditedDefaultPortraitTexture;
	FGameplayTag EditedPortraitTag;
	TSoftObjectPtr<UTexture2D> EditedPortraitTexture;
	TArray<float> EditedRelationshipThresholds;
	TArray<TSharedPtr<FThresholdEntry>> ThresholdEntries;
	FName SelectedSpeakerRowName = NAME_None;
	int32 SelectedPortraitIndex = INDEX_NONE;
	int32 SelectedThresholdIndex = INDEX_NONE;
	ESpeakerSortMode CurrentSortMode = ESpeakerSortMode::DisplayName;
	bool bSortAscending = true;
	bool bIgnoreSpeakerSelectionChanged = false;
	FString ValidationOutput;
};
