#include "AREnemyAuthoringPanel.h"

#include "ARInvaderAuthoringEditorProxies.h"
#include "ARInvaderDirectorSettings.h"
#include "ARInvaderToolingSettings.h"
#include "ARLog.h"

#include "FileHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	TWeakPtr<SEnemyAuthoringPanel> GActiveEnemyPanel;
	TOptional<FGameplayTag> GPendingFocusTag;
	static const FString EnemyIdentifierDefaultRoot(TEXT("Enemy.Identifier"));

	bool HasExpectedEnemyRowStruct(const UDataTable* EnemyTable)
	{
		return EnemyTable && EnemyTable->GetRowStruct() == FARInvaderEnemyDefRow::StaticStruct();
	}

	UDataTable* LoadEnemyTableFromSettings()
	{
		if (const UARInvaderToolingSettings* ToolingSettings = GetDefault<UARInvaderToolingSettings>())
		{
			if (UDataTable* ToolingTable = ToolingSettings->EnemyDataTable.LoadSynchronous())
			{
				return ToolingTable;
			}
		}

		if (const UARInvaderDirectorSettings* RuntimeSettings = GetDefault<UARInvaderDirectorSettings>())
		{
			return RuntimeSettings->EnemyDataTable.LoadSynchronous();
		}

		return nullptr;
	}

	FSoftClassPath ToSoftClassPath(const TSoftClassPtr<AAREnemyBase>& EnemyClassRef)
	{
		const FString ClassPathString = EnemyClassRef.ToString();
		return ClassPathString.IsEmpty() ? FSoftClassPath() : FSoftClassPath(ClassPathString);
	}

	FString GetEnemyClassLabel(const FSoftClassPath& ClassPath)
	{
		if (!ClassPath.IsValid())
		{
			return TEXT("<None>");
		}

		const FString AssetName = ClassPath.GetAssetName();
		return AssetName.IsEmpty() ? ClassPath.ToString() : AssetName;
	}

	FString GetTagLeafString(const FGameplayTag& Tag)
	{
		const FString TagString = Tag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (TagString.FindLastChar(TEXT('.'), DotIndex))
		{
			return TagString.Mid(DotIndex + 1);
		}
		return TagString;
	}

	FString BuildTagPathFromRowName(const FARInvaderEnemyDefRow& Row, const FName RowName)
	{
		const FString RowNameString = RowName.ToString();
		const FString ExistingTagString = Row.EnemyIdentifierTag.ToString();

		FString ParentPath = EnemyIdentifierDefaultRoot;
		int32 DotIndex = INDEX_NONE;
		if (ExistingTagString.FindLastChar(TEXT('.'), DotIndex) && DotIndex > 0)
		{
			ParentPath = ExistingTagString.Left(DotIndex);
		}

		return FString::Printf(TEXT("%s.%s"), *ParentPath, *RowNameString);
	}
}

namespace ARInvaderEnemyAuthoringEditor
{
	const FName TabName(TEXT("AR_EnemyAuthoringTool"));

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		(void)Args;
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SEnemyAuthoringPanel)
			];
	}

	void OpenEnemyAuthoringForIdentifierTag(const FGameplayTag& EnemyIdentifierTag)
	{
		GPendingFocusTag = EnemyIdentifierTag;
		FGlobalTabmanager::Get()->TryInvokeTab(TabName);
		if (const TSharedPtr<SEnemyAuthoringPanel> PinnedPanel = GActiveEnemyPanel.Pin())
		{
			PinnedPanel->FocusByIdentifierTag(EnemyIdentifierTag);
		}
	}

	bool ResolveEnemyRowByIdentifierTag(const FGameplayTag& EnemyIdentifierTag, FName& OutRowName, FARInvaderEnemyDefRow& OutRow)
	{
		OutRowName = NAME_None;
		OutRow = FARInvaderEnemyDefRow();

		UDataTable* EnemyTable = LoadEnemyTableFromSettings();
		if (!EnemyTable || !EnemyIdentifierTag.IsValid() || !HasExpectedEnemyRowStruct(EnemyTable))
		{
			return false;
		}

		for (const FName RowName : EnemyTable->GetRowNames())
		{
			const FARInvaderEnemyDefRow* Row = EnemyTable->FindRow<FARInvaderEnemyDefRow>(RowName, TEXT("ResolveEnemyRowByIdentifierTag"), false);
			if (!Row)
			{
				continue;
			}

			if (Row->EnemyIdentifierTag.MatchesTagExact(EnemyIdentifierTag))
			{
				OutRowName = RowName;
				OutRow = *Row;
				return true;
			}
		}

		return false;
	}
}

void SEnemyAuthoringPanel::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	RowProxy.Reset(NewObject<UARInvaderEnemyRowProxy>(GetTransientPackage()));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(SharedThis(this), &SEnemyAuthoringPanel::HandlePropertiesChanged);

	BuildLayout();
	RefreshTable();
	if (EnemyTable && HasExpectedEnemyRowStruct(EnemyTable))
	{
		const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringNormalizeIdentifierTagsOnOpen", "Normalize Enemy Identifier Tags"));
		NormalizeIdentifierTagsForRows(EnemyTable->GetRowNames(), true, nullptr);
	}
	RefreshRows();
	RefreshDetails();
	RefreshIssues();

	ObjectTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(SharedThis(this), &SEnemyAuthoringPanel::HandleObjectTransacted);
	GActiveEnemyPanel = SharedThis(this);

	if (GPendingFocusTag.IsSet())
	{
		FocusByIdentifierTag(GPendingFocusTag.GetValue());
		GPendingFocusTag.Reset();
	}

	SetStatus(TEXT("Enemy authoring tool ready."));
}

