#pragma once

#include "CoreMinimal.h"
#include "ARDialogueTypes.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class SEditableTextBox;
class SSearchBox;
class SBox;
class SObjectPropertyEntryBox;
class STableViewBase;
class SExpandableArea;
class UARDialogueConversationAsset;
class UDataTable;
class UFont;
class UTexture2D;
struct FAssetData;
struct FGeometry;
struct FKeyEvent;

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
		int32 EmotionCount = 1;
		FString ThresholdSummary;
	};

	SLATE_BEGIN_ARGS(SDialogueSpeakerEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SDialogueSpeakerEditorPanel() override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	struct FConversationEntry
	{
		bool bIsBandHeader = false;
		bool bIsLevelZeroDropTarget = false;
		FString DisplayTitle;
		FString Label;
		FString RelationshipBandLabel;
		FString UnlocksSummary;
		FString RequiresSummary;
		FGameplayTagContainer RequiredTags;
		FString ConversationTagFilter;
		TArray<FGameplayTag> LockedByConversationTags;
		FGameplayTag ConversationTag;
		FGameplayTag PrimarySpeakerTag;
		int32 Priority = 0;
		int32 LockedConditionCount = 0;
		int32 BlockedConditionCount = 0;
		int32 AddedTagCount = 0;
		int32 RemovedTagCount = 0;
		int32 RequiredTagCount = 0;
		float MinimumRelationshipPoints = 0.0f;
		bool bImportant = false;
		bool bRepeatable = false;
		bool bSeenByGameBlocksReoffer = false;
		bool bSeenByPlayerBlocksReoffer = false;
		bool bCompletedByGameBlocksReoffer = false;
		bool bCompletedByGame = false;
		int32 RelationshipBand = 0;
		TWeakObjectPtr<UARDialogueConversationAsset> Asset;
	};

	struct FPortraitEntry
	{
		int32 PortraitIndex = INDEX_NONE;
		FGameplayTag PortraitTag;
		TSoftObjectPtr<UTexture2D> PortraitTexture;
		FString Label;
		FSlateBrush PortraitBrush;
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
	bool BuildEditedSpeakerRow(FARDialogueSpeakerRow& OutRow, FString& OutError) const;
	bool CommitEditedSpeakerRow(FString& OutError);
	FGameplayTag GetEditedSpeakerTag() const;
	void OnEditedSpeakerTagChanged(FGameplayTag NewTag);
	FGameplayTag GetEditedFactionTag() const;
	void OnEditedFactionTagChanged(FGameplayTag NewTag);
	FString GetEditedLineFontPath() const;
	void OnEditedLineFontChanged(const FAssetData& AssetData);
	FString GetEditedDefaultPortraitTexturePath() const;
	void OnEditedDefaultPortraitTextureChanged(const FAssetData& AssetData);
	FGameplayTag GetEditedPortraitTag() const;
	void OnEditedPortraitTagChanged(FGameplayTag NewTag);
	FString GetEditedPortraitTexturePath() const;
	void OnEditedPortraitTextureChanged(const FAssetData& AssetData);
	FString GetEmotionTagFilter() const;
	void RebuildEmotionTagCombo();
	void EnsureSpeakerDefaultEmotionTag(const FGameplayTag& SpeakerTag);
	void AppendLogLine(const FString& Message);
	void SyncSpeakerFieldsFromSelection();
	void SetSortMode(ESpeakerSortMode NewMode);
	void SetSelectedSpeakerRow(FName RowName);
	TSharedPtr<SWidget> BuildSpeakerListContextMenu();
	TSharedPtr<SWidget> BuildConversationListContextMenu();
	TSharedPtr<SWidget> BuildThresholdContextMenu();
	TSharedPtr<SWidget> BuildEmotionListContextMenu();
	void HandleCopySpeaker();
	void HandleCutSpeaker();
	void HandlePasteSpeaker();
	bool CanPasteSpeaker() const;
	void BeginInlineThresholdEdit(int32 ThresholdIndex);
	void CommitInlineThresholdEdit(int32 ThresholdIndex, const FText& NewText, ETextCommit::Type CommitType);
	TArray<float> GetActiveThresholdsForConversationMap() const;
	float GetMinimumRelationshipForBand(int32 BandIndex, const TArray<float>& Thresholds) const;
	FReply HandleCycleConversationBand(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset);
	FReply HandleSetConversationBand(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset, int32 TargetBand);
	void CommitConversationPriority(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset, const FText& NewText, ETextCommit::Type CommitType);
	FReply HandleToggleConversationRepeatable(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset);
	FReply HandleToggleConversationImportant(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset);
	void CommitConversationDisplayName(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset, const FText& NewText, ETextCommit::Type CommitType);
	FReply HandleAddConversationLockedByTag(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset);
	FReply HandleRemoveConversationLockedByTag(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset, int32 TagIndex);
	void HandleConversationLockedByTagChanged(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset, int32 TagIndex, FGameplayTag NewTag);
	void CommitConversationRequiredTags(TWeakObjectPtr<UARDialogueConversationAsset> ConversationAsset, const FGameplayTagContainer& NewTags);
	FReply HandleThresholdListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	FReply HandleSpeakerListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	void OnConversationSelectionChanged(TSharedPtr<FConversationEntry> Item, ESelectInfo::Type SelectInfo);
	const FSlateBrush* GetDefaultPortraitFieldBrush() const;

	FReply HandleRefresh();
	FReply HandleNewSpeaker();
	FReply HandleDuplicateSpeaker();
	FReply HandleDeleteSpeaker();
	FReply HandleValidateSpeaker();
	FReply HandleSaveSpeaker();
	FReply HandleCreateConversation();
	FReply HandleOpenConversation();
	FReply HandleRenameConversation();
	FReply HandleDuplicateConversation();
	FReply HandleDeleteConversation();
	FReply HandleDeleteConversationAndAsset();
	FReply HandleCleanupGeneratedConversationTags();
	FReply HandleFindBrokenConversations();
	FReply HandleSortByName();
	FReply HandleSortByConversationCount();
	FReply HandleResetThresholds();
	FReply HandleAddThreshold();
	FReply HandleRemoveThreshold();
	FReply HandleMoveThresholdUp();
	FReply HandleMoveThresholdDown();
	FReply HandleAddEmotionSlot();
	FReply HandleAddPortrait();
	FReply HandleRemovePortrait();
	FReply HandleDeleteConversationInternal(bool bDeleteAssetFromContentBrowser);

	TSharedRef<ITableRow> OnGenerateSpeakerRow(TSharedPtr<FSpeakerEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateConversationRow(TSharedPtr<FConversationEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGeneratePortraitRow(TSharedPtr<FPortraitEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSpeakerSelectionChanged(TSharedPtr<FSpeakerEntry> Item, ESelectInfo::Type SelectInfo);
	void OnConversationDoubleClicked(TSharedPtr<FConversationEntry> Item);
	void OnPortraitSelectionChanged(TSharedPtr<FPortraitEntry> Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> OnGenerateThresholdRow(TSharedPtr<FThresholdEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnThresholdSelectionChanged(TSharedPtr<FThresholdEntry> Item, ESelectInfo::Type SelectInfo);
	void OnThresholdDoubleClicked(TSharedPtr<FThresholdEntry> Item);
	void RefreshThresholdList();
	void SetEditedThresholdValue(float NewValue);

	TSharedPtr<SListView<TSharedPtr<FSpeakerEntry>>> SpeakerListView;
	TSharedPtr<SListView<TSharedPtr<FConversationEntry>>> ConversationListView;
	TSharedPtr<SListView<TSharedPtr<FPortraitEntry>>> PortraitListView;
	TSharedPtr<SListView<TSharedPtr<FThresholdEntry>>> ThresholdListView;
	TSharedPtr<SSearchBox> SearchTextBox;
	TSharedPtr<SEditableTextBox> DisplayNameTextBox;
	TSharedPtr<SEditableTextBox> DescriptionTextBox;
	TSharedPtr<SObjectPropertyEntryBox> LineFontPicker;
	TSharedPtr<SObjectPropertyEntryBox> DefaultPortraitTexturePicker;
	TSharedPtr<SObjectPropertyEntryBox> PortraitTexturePicker;
	TSharedPtr<SBox> EmotionTagComboHost;
	TSharedPtr<SExpandableArea> EmotionsExpandableArea;
	TSharedPtr<SExpandableArea> ThresholdsExpandableArea;

	TArray<TSharedPtr<FSpeakerEntry>> AllSpeakerEntries;
	TArray<TSharedPtr<FSpeakerEntry>> FilteredSpeakerEntries;
	TArray<TSharedPtr<FConversationEntry>> ConversationEntries;
	TArray<TSharedPtr<FPortraitEntry>> PortraitEntries;
	TWeakObjectPtr<UDataTable> SpeakerDataTable;
	FGameplayTag EditedSpeakerTag;
	FGameplayTag EditedFactionTag;
	TSoftObjectPtr<UFont> EditedLineFontAsset;
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
	bool bIsTearingDown = false;
	bool bHasSpeakerClipboard = false;
	bool bEmotionsExpanded = true;
	bool bThresholdsExpanded = true;
	int32 EditingThresholdIndex = INDEX_NONE;
	FName SpeakerClipboardSourceRowName = NAME_None;
	FARDialogueSpeakerRow SpeakerClipboardRow;
	FString ValidationOutput;
	mutable FSlateBrush DefaultPortraitFieldBrush;
	TWeakObjectPtr<UARDialogueConversationAsset> RenamingConversationAsset;
};
