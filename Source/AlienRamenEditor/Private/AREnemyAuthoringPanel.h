#pragma once

#include "CoreMinimal.h"
#include "ARInvaderTypes.h"
#include "Input/Reply.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"

class IDetailsView;
class SBorder;
class SEditableTextBox;
class SExpandableArea;
class STableViewBase;
class STextBlock;
class ITableRow;
class SWidget;
template<typename ItemType> class SListView;
class UDataTable;
class UARInvaderEnemyRowProxy;
class FSpawnTabArgs;
struct FPropertyChangedEvent;
class UObject;
class FTransactionObjectEvent;

namespace ARInvaderEnemyAuthoringEditor
{
	extern const FName TabName;
	TSharedRef<class SDockTab> SpawnTab(const FSpawnTabArgs& Args);
	void OpenEnemyAuthoringForIdentifierTag(const FGameplayTag& EnemyIdentifierTag);
	bool ResolveEnemyRowByIdentifierTag(const FGameplayTag& EnemyIdentifierTag, FName& OutRowName, FARInvaderEnemyDefRow& OutRow);
}

class SEnemyAuthoringPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEnemyAuthoringPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SEnemyAuthoringPanel();
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void FocusByIdentifierTag(FGameplayTag EnemyIdentifierTag);

private:
	struct FEnemyRowItem
	{
		FName RowName = NAME_None;
		bool bEnabled = true;
		FText DisplayName;
		FSoftClassPath EnemyClassPath;
		float MaxHealth = 0.f;
		FGameplayTag ArchetypeTag;
		FGameplayTag IdentifierTag;
	};

	struct FEnemyIssue
	{
		bool bError = true;
		FName RowName = NAME_None;
		FString Message;
	};

	void BuildLayout();
	void RefreshTable();
	void RefreshRows();
	void RefreshDetails();
	void RefreshIssues();
	void SetStatus(const FString& Message);
	void SelectRow(FName RowName);
	FName MakeUniqueRowName(const FString& BaseName) const;

	const FARInvaderEnemyDefRow* GetRow(FName RowName) const;
	FARInvaderEnemyDefRow* GetMutableRow(FName RowName);
	TArray<FName> GetSelectedRowNames() const;
	FName GetPrimarySelectedRowName() const;

	FReply OnReloadTable();
	FReply OnCreateRow();
	FReply OnDuplicateRow();
	FReply OnRenameRow();
	FReply OnDeleteRow();
	FReply OnToggleSelectedEnabled();
	FReply OnSaveTable();
	FReply OnValidateSelected();
	FReply OnValidateAll();

	void ValidateRows(const TArray<FName>& TargetRows, TArray<FEnemyIssue>& OutIssues) const;
	void ValidateSingleRow(const FName RowName, const FARInvaderEnemyDefRow& Row, TArray<FEnemyIssue>& OutIssues) const;

	void HandleRowSelectionChanged(TSharedPtr<FEnemyRowItem> Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FEnemyRowItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SWidget> OnOpenRowContextMenu();
	TSharedRef<SWidget> BuildRowContextMenu();
	FReply OnSetSelectedRowsEnabled(bool bEnable);
	void ToggleSortByColumn(FName ColumnId);
	FText GetSortLabel(FName ColumnId, FString BaseLabel) const;

	void HandlePropertiesChanged(const FPropertyChangedEvent& Event);
	void HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

private:
	TObjectPtr<UDataTable> EnemyTable = nullptr;
	FName SelectedRowName = NAME_None;

	TArray<TSharedPtr<FEnemyRowItem>> RowItems;
	TArray<TSharedPtr<FEnemyIssue>> IssueItems;

	TSharedPtr<SListView<TSharedPtr<FEnemyRowItem>>> RowListView;
	TSharedPtr<SListView<TSharedPtr<FEnemyIssue>>> IssueListView;
	TSharedPtr<SEditableTextBox> RenameTextBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SExpandableArea> IssuesArea;

	TStrongObjectPtr<UARInvaderEnemyRowProxy> RowProxy;
	bool bApplyingProxyToModel = false;
	FDelegateHandle ObjectTransactedHandle;

	FName SortColumn = TEXT("DisplayName");
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;
};