SEnemyAuthoringPanel::~SEnemyAuthoringPanel()
{
	if (DetailsView.IsValid())
	{
		DetailsView->OnFinishedChangingProperties().RemoveAll(this);
	}
	if (ObjectTransactedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectTransacted.Remove(ObjectTransactedHandle);
		ObjectTransactedHandle.Reset();
	}
	if (GActiveEnemyPanel.Pin().Get() == this)
	{
		GActiveEnemyPanel.Reset();
	}
}

FReply SEnemyAuthoringPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;
	if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
	{
		return OnDeleteRow();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SEnemyAuthoringPanel::FocusByIdentifierTag(FGameplayTag EnemyIdentifierTag)
{
	if (!EnemyIdentifierTag.IsValid() || !EnemyTable || !HasExpectedEnemyRowStruct(EnemyTable))
	{
		return;
	}

	for (const FName RowName : EnemyTable->GetRowNames())
	{
		const FARInvaderEnemyDefRow* Row = EnemyTable->FindRow<FARInvaderEnemyDefRow>(RowName, TEXT("FocusByIdentifierTag"), false);
		if (!Row)
		{
			continue;
		}

		if (Row->EnemyIdentifierTag.MatchesTagExact(EnemyIdentifierTag))
		{
			SelectRow(RowName);
			SetStatus(FString::Printf(TEXT("Focused enemy row '%s' for tag '%s'."), *RowName.ToString(), *EnemyIdentifierTag.ToString()));
			return;
		}
	}

	SetStatus(FString::Printf(TEXT("Enemy tag '%s' was not found in enemy table."), *EnemyIdentifierTag.ToString()));
}

void SEnemyAuthoringPanel::BuildLayout()
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Reload Table"))
				.ToolTipText(FText::FromString("Reload enemy DataTable from disk."))
				.OnClicked(this, &SEnemyAuthoringPanel::OnReloadTable)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Validate Selected"))
				.ToolTipText(FText::FromString("Validate currently selected enemy row(s)."))
				.OnClicked(this, &SEnemyAuthoringPanel::OnValidateSelected)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Validate All"))
				.ToolTipText(FText::FromString("Validate all rows in the enemy DataTable."))
				.OnClicked(this, &SEnemyAuthoringPanel::OnValidateAll)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.48f)
			[
				SNew(SBorder)
				.Padding(6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton)
							.Text(FText::FromString("New"))
							.ToolTipText(FText::FromString("Create a new enemy row."))
							.OnClicked(this, &SEnemyAuthoringPanel::OnCreateRow)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Duplicate"))
							.ToolTipText(FText::FromString("Duplicate selected enemy row(s)."))
							.OnClicked(this, &SEnemyAuthoringPanel::OnDuplicateRow)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SAssignNew(RenameTextBox, SEditableTextBox)
						.HintText(FText::FromString("Enemy row name"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Rename"))
							.ToolTipText(FText::FromString("Rename selected row using the text box value."))
							.OnClicked(this, &SEnemyAuthoringPanel::OnRenameRow)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Delete"))
							.ToolTipText(FText::FromString("Delete selected enemy row(s)."))
							.OnClicked(this, &SEnemyAuthoringPanel::OnDeleteRow)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Enable/Disable"))
							.ToolTipText(FText::FromString("Enable disabled rows, otherwise disable selected rows."))
							.OnClicked(this, &SEnemyAuthoringPanel::OnToggleSelectedEnabled)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Save Table"))
							.ToolTipText(FText::FromString("Open save prompt for the enemy DataTable package."))
							.OnClicked(this, &SEnemyAuthoringPanel::OnSaveTable)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(0.15f)
						[
							SNew(SButton)
							.Text(this, &SEnemyAuthoringPanel::GetSortLabel, FName(TEXT("Enabled")), FString(TEXT("Enabled")))
							.OnClicked_Lambda([this]() { ToggleSortByColumn(TEXT("Enabled")); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().FillWidth(0.30f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(this, &SEnemyAuthoringPanel::GetSortLabel, FName(TEXT("DisplayName")), FString(TEXT("DisplayName")))
							.OnClicked_Lambda([this]() { ToggleSortByColumn(TEXT("DisplayName")); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(this, &SEnemyAuthoringPanel::GetSortLabel, FName(TEXT("EnemyClass")), FString(TEXT("EnemyClass")))
							.OnClicked_Lambda([this]() { ToggleSortByColumn(TEXT("EnemyClass")); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(this, &SEnemyAuthoringPanel::GetSortLabel, FName(TEXT("MaxHealth")), FString(TEXT("MaxHealth")))
							.OnClicked_Lambda([this]() { ToggleSortByColumn(TEXT("MaxHealth")); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(this, &SEnemyAuthoringPanel::GetSortLabel, FName(TEXT("ArchetypeTag")), FString(TEXT("ArchetypeTag")))
							.OnClicked_Lambda([this]() { ToggleSortByColumn(TEXT("ArchetypeTag")); return FReply::Handled(); })
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(RowListView, SListView<TSharedPtr<FEnemyRowItem>>)
						.ListItemsSource(&RowItems)
						.SelectionMode(ESelectionMode::Multi)
						.OnGenerateRow(this, &SEnemyAuthoringPanel::HandleGenerateRow)
						.OnContextMenuOpening(this, &SEnemyAuthoringPanel::OnOpenRowContextMenu)
						.OnSelectionChanged(this, &SEnemyAuthoringPanel::HandleRowSelectionChanged)
					]
				]
			]
			+ SSplitter::Slot().Value(0.52f)
			[
				SNew(SBorder)
				.Padding(6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(FText::FromString("Enemy Row Details"))
					]
					+ SVerticalBox::Slot().FillHeight(0.62f).Padding(0.f, 0.f, 0.f, 6.f)
					[
						DetailsView.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(IssuesArea, SExpandableArea)
						.HeaderContent()
						[
							SNew(STextBlock).Text(FText::FromString("Validation Issues"))
						]
						.BodyContent()
						[
							SNew(SBox)
							.MinDesiredHeight(120.f)
							.MaxDesiredHeight(220.f)
							[
								SAssignNew(IssueListView, SListView<TSharedPtr<FEnemyIssue>>)
								.ListItemsSource(&IssueItems)
								.OnGenerateRow_Lambda([](TSharedPtr<FEnemyIssue> Item, const TSharedRef<STableViewBase>& OwnerTable)
								{
									const FString Label = Item.IsValid()
										? FString::Printf(TEXT("[%s] %s | %s"), Item->bError ? TEXT("Error") : TEXT("Warn"), *Item->RowName.ToString(), *Item->Message)
										: TEXT("<invalid>");
									return SNew(STableRow<TSharedPtr<FEnemyIssue>>, OwnerTable)
									[
										SNew(STextBlock)
										.Text(FText::FromString(Label))
										.ColorAndOpacity(Item.IsValid() && Item->bError ? FLinearColor::Red : FLinearColor(1.f, 0.85f, 0.2f, 1.f))
									];
								})
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SAssignNew(StatusText, STextBlock).Text(FText::FromString("Ready"))
					]
				]
			]
		]
	];
}

void SEnemyAuthoringPanel::RefreshTable()
{
	EnemyTable = LoadEnemyTableFromSettings();
	if (!EnemyTable)
	{
		SetStatus(TEXT("Failed to load enemy table from tooling/runtime settings."));
		return;
	}

	if (!HasExpectedEnemyRowStruct(EnemyTable))
	{
		const UScriptStruct* FoundStruct = EnemyTable->GetRowStruct();
		SetStatus(FString::Printf(
			TEXT("Enemy table row struct mismatch. Expected '%s', found '%s'."),
			*GetNameSafe(FARInvaderEnemyDefRow::StaticStruct()),
			*GetNameSafe(FoundStruct)));
		EnemyTable = nullptr;
	}
}

void SEnemyAuthoringPanel::RefreshRows()
{
	RowItems.Reset();

	if (!EnemyTable)
	{
		if (RowListView.IsValid())
		{
			RowListView->RequestListRefresh();
		}
		return;
	}

	for (const FName RowName : EnemyTable->GetRowNames())
	{
		const FARInvaderEnemyDefRow* Row = EnemyTable->FindRow<FARInvaderEnemyDefRow>(RowName, TEXT("RefreshRows"), false);
		if (!Row)
		{
			continue;
		}

		TSharedPtr<FEnemyRowItem> Item = MakeShared<FEnemyRowItem>();
		Item->RowName = RowName;
		Item->bEnabled = Row->bEnabled;
		Item->DisplayName = Row->DisplayName.IsEmpty()
			? FText::FromString(Row->EnemyIdentifierTag.IsValid() ? Row->EnemyIdentifierTag.ToString() : RowName.ToString())
			: Row->DisplayName;
		Item->EnemyClassPath = ToSoftClassPath(Row->EnemyClass);
		Item->MaxHealth = Row->RuntimeInit.MaxHealth;
		Item->ArchetypeTag = Row->RuntimeInit.EnemyArchetypeTag;
		Item->IdentifierTag = Row->EnemyIdentifierTag;
		RowItems.Add(Item);
	}

	RowItems.Sort([this](const TSharedPtr<FEnemyRowItem>& A, const TSharedPtr<FEnemyRowItem>& B)
	{
		auto CompareStrings = [this](const FString& Lhs, const FString& Rhs)
		{
			const int32 CompareValue = Lhs.Compare(Rhs, ESearchCase::IgnoreCase);
			return SortMode == EColumnSortMode::Ascending ? CompareValue < 0 : CompareValue > 0;
		};

		auto CompareFloats = [this](float Lhs, float Rhs)
		{
			if (FMath::IsNearlyEqual(Lhs, Rhs))
			{
				return false;
			}
			return SortMode == EColumnSortMode::Ascending ? Lhs < Rhs : Lhs > Rhs;
		};

		if (!A.IsValid() || !B.IsValid())
		{
			return A.IsValid();
		}

		if (SortColumn == TEXT("Enabled") && A->bEnabled != B->bEnabled)
		{
			return SortMode == EColumnSortMode::Ascending ? (A->bEnabled < B->bEnabled) : (A->bEnabled > B->bEnabled);
		}
		if (SortColumn == TEXT("DisplayName") && A->DisplayName.ToString() != B->DisplayName.ToString())
		{
			return CompareStrings(A->DisplayName.ToString(), B->DisplayName.ToString());
		}
		if (SortColumn == TEXT("EnemyClass") && A->EnemyClassPath != B->EnemyClassPath)
		{
			return CompareStrings(GetEnemyClassLabel(A->EnemyClassPath), GetEnemyClassLabel(B->EnemyClassPath));
		}
		if (SortColumn == TEXT("MaxHealth") && !FMath::IsNearlyEqual(A->MaxHealth, B->MaxHealth))
		{
			return CompareFloats(A->MaxHealth, B->MaxHealth);
		}
		if (SortColumn == TEXT("ArchetypeTag") && A->ArchetypeTag != B->ArchetypeTag)
		{
			return CompareStrings(A->ArchetypeTag.ToString(), B->ArchetypeTag.ToString());
		}

		return A->RowName.LexicalLess(B->RowName);
	});

	if (RowListView.IsValid())
	{
		RowListView->RequestListRefresh();
		for (const TSharedPtr<FEnemyRowItem>& Item : RowItems)
		{
			if (Item.IsValid() && Item->RowName == SelectedRowName)
			{
				RowListView->SetSelection(Item, ESelectInfo::Direct);
				break;
			}
		}
	}
}

void SEnemyAuthoringPanel::RefreshDetails()
{
	bApplyingProxyToModel = true;
	if (const FARInvaderEnemyDefRow* SelectedRow = GetRow(SelectedRowName))
	{
		if (RowProxy.IsValid() && DetailsView.IsValid())
		{
			RowProxy->Row = *SelectedRow;
			DetailsView->SetObject(RowProxy.Get());
		}
	}
	else if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
	}
	bApplyingProxyToModel = false;
}

void SEnemyAuthoringPanel::RefreshIssues()
{
	if (IssueListView.IsValid())
	{
		IssueListView->RequestListRefresh();
	}
	if (IssuesArea.IsValid() && IssueItems.IsEmpty())
	{
		IssuesArea->SetExpanded(false);
	}
}

void SEnemyAuthoringPanel::SetStatus(const FString& Message)
{
	UE_LOG(ARLog, Log, TEXT("[EnemyAuthoring] %s"), *Message);
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

void SEnemyAuthoringPanel::SelectRow(FName RowName)
{
	SelectedRowName = RowName;
	if (RenameTextBox.IsValid())
	{
		RenameTextBox->SetText(RowName.IsNone() ? FText::GetEmpty() : FText::FromName(RowName));
	}
	if (RowListView.IsValid())
	{
		for (const TSharedPtr<FEnemyRowItem>& Item : RowItems)
		{
			if (Item.IsValid() && Item->RowName == RowName)
			{
				RowListView->SetSelection(Item, ESelectInfo::Direct);
				break;
			}
		}
	}
	RefreshDetails();
}

FName SEnemyAuthoringPanel::MakeUniqueRowName(const FString& BaseName) const
{
	if (!EnemyTable)
	{
		return FName(*BaseName);
	}

	FString Candidate = BaseName.IsEmpty() ? TEXT("Enemy") : BaseName;
	int32 Suffix = 1;
	while (EnemyTable->GetRowMap().Contains(FName(*Candidate)))
	{
		Candidate = FString::Printf(TEXT("%s_%d"), *(BaseName.IsEmpty() ? FString(TEXT("Enemy")) : BaseName), Suffix++);
	}
	return FName(*Candidate);
}

const FARInvaderEnemyDefRow* SEnemyAuthoringPanel::GetRow(FName RowName) const
{
	if (!EnemyTable || RowName.IsNone())
	{
		return nullptr;
	}
	return EnemyTable->FindRow<FARInvaderEnemyDefRow>(RowName, TEXT("EnemyAuthoring"), false);
}

FARInvaderEnemyDefRow* SEnemyAuthoringPanel::GetMutableRow(FName RowName)
{
	if (!EnemyTable || RowName.IsNone())
	{
		return nullptr;
	}
	return EnemyTable->FindRow<FARInvaderEnemyDefRow>(RowName, TEXT("EnemyAuthoring"), false);
}

TArray<FName> SEnemyAuthoringPanel::GetSelectedRowNames() const
{
	TArray<FName> Result;
	if (!RowListView.IsValid())
	{
		return Result;
	}

	const TArray<TSharedPtr<FEnemyRowItem>> SelectedItems = RowListView->GetSelectedItems();
	for (const TSharedPtr<FEnemyRowItem>& Item : SelectedItems)
	{
		if (Item.IsValid() && !Item->RowName.IsNone())
		{
			Result.AddUnique(Item->RowName);
		}
	}
	return Result;
}

FName SEnemyAuthoringPanel::GetPrimarySelectedRowName() const
{
	const TArray<FName> SelectedRows = GetSelectedRowNames();
	return SelectedRows.IsEmpty() ? NAME_None : SelectedRows[0];
}

FReply SEnemyAuthoringPanel::OnReloadTable()
{
	RefreshTable();
	if (EnemyTable && HasExpectedEnemyRowStruct(EnemyTable))
	{
		const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringNormalizeIdentifierTags", "Normalize Enemy Identifier Tags"));
		NormalizeIdentifierTagsForRows(EnemyTable->GetRowNames(), true, nullptr);
	}
	RefreshRows();
	RefreshDetails();
	SetStatus(TEXT("Enemy table reloaded."));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnCreateRow()
{
	if (!EnemyTable)
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	const FString BaseName = RenameTextBox.IsValid() ? RenameTextBox->GetText().ToString() : FString(TEXT("Enemy"));
	const FName NewRowName = MakeUniqueRowName(BaseName);

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringCreateRow", "Create Enemy Row"));
	EnemyTable->Modify();

	FARInvaderEnemyDefRow NewRow;
	EnemyTable->AddRow(NewRowName, NewRow);
	NormalizeIdentifierTagsForRows({ NewRowName }, false, nullptr);
	EnemyTable->MarkPackageDirty();

	RefreshRows();
	SelectRow(NewRowName);
	SetStatus(FString::Printf(TEXT("Created enemy row '%s'."), *NewRowName.ToString()));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnDuplicateRow()
{
	if (!EnemyTable)
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	const TArray<FName> SourceRows = GetSelectedRowNames();
	if (SourceRows.IsEmpty())
	{
		SetStatus(TEXT("Select one or more rows to duplicate."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringDuplicateRow", "Duplicate Enemy Row"));
	EnemyTable->Modify();

	FName LastNewRow = NAME_None;
	TArray<FName> NewRows;
	for (const FName& SourceRowName : SourceRows)
	{
		const FARInvaderEnemyDefRow* SourceRow = GetRow(SourceRowName);
		if (!SourceRow)
		{
			continue;
		}

		const FName NewRowName = MakeUniqueRowName(SourceRowName.ToString() + TEXT("_Copy"));
		EnemyTable->AddRow(NewRowName, *SourceRow);
		LastNewRow = NewRowName;
		NewRows.Add(NewRowName);
	}

	NormalizeIdentifierTagsForRows(NewRows, false, nullptr);
	EnemyTable->MarkPackageDirty();
	RefreshRows();
	if (!LastNewRow.IsNone())
	{
		SelectRow(LastNewRow);
	}
	SetStatus(FString::Printf(TEXT("Duplicated %d enemy row(s)."), SourceRows.Num()));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnRenameRow()
{
	if (!EnemyTable)
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	const FName OldRowName = GetPrimarySelectedRowName();
	if (OldRowName.IsNone())
	{
		SetStatus(TEXT("Select one row to rename."));
		return FReply::Handled();
	}

	const FString NewNameString = RenameTextBox.IsValid() ? RenameTextBox->GetText().ToString() : FString();
	if (NewNameString.IsEmpty())
	{
		SetStatus(TEXT("Enter a new row name first."));
		return FReply::Handled();
	}

	const FName NewRowName(*NewNameString);
	if (NewRowName == OldRowName)
	{
		SetStatus(TEXT("Row name is unchanged."));
		return FReply::Handled();
	}
	if (EnemyTable->GetRowMap().Contains(NewRowName))
	{
		SetStatus(TEXT("A row with that name already exists."));
		return FReply::Handled();
	}

	const FARInvaderEnemyDefRow* OldRow = GetRow(OldRowName);
	if (!OldRow)
	{
		SetStatus(TEXT("Selected row no longer exists."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringRenameRow", "Rename Enemy Row"));
	EnemyTable->Modify();

	const FARInvaderEnemyDefRow CachedRow = *OldRow;
	EnemyTable->RemoveRow(OldRowName);
	EnemyTable->AddRow(NewRowName, CachedRow);
	NormalizeIdentifierTagsForRows({ NewRowName }, false, nullptr);
	EnemyTable->MarkPackageDirty();

	RefreshRows();
	SelectRow(NewRowName);
	SetStatus(FString::Printf(TEXT("Renamed '%s' -> '%s'."), *OldRowName.ToString(), *NewRowName.ToString()));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnDeleteRow()
{
	if (!EnemyTable)
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	const TArray<FName> Targets = GetSelectedRowNames();
	if (Targets.IsEmpty())
	{
		SetStatus(TEXT("Select one or more rows to delete."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringDeleteRow", "Delete Enemy Row"));
	EnemyTable->Modify();
	for (const FName& RowName : Targets)
	{
		EnemyTable->RemoveRow(RowName);
	}
	EnemyTable->MarkPackageDirty();

	SelectedRowName = NAME_None;
	RefreshRows();
	RefreshDetails();
	SetStatus(FString::Printf(TEXT("Deleted %d enemy row(s)."), Targets.Num()));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnToggleSelectedEnabled()
{
	if (!EnemyTable)
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	const TArray<FName> Targets = GetSelectedRowNames();
	if (Targets.IsEmpty())
	{
		SetStatus(TEXT("Select one or more rows first."));
		return FReply::Handled();
	}

	bool bEnable = false;
	for (const FName& RowName : Targets)
	{
		if (const FARInvaderEnemyDefRow* Row = GetRow(RowName))
		{
			if (!Row->bEnabled)
			{
				bEnable = true;
				break;
			}
		}
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringToggleEnabled", "Toggle Enemy Enabled"));
	EnemyTable->Modify();

	int32 ChangedCount = 0;
	for (const FName& RowName : Targets)
	{
		FARInvaderEnemyDefRow* Row = GetMutableRow(RowName);
		if (Row && Row->bEnabled != bEnable)
		{
			Row->bEnabled = bEnable;
			++ChangedCount;
		}
	}
	EnemyTable->MarkPackageDirty();

	RefreshRows();
	RefreshDetails();
	SetStatus(FString::Printf(TEXT("%s %d enemy row(s)."), bEnable ? TEXT("Enabled") : TEXT("Disabled"), ChangedCount));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnSaveTable()
{
	if (!EnemyTable || !EnemyTable->GetOutermost())
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	TArray<UPackage*> Packages;
	Packages.Add(EnemyTable->GetOutermost());
	FEditorFileUtils::PromptForCheckoutAndSave(Packages, false, false);
	SetStatus(TEXT("Save prompt opened for enemy table."));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnValidateSelected()
{
	TArray<FName> SelectedRows = GetSelectedRowNames();
	TArray<FEnemyIssue> Issues;
	ValidateRows(SelectedRows, Issues);

	IssueItems.Reset();
	for (const FEnemyIssue& Issue : Issues)
	{
		IssueItems.Add(MakeShared<FEnemyIssue>(Issue));
	}
	RefreshIssues();
	SetStatus(FString::Printf(TEXT("Validation (selected) produced %d issue(s)."), IssueItems.Num()));
	return FReply::Handled();
}

FReply SEnemyAuthoringPanel::OnValidateAll()
{
	TArray<FEnemyIssue> Issues;
	ValidateRows(TArray<FName>(), Issues);

	IssueItems.Reset();
	for (const FEnemyIssue& Issue : Issues)
	{
		IssueItems.Add(MakeShared<FEnemyIssue>(Issue));
	}
	RefreshIssues();
	SetStatus(FString::Printf(TEXT("Validation (all) produced %d issue(s)."), IssueItems.Num()));
	return FReply::Handled();
}

bool SEnemyAuthoringPanel::EnsureIdentifierTagMatchesRowName(FName RowName, FARInvaderEnemyDefRow& Row, FString* OutFailureReason) const
{
	const FString DesiredTagPath = BuildTagPathFromRowName(Row, RowName);
	const FGameplayTag DesiredTag = FGameplayTag::RequestGameplayTag(FName(*DesiredTagPath), false);
	if (!DesiredTag.IsValid())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("Gameplay tag '%s' is not registered."), *DesiredTagPath);
		}
		return false;
	}

	Row.EnemyIdentifierTag = DesiredTag;
	return true;
}

void SEnemyAuthoringPanel::NormalizeIdentifierTagsForRows(const TArray<FName>& TargetRows, bool bUseTransaction, bool* bOutAnyChanged)
{
	if (bOutAnyChanged)
	{
		*bOutAnyChanged = false;
	}

	if (!EnemyTable || !HasExpectedEnemyRowStruct(EnemyTable) || TargetRows.IsEmpty())
	{
		return;
	}

	TArray<FName> ChangedRows;
	TArray<FString> Failures;

	if (bUseTransaction)
	{
		EnemyTable->Modify();
	}

	for (const FName RowName : TargetRows)
	{
		FARInvaderEnemyDefRow* MutableRow = GetMutableRow(RowName);
		if (!MutableRow)
		{
			continue;
		}

		const FGameplayTag PreviousTag = MutableRow->EnemyIdentifierTag;
		FString FailureReason;
		if (!EnsureIdentifierTagMatchesRowName(RowName, *MutableRow, &FailureReason))
		{
			Failures.Add(FString::Printf(TEXT("%s: %s"), *RowName.ToString(), *FailureReason));
			continue;
		}

		if (!MutableRow->EnemyIdentifierTag.MatchesTagExact(PreviousTag))
		{
			ChangedRows.Add(RowName);
		}
	}

	if (!ChangedRows.IsEmpty())
	{
		EnemyTable->MarkPackageDirty();
		if (bOutAnyChanged)
		{
			*bOutAnyChanged = true;
		}
	}

	if (!Failures.IsEmpty())
	{
		SetStatus(FString::Printf(TEXT("Identifier tag sync incomplete: %s"), *FString::Join(Failures, TEXT(" | "))));
	}
}

void SEnemyAuthoringPanel::ValidateRows(const TArray<FName>& TargetRows, TArray<FEnemyIssue>& OutIssues) const
{
	if (!EnemyTable || !HasExpectedEnemyRowStruct(EnemyTable))
	{
		OutIssues.Add({ true, NAME_None, TEXT("Enemy table is not loaded.") });
		return;
	}

	TSet<FName> TargetSet;
	if (!TargetRows.IsEmpty())
	{
		for (const FName& RowName : TargetRows)
		{
			TargetSet.Add(RowName);
		}
	}

	TMap<FGameplayTag, FName> FirstSeenByTag;
	for (const FName RowName : EnemyTable->GetRowNames())
	{
		const FARInvaderEnemyDefRow* Row = EnemyTable->FindRow<FARInvaderEnemyDefRow>(RowName, TEXT("ValidateRows"), false);
		if (!Row)
		{
			continue;
		}

		const bool bShouldValidate = TargetSet.IsEmpty() || TargetSet.Contains(RowName);
		if (bShouldValidate)
		{
			ValidateSingleRow(RowName, *Row, OutIssues);
		}

		if (Row->EnemyIdentifierTag.IsValid())
		{
			if (const FName* FirstRowName = FirstSeenByTag.Find(Row->EnemyIdentifierTag))
			{
				const bool bEmitForFirst = TargetSet.IsEmpty() || TargetSet.Contains(*FirstRowName);
				const bool bEmitForCurrent = TargetSet.IsEmpty() || TargetSet.Contains(RowName);
				if (bEmitForFirst)
				{
					OutIssues.Add({ true, *FirstRowName, FString::Printf(TEXT("Duplicate EnemyIdentifierTag '%s' also used by row '%s'."), *Row->EnemyIdentifierTag.ToString(), *RowName.ToString()) });
				}
				if (bEmitForCurrent)
				{
					OutIssues.Add({ true, RowName, FString::Printf(TEXT("Duplicate EnemyIdentifierTag '%s' also used by row '%s'."), *Row->EnemyIdentifierTag.ToString(), *FirstRowName->ToString()) });
				}
			}
			else
			{
				FirstSeenByTag.Add(Row->EnemyIdentifierTag, RowName);
			}
		}
	}
}

void SEnemyAuthoringPanel::ValidateSingleRow(const FName RowName, const FARInvaderEnemyDefRow& Row, TArray<FEnemyIssue>& OutIssues) const
{
	if (!Row.EnemyIdentifierTag.IsValid())
	{
		OutIssues.Add({ true, RowName, TEXT("EnemyIdentifierTag is required.") });
	}
	else
	{
		const FString Leaf = GetTagLeafString(Row.EnemyIdentifierTag);
		const FString RowNameString = RowName.ToString();
		if (Leaf != RowNameString)
		{
			OutIssues.Add({ true, RowName, FString::Printf(TEXT("EnemyIdentifierTag leaf '%s' must exactly match row name '%s'."), *Leaf, *RowNameString) });
		}
	}
	if (Row.EnemyClass.IsNull())
	{
		OutIssues.Add({ true, RowName, TEXT("EnemyClass is required.") });
	}
	if (Row.RuntimeInit.MaxHealth <= 0.f)
	{
		OutIssues.Add({ true, RowName, TEXT("MaxHealth must be > 0.") });
	}
	if (Row.RuntimeInit.Damage < 0.f)
	{
		OutIssues.Add({ true, RowName, TEXT("Damage must be >= 0.") });
	}
	if (Row.RuntimeInit.CollisionDamage < 0.f)
	{
		OutIssues.Add({ true, RowName, TEXT("CollisionDamage must be >= 0.") });
	}
	if (Row.RuntimeInit.MoveSpeed < 0.f)
	{
		OutIssues.Add({ true, RowName, TEXT("MoveSpeed must be >= 0.") });
	}
	if (Row.RuntimeInit.FireRate < 0.f)
	{
		OutIssues.Add({ true, RowName, TEXT("FireRate must be >= 0.") });
	}
	if (Row.RuntimeInit.DamageTakenMultiplier < 0.f)
	{
		OutIssues.Add({ true, RowName, TEXT("DamageTakenMultiplier must be >= 0.") });
	}
	for (int32 AbilityIndex = 0; AbilityIndex < Row.RuntimeInit.EnemySpecificAbilities.Num(); ++AbilityIndex)
	{
		const FARAbilitySet_AbilityEntry& Entry = Row.RuntimeInit.EnemySpecificAbilities[AbilityIndex];
		if (!Entry.Ability)
		{
			OutIssues.Add({ true, RowName, FString::Printf(TEXT("EnemySpecificAbilities[%d] has no Ability class."), AbilityIndex) });
		}
	}
}

void SEnemyAuthoringPanel::HandleRowSelectionChanged(TSharedPtr<FEnemyRowItem> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (!Item.IsValid())
	{
		return;
	}

	SelectedRowName = Item->RowName;
	if (RenameTextBox.IsValid())
	{
		RenameTextBox->SetText(FText::FromName(SelectedRowName));
	}
	RefreshDetails();
}

TSharedRef<ITableRow> SEnemyAuthoringPanel::HandleGenerateRow(TSharedPtr<FEnemyRowItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString EnabledLabel = Item.IsValid() ? (Item->bEnabled ? TEXT("Yes") : TEXT("No")) : TEXT("-");
	const FString DisplayName = Item.IsValid()
		? (Item->bEnabled ? Item->DisplayName.ToString() : FString::Printf(TEXT("%s [Disabled]"), *Item->DisplayName.ToString()))
		: TEXT("<invalid>");
	const FString EnemyClass = Item.IsValid() ? GetEnemyClassLabel(Item->EnemyClassPath) : TEXT("<invalid>");
	const FString MaxHealth = Item.IsValid() ? FString::Printf(TEXT("%.1f"), Item->MaxHealth) : TEXT("-");
	const FString Archetype = (Item.IsValid() && Item->ArchetypeTag.IsValid()) ? Item->ArchetypeTag.ToString() : TEXT("<None>");
	const FLinearColor RowTint = (Item.IsValid() && !Item->bEnabled)
		? FLinearColor(0.5f, 0.5f, 0.5f, 1.f)
		: FLinearColor::White;

	return SNew(STableRow<TSharedPtr<FEnemyRowItem>>, OwnerTable)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.OnMouseButtonDown_Lambda([this, Item](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Item.IsValid() && RowListView.IsValid())
			{
				RowListView->SetSelection(Item, ESelectInfo::Direct);
			}
			return FReply::Unhandled();
		})
		[
			SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.15f)
		[
			SNew(STextBlock).Text(FText::FromString(EnabledLabel)).ColorAndOpacity(RowTint)
		]
		+ SHorizontalBox::Slot().FillWidth(0.30f).Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(DisplayName)).ColorAndOpacity(RowTint)
		]
		+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(EnemyClass)).ColorAndOpacity(RowTint)
		]
		+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(MaxHealth)).ColorAndOpacity(RowTint)
		]
		+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(FText::FromString(Archetype)).ColorAndOpacity(RowTint)
		]
		]
	];
}

TSharedPtr<SWidget> SEnemyAuthoringPanel::OnOpenRowContextMenu()
{
	if (GetSelectedRowNames().IsEmpty())
	{
		return nullptr;
	}

	return BuildRowContextMenu();
}

TSharedRef<SWidget> SEnemyAuthoringPanel::BuildRowContextMenu()
{
	const TArray<FName> SelectedRows = GetSelectedRowNames();
	const bool bSingleSelection = (SelectedRows.Num() == 1);
	bool bAllEnabled = true;
	bool bAnyValidRow = false;
	for (const FName& RowName : SelectedRows)
	{
		if (const FARInvaderEnemyDefRow* Row = GetRow(RowName))
		{
			bAnyValidRow = true;
			bAllEnabled &= Row->bEnabled;
		}
	}
	const bool bDisableAction = bAnyValidRow && bAllEnabled;
	const FText ToggleText = bDisableAction ? FText::FromString("Disable") : FText::FromString("Enable");
	const FText ToggleDescText = bDisableAction
		? FText::FromString("Disable selected row(s) so they never spawn.")
		: FText::FromString("Enable selected row(s) so they can spawn.");

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Rename")),
		FText::FromString(TEXT("Rename selected row from the rename textbox value.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { OnRenameRow(); }),
			FCanExecuteAction::CreateLambda([bSingleSelection]() { return bSingleSelection; })));
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Duplicate")),
		FText::FromString(TEXT("Duplicate selected row(s).")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnDuplicateRow(); })));
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Delete")),
		FText::FromString(TEXT("Delete selected row(s).")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnDeleteRow(); })));
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(
		ToggleText,
		ToggleDescText,
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, bDisableAction]() { OnSetSelectedRowsEnabled(!bDisableAction); })));

	return MenuBuilder.MakeWidget();
}

FReply SEnemyAuthoringPanel::OnSetSelectedRowsEnabled(bool bEnable)
{
	if (!EnemyTable)
	{
		SetStatus(TEXT("Enemy table is not loaded."));
		return FReply::Handled();
	}

	const TArray<FName> Targets = GetSelectedRowNames();
	if (Targets.IsEmpty())
	{
		SetStatus(TEXT("Select one or more rows first."));
		return FReply::Handled();
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringSetEnabled", "Set Enemy Rows Enabled"));
	EnemyTable->Modify();

	int32 ChangedCount = 0;
	for (const FName& RowName : Targets)
	{
		FARInvaderEnemyDefRow* Row = GetMutableRow(RowName);
		if (Row && Row->bEnabled != bEnable)
		{
			Row->bEnabled = bEnable;
			++ChangedCount;
		}
	}
	EnemyTable->MarkPackageDirty();

	RefreshRows();
	RefreshDetails();
	SetStatus(FString::Printf(TEXT("%s %d enemy row(s)."), bEnable ? TEXT("Enabled") : TEXT("Disabled"), ChangedCount));
	return FReply::Handled();
}

void SEnemyAuthoringPanel::ToggleSortByColumn(FName ColumnId)
{
	if (SortColumn == ColumnId)
	{
		SortMode = (SortMode == EColumnSortMode::Ascending) ? EColumnSortMode::Descending : EColumnSortMode::Ascending;
	}
	else
	{
		SortColumn = ColumnId;
		SortMode = EColumnSortMode::Ascending;
	}

	RefreshRows();
}

FText SEnemyAuthoringPanel::GetSortLabel(FName ColumnId, FString BaseLabel) const
{
	if (SortColumn != ColumnId)
	{
		return FText::FromString(BaseLabel);
	}

	const TCHAR* Suffix = (SortMode == EColumnSortMode::Ascending) ? TEXT(" ↑") : TEXT(" ↓");
	return FText::FromString(BaseLabel + Suffix);
}

void SEnemyAuthoringPanel::HandlePropertiesChanged(const FPropertyChangedEvent& Event)
{
	(void)Event;
	if (bApplyingProxyToModel || !EnemyTable || !RowProxy.IsValid() || SelectedRowName.IsNone())
	{
		return;
	}

	FARInvaderEnemyDefRow* MutableRow = GetMutableRow(SelectedRowName);
	if (!MutableRow)
	{
		return;
	}

	const FScopedTransaction Tx(NSLOCTEXT("AlienRamenEditor", "EnemyAuthoringEditRow", "Edit Enemy Row"));
	EnemyTable->Modify();
	*MutableRow = RowProxy->Row;
	NormalizeIdentifierTagsForRows({ SelectedRowName }, false, nullptr);
	EnemyTable->MarkPackageDirty();

	RefreshRows();
	RefreshDetails();
}

void SEnemyAuthoringPanel::HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	(void)Event;
	if (!Object || Object != EnemyTable)
	{
		return;
	}

	RefreshRows();
	RefreshDetails();
	RefreshIssues();
}
