#include "AREnemyAuthoringPanel.h"
#include "ARInvaderAuthoringPanel.h"
#include "ARLog.h"
#include "ARSaveSubsystem.h"
#include "ARSaveGame.h"
#include "ARSaveIndexGame.h"
#include "ARSaveTypes.h"
#include "ARLoadoutSettings.h"

#include "IDetailsView.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"

namespace ARDebugSaveEditor
{
	static const FName TabName(TEXT("AR_DebugSaveTool"));
	static const FString DebugSlotSuffix = TEXT("_debug");

	class SPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPanel) {}
		SLATE_END_ARGS()

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::S)
			{
				return OnSaveCurrent();
			}

			return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
		}

		void Construct(const FArguments&)
		{
			FPropertyEditorModule& PropertyEditorModule =
				FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

			FDetailsViewArgs DetailsArgs;
			DetailsArgs.bAllowSearch = true;
			DetailsArgs.bHideSelectionTip = true;
			DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			DetailsArgs.bLockable = false;
			DetailsArgs.bUpdatesFromSelection = false;

			SaveDetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);

			ChildSlot
			[
				SNew(SSplitter)
				+ SSplitter::Slot()
				.Value(0.35f)
				[
					SNew(SBorder)
					.Padding(8.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SNew(STextBlock).Text(FText::FromString("Debug Slot Base Name"))
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
								[
									SNew(SCheckBox)
									.IsChecked(this, &SPanel::GetDebugNamespaceCheckState)
									.OnCheckStateChanged(this, &SPanel::OnSaveNamespaceToggled)
								]
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(this, &SPanel::GetSaveNamespaceLabel)
								]
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SAssignNew(SlotNameTextBox, SEditableTextBox)
								.HintText(FText::FromString("example: sammy_test"))
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SNew(SUniformWrapPanel)
								.SlotPadding(FVector2D(6.f, 6.f))
								.MinDesiredSlotWidth(96.f)
								+ SUniformWrapPanel::Slot()
								[
									SNew(SBox).MinDesiredWidth(96.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Create"))
										.OnClicked(this, &SPanel::OnCreateSlot)
									]
								]

								+ SUniformWrapPanel::Slot()
								[
									SNew(SBox).MinDesiredWidth(96.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Refresh"))
										.OnClicked(this, &SPanel::OnRefreshSlots)
									]
								]

								+ SUniformWrapPanel::Slot()
								[
									SNew(SBox).MinDesiredWidth(96.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Load"))
										.OnClicked(this, &SPanel::OnLoadSelected)
									]
								]

								+ SUniformWrapPanel::Slot()
								[
									SNew(SBox).MinDesiredWidth(96.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Delete"))
										.OnClicked(this, &SPanel::OnDeleteSelected)
									]
								]

								+ SUniformWrapPanel::Slot()
								[
									SNew(SBox).MinDesiredWidth(96.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Rename"))
										.OnClicked(this, &SPanel::OnRenameSelected)
									]
								]

								+ SUniformWrapPanel::Slot()
								[
									SNew(SBox).MinDesiredWidth(96.f)
									[
										SNew(SButton)
										.Text(FText::FromString("Duplicate"))
										.OnClicked(this, &SPanel::OnDuplicateSelected)
									]
								]
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SAssignNew(CurrentSlotText, STextBlock)
								.Text(FText::FromString("Current Slot: <none>"))
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SAssignNew(StatusText, STextBlock)
								.Text(FText::FromString("Ready"))
							]

							+ SVerticalBox::Slot().FillHeight(1.f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f)
								[
									SAssignNew(SlotListView, SListView<TSharedPtr<FARSaveSlotDescriptor>>)
									.ListItemsSource(&SlotItems)
									.SelectionMode(ESelectionMode::Multi)
									.OnGenerateRow(this, &SPanel::OnGenerateRow)
									.OnMouseButtonDoubleClick(this, &SPanel::OnSlotDoubleClicked)
									.OnContextMenuOpening(this, &SPanel::OnSlotListContextMenuOpening)
									.ExternalScrollbar(SlotListScrollbar)
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SAssignNew(SlotListScrollbar, SScrollBar)
									.AlwaysShowScrollbar(true)
								]
							]
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.65f)
				[
					SNew(SBorder)
					.Padding(8.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
							[
								SNew(STextBlock).Text(FText::FromString("Loaded Save"))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
							[
								SNew(SUniformWrapPanel)
								.SlotPadding(FVector2D(6.f, 6.f))
								.MinDesiredSlotWidth(120.f)
								+ SUniformWrapPanel::Slot()
								[
									SNew(SButton)
									.Text(FText::FromString("Save"))
									.OnClicked(this, &SPanel::OnSaveCurrent)
								]
								+ SUniformWrapPanel::Slot()
								[
									SNew(SButton)
									.Text(FText::FromString("Set Default Unlocks"))
									.OnClicked(this, &SPanel::OnSetDefaultUnlocksCurrent)
								]
								+ SUniformWrapPanel::Slot()
								[
									SNew(SButton)
									.Text(FText::FromString("Set Default Loadout (All Players)"))
									.OnClicked(this, &SPanel::OnDefaultLoadoutAllPlayersCurrent)
								]
								+ SUniformWrapPanel::Slot()
								[
									SNew(SButton)
									.Text(FText::FromString("Revert"))
									.OnClicked(this, &SPanel::OnRevertCurrent)
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.f)
							[
								SaveDetailsView.ToSharedRef()
							]
						]
					]
				]
			];

			RefreshSlots();
		}

	private:
		TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FARSaveSlotDescriptor> Item, const TSharedRef<STableViewBase>& OwnerTable) const
		{
			const FString Label = FString::Printf(TEXT("%s  |  # %d  |  Money %d"),
				*GetDisplaySlotName(Item->SlotName).ToString(), Item->SlotNumber, Item->Money);
			return SNew(STableRow<TSharedPtr<FARSaveSlotDescriptor>>, OwnerTable)
				[
					SNew(STextBlock).Text(FText::FromString(Label))
				];
		}

		void OnSlotDoubleClicked(TSharedPtr<FARSaveSlotDescriptor> Item)
		{
			if (!Item.IsValid())
			{
				return;
			}

			SlotListView->ClearSelection();
			SlotListView->SetSelection(Item);
			LoadSlotByName(Item->SlotName, /*bAutoSaveCurrentFirst=*/ true);
		}

		bool GetPrimarySelectionForSingleAction(TSharedPtr<FARSaveSlotDescriptor>& OutPrimarySelected)
		{
			OutPrimarySelected.Reset();
			const TArray<TSharedPtr<FARSaveSlotDescriptor>> Selected = SlotListView->GetSelectedItems();
			if (Selected.IsEmpty() || !Selected[0].IsValid())
			{
				return false;
			}

			OutPrimarySelected = Selected[0];
			if (Selected.Num() > 1)
			{
				SlotListView->ClearSelection();
				SlotListView->SetSelection(OutPrimarySelected);
				SlotListView->RequestListRefresh();
			}
			return true;
		}

		bool SaveCurrentLoadedForSwitch(const FName TargetSlotName)
		{
			if (!CurrentSaveObject || CurrentSlotName.IsNone() || CurrentSlotName == TargetSlotName)
			{
				return true;
			}

			FString Error;
			if (UARSaveSubsystem* Subsystem = TryGetSaveSubsystem())
			{
				FARSaveResult Result;
				if (!Subsystem->SaveCurrentGame(CurrentSlotName, true, Result, bUseDebugSaves))
				{
					SetStatus(FString::Printf(TEXT("Autosave-before-load failed: %s"), *Result.Error));
					return false;
				}
			}
			else if (!SaveCurrentOffline(bUseDebugSaves, Error))
			{
				SetStatus(FString::Printf(TEXT("Autosave-before-load failed: %s"), *Error));
				return false;
			}

			return true;
		}

		void CaptureLoadedSnapshot()
		{
			LoadedSnapshotBytes.Reset();
			LoadedSnapshotSlotName = NAME_None;
			if (!CurrentSaveObject || CurrentSlotName.IsNone())
			{
				return;
			}

			if (UGameplayStatics::SaveGameToMemory(CurrentSaveObject.Get(), LoadedSnapshotBytes) && LoadedSnapshotBytes.Num() > 0)
			{
				LoadedSnapshotSlotName = CurrentSlotName;
			}
		}

		FReply LoadSlotByName(const FName SlotName, const bool bAutoSaveCurrentFirst)
		{
			if (SlotName.IsNone())
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}

			if (bAutoSaveCurrentFirst && !SaveCurrentLoadedForSwitch(SlotName))
			{
				return FReply::Handled();
			}

			if (UARSaveSubsystem* Subsystem = TryGetSaveSubsystem())
			{
				FARSaveResult Result;
				if (!Subsystem->LoadGame(SlotName, -1, Result, bUseDebugSaves))
				{
					CurrentSlotName = NAME_None;
					CurrentSaveObject.Reset();
					LoadedSnapshotBytes.Reset();
					LoadedSnapshotSlotName = NAME_None;
					BindCurrentSaveToDetails();
					const bool bPruned = PruneSlotFromIndexOffline(SlotName, bUseDebugSaves);
					if (bPruned)
					{
						RefreshSlots();
					}
					else
					{
						PromptRemoveBrokenSlotEntry(SlotName, Result.Error);
					}
					SetStatus(FString::Printf(TEXT("Load failed: %s"), *Result.Error));
					return FReply::Handled();
				}

				CurrentSlotName = SlotName;
				CurrentSaveObject.Reset(Subsystem->GetCurrentSaveGame());
			}
			else
			{
				FString Error;
				if (!LoadSlotOffline(SlotName, bUseDebugSaves, Error))
				{
					CurrentSlotName = NAME_None;
					CurrentSaveObject.Reset();
					LoadedSnapshotBytes.Reset();
					LoadedSnapshotSlotName = NAME_None;
					BindCurrentSaveToDetails();
					const bool bPruned = PruneSlotFromIndexOffline(SlotName, bUseDebugSaves);
					if (bPruned)
					{
						RefreshSlots();
					}
					else
					{
						PromptRemoveBrokenSlotEntry(SlotName, Error);
					}
					SetStatus(FString::Printf(TEXT("Load failed: %s"), *Error));
					return FReply::Handled();
				}
			}

			CaptureLoadedSnapshot();
			BindCurrentSaveToDetails();
			SetStatus(FString::Printf(TEXT("Loaded slot '%s'."), *GetDisplaySlotName(CurrentSlotName).ToString()));
			return FReply::Handled();
		}

		TSharedPtr<SWidget> OnSlotListContextMenuOpening()
		{
			const TArray<TSharedPtr<FARSaveSlotDescriptor>> Selected = SlotListView->GetSelectedItems();
			if (Selected.IsEmpty() || !Selected[0].IsValid())
			{
				return SNullWidget::NullWidget;
			}

			FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
			MenuBuilder.AddMenuEntry(
				FText::FromString("Load"),
				FText::FromString("Load selected save slot."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPanel::HandleContextLoad)));
			MenuBuilder.AddMenuEntry(
				FText::FromString("Delete"),
				FText::FromString("Delete selected save slot."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPanel::HandleContextDelete)));
			MenuBuilder.AddMenuEntry(
				FText::FromString("Rename"),
				FText::FromString("Rename selected slot to the textbox value."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPanel::HandleContextRename)));
			MenuBuilder.AddMenuEntry(
				FText::FromString("Duplicate"),
				FText::FromString("Duplicate selected slot to the textbox value."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPanel::HandleContextDuplicate)));
			MenuBuilder.AddMenuEntry(
				FText::FromString("Heal Missing"),
				FText::FromString("If selected slot data is missing on disk, remove stale index entry."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPanel::HandleContextHealMissing)));
			return MenuBuilder.MakeWidget();
		}

		void HandleContextLoad() { OnLoadSelected(); }
		void HandleContextDelete() { OnDeleteSelected(); }
		void HandleContextRename() { OnRenameSelected(); }
		void HandleContextDuplicate() { OnDuplicateSelected(); }
		void HandleContextHealMissing()
		{
			TSharedPtr<FARSaveSlotDescriptor> SelectedPrimary;
			if (!GetPrimarySelectionForSingleAction(SelectedPrimary))
			{
				SetStatus(TEXT("No slot selected."));
				return;
			}

			if (HasAnyPhysicalRevisionForSlot(SelectedPrimary->SlotName, bUseDebugSaves))
			{
				SetStatus(TEXT("Selected slot has at least one revision on disk; nothing to heal."));
				return;
			}

			PromptRemoveBrokenSlotEntry(SelectedPrimary->SlotName, TEXT("No revision files found on disk."));
		}

		void RefreshSlots()
		{
			TArray<FARSaveSlotDescriptor> Slots;
			if (UARSaveSubsystem* Subsystem = TryGetSaveSubsystem())
			{
				FARSaveResult Result;
				if (!Subsystem->ListSaves(Slots, Result, bUseDebugSaves))
				{
					SetStatus(FString::Printf(TEXT("Refresh failed: %s"), *Result.Error));
					return;
				}
			}
			else
			{
				FString Error;
				if (!ListSlotsOffline(bUseDebugSaves, Slots, Error))
				{
					SetStatus(FString::Printf(TEXT("Refresh failed: %s"), *Error));
					return;
				}
			}

			SlotItems.Reset();
			for (const FARSaveSlotDescriptor& Entry : Slots)
			{
				SlotItems.Add(MakeShared<FARSaveSlotDescriptor>(Entry));
			}
			SlotListView->RequestListRefresh();
			SetStatus(FString::Printf(TEXT("Loaded %d %s slots."), SlotItems.Num(), bUseDebugSaves ? TEXT("debug") : TEXT("real")));
		}

		FReply OnRefreshSlots()
		{
			RefreshSlots();
			return FReply::Handled();
		}

		FReply OnCreateSlot()
		{
			FName BaseName(*SlotNameTextBox->GetText().ToString());
			if (UARSaveSubsystem* Subsystem = TryGetSaveSubsystem())
			{
				BaseName = NormalizeSlotBase(BaseName, Subsystem);

				FARSaveSlotDescriptor NewSlot;
				FARSaveResult Result;
				if (!Subsystem->CreateNewSave(BaseName, NewSlot, Result, bUseDebugSaves))
				{
					SetStatus(FString::Printf(TEXT("Create failed: %s"), *Result.Error));
					return FReply::Handled();
				}

				CurrentSlotName = NewSlot.SlotName;
				CurrentSaveObject.Reset(Subsystem->GetCurrentSaveGame());
			}
			else
			{
				BaseName = NormalizeSlotBase(BaseName, nullptr);

				FString Error;
				if (!CreateSlotOffline(BaseName, bUseDebugSaves, Error))
				{
					SetStatus(FString::Printf(TEXT("Create failed: %s"), *Error));
					return FReply::Handled();
				}
			}

			SlotNameTextBox->SetText(FText::GetEmpty());
			CaptureLoadedSnapshot();
			BindCurrentSaveToDetails();
			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Created slot base '%s'."), *GetDisplaySlotName(CurrentSlotName).ToString()));
			return FReply::Handled();
		}

		ECheckBoxState GetDebugNamespaceCheckState() const
		{
			return bUseDebugSaves ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		void OnSaveNamespaceToggled(ECheckBoxState NewState)
		{
			bUseDebugSaves = (NewState == ECheckBoxState::Checked);
			RefreshSlots();
		}

		FText GetSaveNamespaceLabel() const
		{
			return FText::FromString("Use Debug Namespace");
		}

		FReply OnLoadSelected()
		{
			TSharedPtr<FARSaveSlotDescriptor> SelectedPrimary;
			if (!GetPrimarySelectionForSingleAction(SelectedPrimary))
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}
			return LoadSlotByName(SelectedPrimary->SlotName, /*bAutoSaveCurrentFirst=*/ false);
		}

		FReply OnSaveCurrent()
		{
			if (!CurrentSaveObject)
			{
				SetStatus(TEXT("No save object loaded."));
				return FReply::Handled();
			}

			if (UARSaveSubsystem* Subsystem = TryGetSaveSubsystem())
			{
				FARSaveResult Result;
				if (!Subsystem->SaveCurrentGame(CurrentSlotName, true, Result, bUseDebugSaves))
				{
					SetStatus(FString::Printf(TEXT("Save failed: %s"), *Result.Error));
					return FReply::Handled();
				}
			}
			else
			{
				FString Error;
				if (!SaveCurrentOffline(bUseDebugSaves, Error))
				{
					SetStatus(FString::Printf(TEXT("Save failed: %s"), *Error));
					return FReply::Handled();
				}
			}

			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Saved slot '%s'."), *GetDisplaySlotName(CurrentSlotName).ToString()));
			return FReply::Handled();
		}

		FReply OnDeleteSelected()
		{
			const TArray<TSharedPtr<FARSaveSlotDescriptor>> Selected = SlotListView->GetSelectedItems();
			if (Selected.IsEmpty())
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}

			const EAppReturnType::Type Confirm = FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::FromString(TEXT("Delete?")));
			if (Confirm != EAppReturnType::Yes)
			{
				return FReply::Handled();
			}

			int32 DeletedCount = 0;
			for (const TSharedPtr<FARSaveSlotDescriptor>& Item : Selected)
			{
				if (!Item.IsValid())
				{
					continue;
				}

				const FName SlotToDelete = Item->SlotName;
				if (UARSaveSubsystem* Subsystem = TryGetSaveSubsystem())
				{
					FARSaveResult Result;
					if (!Subsystem->DeleteSave(SlotToDelete, Result, bUseDebugSaves))
					{
						if (!PruneSlotFromIndexOffline(SlotToDelete, bUseDebugSaves))
						{
							SetStatus(FString::Printf(TEXT("Delete failed: %s"), *Result.Error));
							return FReply::Handled();
						}
					}
				}
				else
				{
					FString Error;
					if (!DeleteSlotOffline(SlotToDelete, bUseDebugSaves, Error))
					{
						if (!PruneSlotFromIndexOffline(SlotToDelete, bUseDebugSaves))
						{
							SetStatus(FString::Printf(TEXT("Delete failed: %s"), *Error));
							return FReply::Handled();
						}
					}
				}

				if (CurrentSlotName == SlotToDelete)
				{
					CurrentSlotName = NAME_None;
					CurrentSaveObject.Reset();
					LoadedSnapshotBytes.Reset();
					LoadedSnapshotSlotName = NAME_None;
					BindCurrentSaveToDetails();
				}
				++DeletedCount;
			}

			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Deleted %d slot(s)."), DeletedCount));
			return FReply::Handled();
		}

		FReply OnDuplicateSelected()
		{
			TSharedPtr<FARSaveSlotDescriptor> SelectedPrimary;
			if (!GetPrimarySelectionForSingleAction(SelectedPrimary))
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}

			const FName DesiredTargetBase(*SlotNameTextBox->GetText().ToString());
			const FName TargetBase = NormalizeSlotBase(DesiredTargetBase, nullptr);
			if (TargetBase.IsNone())
			{
				SetStatus(TEXT("Provide a target slot base name in the text box."));
				return FReply::Handled();
			}

			FString Error;
			if (!DuplicateOrRenameSlotOffline(SelectedPrimary->SlotName, TargetBase, bUseDebugSaves, /*bRename*/ false, Error))
			{
				SetStatus(FString::Printf(TEXT("Duplicate failed: %s"), *Error));
				return FReply::Handled();
			}

			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Duplicated '%s' -> '%s'."), *SelectedPrimary->SlotName.ToString(), *TargetBase.ToString()));
			return FReply::Handled();
		}

		FReply OnRenameSelected()
		{
			TSharedPtr<FARSaveSlotDescriptor> SelectedPrimary;
			if (!GetPrimarySelectionForSingleAction(SelectedPrimary))
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}

			const FName DesiredTargetBase(*SlotNameTextBox->GetText().ToString());
			const FName TargetBase = NormalizeSlotBase(DesiredTargetBase, nullptr);
			if (TargetBase.IsNone())
			{
				SetStatus(TEXT("Provide a target slot base name in the text box."));
				return FReply::Handled();
			}

			FString Error;
			if (!DuplicateOrRenameSlotOffline(SelectedPrimary->SlotName, TargetBase, bUseDebugSaves, /*bRename*/ true, Error))
			{
				SetStatus(FString::Printf(TEXT("Rename failed: %s"), *Error));
				return FReply::Handled();
			}

			CurrentSlotName = NormalizeSlotBaseForMode(TargetBase, bUseDebugSaves);
			if (!LoadSlotOffline(CurrentSlotName, bUseDebugSaves, Error))
			{
				SetStatus(FString::Printf(TEXT("Rename succeeded but reload failed: %s"), *Error));
				RefreshSlots();
				return FReply::Handled();
			}
			CaptureLoadedSnapshot();
			BindCurrentSaveToDetails();
			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Renamed '%s' -> '%s'."), *SelectedPrimary->SlotName.ToString(), *TargetBase.ToString()));
			return FReply::Handled();
		}

		FReply OnSetDefaultUnlocksCurrent()
		{
			if (!CurrentSaveObject)
			{
				SetStatus(TEXT("No save object loaded."));
				return FReply::Handled();
			}
			const FScopedTransaction Transaction(FText::FromString(TEXT("Debug Save: Set Default Unlocks")));
			CurrentSaveObject->Modify();

			const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
			const FGameplayTagContainer UnlockTags = LoadoutSettings ? LoadoutSettings->DefaultStartingUnlocks : FGameplayTagContainer();

			if (UnlockTags.Num() == 0)
			{
				SetStatus(TEXT("Set default unlocks failed: Default Starting Unlocks is empty in project settings."));
				return FReply::Handled();
			}

			CurrentSaveObject->Unlocks = UnlockTags;

			if (SaveDetailsView.IsValid())
			{
				SaveDetailsView->ForceRefresh();
			}

			SetStatus(FString::Printf(TEXT("Applied default unlocks (%d). Press Save to persist."), UnlockTags.Num()));
			return FReply::Handled();
		}

		FReply OnDefaultLoadoutAllPlayersCurrent()
		{
			if (!CurrentSaveObject)
			{
				SetStatus(TEXT("No save object loaded."));
				return FReply::Handled();
			}
			const FScopedTransaction Transaction(FText::FromString(TEXT("Debug Save: Set Default Loadout")));
			CurrentSaveObject->Modify();

			const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
			const FGameplayTagContainer DefaultLoadout = LoadoutSettings ? LoadoutSettings->DefaultPlayerLoadoutTags : FGameplayTagContainer();
			if (DefaultLoadout.IsEmpty())
			{
				SetStatus(TEXT("Default loadout failed: Default Player Loadout Tags is empty in project settings."));
				return FReply::Handled();
			}

			for (FARPlayerStateSaveData& PlayerData : CurrentSaveObject->PlayerStates)
			{
				PlayerData.LoadoutTags = DefaultLoadout;
			}

			if (SaveDetailsView.IsValid())
			{
				SaveDetailsView->ForceRefresh();
			}

			SetStatus(FString::Printf(TEXT("Applied default loadout to %d player state entries. Press Save Current to persist."), CurrentSaveObject->PlayerStates.Num()));
			return FReply::Handled();
		}

		FReply OnRevertCurrent()
		{
			if (!CurrentSaveObject || CurrentSlotName.IsNone())
			{
				SetStatus(TEXT("No save object loaded."));
				return FReply::Handled();
			}
			if (LoadedSnapshotBytes.Num() == 0 || LoadedSnapshotSlotName != CurrentSlotName)
			{
				SetStatus(TEXT("No revert snapshot available for current slot."));
				return FReply::Handled();
			}

			UARSaveGame* Reverted = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromMemory(LoadedSnapshotBytes));
			if (!Reverted)
			{
				SetStatus(TEXT("Revert failed: snapshot deserialize failed."));
				return FReply::Handled();
			}

			const FScopedTransaction Transaction(FText::FromString(TEXT("Debug Save: Revert Loaded Save")));
			CurrentSaveObject->Modify();
			CurrentSaveObject.Reset(Reverted);
			BindCurrentSaveToDetails();
			SetStatus(FString::Printf(TEXT("Reverted slot '%s' to loaded snapshot."), *GetDisplaySlotName(CurrentSlotName).ToString()));
			return FReply::Handled();
		}

		void BindCurrentSaveToDetails()
		{
			if (CurrentSlotText.IsValid())
			{
				CurrentSlotText->SetText(FText::FromString(
					CurrentSlotName.IsNone()
						? TEXT("Current Slot: <none>")
						: FString::Printf(TEXT("Current Slot: %s"), *GetDisplaySlotName(CurrentSlotName).ToString())));
			}

			if (!SaveDetailsView.IsValid())
			{
				return;
			}

			if (!CurrentSaveObject)
			{
				TArray<UObject*> EmptyObjects;
				SaveDetailsView->SetObjects(EmptyObjects);
				return;
			}
			CurrentSaveObject->SetFlags(RF_Transactional);

			TArray<UObject*> Objects;
			Objects.Add(CurrentSaveObject.Get());
			SaveDetailsView->SetObjects(Objects);
		}

		void SetStatus(const FString& InStatus)
		{
			UE_LOG(ARLog, Log, TEXT("[DebugSaveTool] %s"), *InStatus);
			if (StatusText.IsValid())
			{
				StatusText->SetText(FText::FromString(InStatus));
			}
		}

		UARSaveSubsystem* TryGetSaveSubsystem() const
		{
			if (!GEditor)
			{
				return nullptr;
			}

			UWorld* World = nullptr;
			// Prefer PIE world if running.
			if (GEditor->PlayWorld)
			{
				World = GEditor->PlayWorld;
			}
			else
			{
				World = GEditor->GetEditorWorldContext().World();
			}

			if (!World || !World->GetGameInstance())
			{
				return nullptr;
			}

			return World->GetGameInstance()->GetSubsystem<UARSaveSubsystem>();
		}

		static FName NormalizeSlotBase(FName InName, UARSaveSubsystem* Subsystem)
		{
			FString Base = InName.ToString().TrimStartAndEnd();
			if (Base.IsEmpty() && Subsystem)
			{
				Base = Subsystem->GenerateRandomSlotBaseName(true).ToString();
			}
			else if (Base.IsEmpty())
			{
				Base = FString::Printf(TEXT("Save_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
			}
			return FName(*Base);
		}

		static const TCHAR* GetIndexSlotNameForMode(bool bDebugMode)
		{
			return bDebugMode ? TEXT("SaveIndexDebug") : TEXT("SaveIndex");
		}

		static FName NormalizeSlotBaseForMode(FName SlotBase, bool bDebugMode)
		{
			FString Base = SlotBase.ToString().TrimStartAndEnd();
			if (bDebugMode)
			{
				if (!Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
				{
					Base += DebugSlotSuffix;
				}
			}
			else if (Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
			{
				Base.LeftChopInline(DebugSlotSuffix.Len(), EAllowShrinking::No);
			}
			return FName(*Base);
		}

		static FName LogicalSlotBaseForMode(FName SlotBase, bool bDebugMode)
		{
			FString Base = SlotBase.ToString().TrimStartAndEnd();
			if (bDebugMode && Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
			{
				Base.LeftChopInline(DebugSlotSuffix.Len(), EAllowShrinking::No);
			}
			return FName(*Base);
		}

		FName GetDisplaySlotName(FName SlotBase) const
		{
			return LogicalSlotBaseForMode(SlotBase, bUseDebugSaves);
		}

		static FName BuildRevisionSlotName(FName SlotBase, int32 Revision)
		{
			return FName(*FString::Printf(TEXT("%s__%d"), *SlotBase.ToString(), Revision));
		}

		bool LoadOrCreateIndexOffline(bool bDebugMode, UARSaveIndexGame*& OutIndex, FString& OutError) const
		{
			OutIndex = nullptr;
			const FString IndexSlot = GetIndexSlotNameForMode(bDebugMode);

			if (UGameplayStatics::DoesSaveGameExist(IndexSlot, UARSaveSubsystem::DefaultUserIndex))
			{
				OutIndex = Cast<UARSaveIndexGame>(UGameplayStatics::LoadGameFromSlot(IndexSlot, UARSaveSubsystem::DefaultUserIndex));
				if (!OutIndex)
				{
					UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool] Recreating incompatible save index '%s'."), *IndexSlot);
					UGameplayStatics::DeleteGameInSlot(IndexSlot, UARSaveSubsystem::DefaultUserIndex);
				}
				else
				{
					return true;
				}
			}

			OutIndex = Cast<UARSaveIndexGame>(UGameplayStatics::CreateSaveGameObject(UARSaveIndexGame::StaticClass()));
			if (!OutIndex)
			{
				OutError = TEXT("Failed to create save index.");
				return false;
			}

			if (!UGameplayStatics::SaveGameToSlot(OutIndex, IndexSlot, UARSaveSubsystem::DefaultUserIndex))
			{
				OutError = FString::Printf(TEXT("Failed to save save index '%s'."), *IndexSlot);
				return false;
			}

			return true;
		}

		bool PruneSlotFromIndexOffline(FName InSlotBase, bool bDebugMode)
		{
			FString Error;
			if (DeleteSlotOffline(InSlotBase, bDebugMode, Error))
			{
				return true;
			}

			if (RemoveIndexEntryExactOffline(InSlotBase, bDebugMode, Error))
			{
				return true;
			}

			if (DeleteSlotOffline(InSlotBase, !bDebugMode, Error))
			{
				return true;
			}

			if (RemoveIndexEntryExactOffline(InSlotBase, !bDebugMode, Error))
			{
				return true;
			}

			return false;
		}

		bool HasAnyPhysicalRevisionForSlot(FName InSlotBase, bool bDebugMode) const
		{
			UARSaveIndexGame* IndexObj = nullptr;
			FString Error;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, Error))
			{
				return false;
			}

			const FName SlotBase = NormalizeSlotBaseForMode(InSlotBase, bDebugMode);
			int32 MaxRevision = -1;
			for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
			{
				if (Entry.SlotName == SlotBase)
				{
					MaxRevision = Entry.SlotNumber;
					break;
				}
			}

			if (MaxRevision < 0)
			{
				return false;
			}

			for (int32 Revision = MaxRevision; Revision >= 0; --Revision)
			{
				const FName PhysicalName = BuildRevisionSlotName(SlotBase, Revision);
				if (UGameplayStatics::DoesSaveGameExist(PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex))
				{
					return true;
				}
			}

			return false;
		}

		void PromptRemoveBrokenSlotEntry(FName SlotName, const FString& FailureReason)
		{
			if (HasAnyPhysicalRevisionForSlot(SlotName, bUseDebugSaves))
			{
				return;
			}

			const FText Prompt = FText::FromString(FString::Printf(
				TEXT("Could not find any revision files for '%s'.\n\nReason: %s\n\nRemove this stale entry from the save index?"),
				*GetDisplaySlotName(SlotName).ToString(),
				*FailureReason));
			const EAppReturnType::Type Confirm = FMessageDialog::Open(EAppMsgType::YesNo, Prompt);
			if (Confirm != EAppReturnType::Yes)
			{
				return;
			}

			if (PruneSlotFromIndexOffline(SlotName, bUseDebugSaves))
			{
				RefreshSlots();
				SetStatus(FString::Printf(TEXT("Removed stale slot index entry '%s'."), *GetDisplaySlotName(SlotName).ToString()));
			}
			else
			{
				SetStatus(FString::Printf(TEXT("Failed to remove stale slot index entry '%s'."), *GetDisplaySlotName(SlotName).ToString()));
			}
		}

		bool SaveIndexOffline(bool bDebugMode, UARSaveIndexGame* IndexObj, FString& OutError) const
		{
			if (!IndexObj)
			{
				OutError = TEXT("Save index object was null.");
				return false;
			}

			const FString IndexSlot = GetIndexSlotNameForMode(bDebugMode);
			if (!UGameplayStatics::SaveGameToSlot(IndexObj, IndexSlot, UARSaveSubsystem::DefaultUserIndex))
			{
				OutError = FString::Printf(TEXT("Failed to save save index '%s'."), *IndexSlot);
				return false;
			}
			return true;
		}

		bool ListSlotsOffline(bool bDebugMode, TArray<FARSaveSlotDescriptor>& OutSlots, FString& OutError) const
		{
			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			OutSlots = IndexObj->SlotNames;
			return true;
		}

		bool CreateSlotOffline(FName InSlotBase, bool bDebugMode, FString& OutError)
		{
			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			const FName SlotBase = NormalizeSlotBaseForMode(InSlotBase, bDebugMode);
			for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
			{
				if (Entry.SlotName == SlotBase)
				{
					OutError = FString::Printf(TEXT("Slot '%s' already exists."), *SlotBase.ToString());
					return false;
				}
			}

			UARSaveGame* SaveObject = Cast<UARSaveGame>(UGameplayStatics::CreateSaveGameObject(UARSaveGame::StaticClass()));
			if (!SaveObject)
			{
				OutError = TEXT("Failed to create save object.");
				return false;
			}

			SaveObject->SaveSlot = LogicalSlotBaseForMode(SlotBase, bDebugMode);
			SaveObject->SaveSlotNumber = 0;
			SaveObject->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
			SaveObject->LastSaved = FDateTime::UtcNow();
			if (const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>())
			{
				SaveObject->Unlocks = LoadoutSettings->DefaultStartingUnlocks;
			}
			SaveObject->ValidateAndSanitize(nullptr);

			const FName PhysicalName = BuildRevisionSlotName(SlotBase, 0);
			if (!UGameplayStatics::SaveGameToSlot(SaveObject, PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex))
			{
				OutError = FString::Printf(TEXT("Failed to write save slot '%s'."), *PhysicalName.ToString());
				return false;
			}

			FARSaveSlotDescriptor Descriptor;
			Descriptor.SlotName = SlotBase;
			Descriptor.SlotNumber = 0;
			Descriptor.SaveVersion = SaveObject->SaveGameVersion;
			Descriptor.CyclesPlayed = SaveObject->Cycles;
			Descriptor.LastSavedTime = SaveObject->LastSaved;
			Descriptor.Money = SaveObject->Money;
			IndexObj->SlotNames.Add(Descriptor);
			if (!SaveIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			CurrentSlotName = SlotBase;
			CurrentSaveObject.Reset(SaveObject);
			return true;
		}

		bool LoadSlotOffline(FName InSlotBase, bool bDebugMode, FString& OutError)
		{
			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			const FName SlotBase = NormalizeSlotBaseForMode(InSlotBase, bDebugMode);
			int32 Revision = INDEX_NONE;
			for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
			{
				if (Entry.SlotName == SlotBase)
				{
					Revision = Entry.SlotNumber;
					break;
				}
			}

			if (Revision < 0)
			{
				OutError = FString::Printf(TEXT("Slot '%s' not found."), *SlotBase.ToString());
				return false;
			}

			UARSaveGame* Loaded = nullptr;
			for (int32 TryRevision = Revision; TryRevision >= 0; --TryRevision)
			{
				const FName PhysicalName = BuildRevisionSlotName(SlotBase, TryRevision);
				Loaded = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromSlot(PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex));
				if (Loaded)
				{
					break;
				}
			}

			if (!Loaded)
			{
				OutError = FString::Printf(TEXT("Failed to load any revision for '%s'."), *SlotBase.ToString());
				return false;
			}

			CurrentSlotName = SlotBase;
			CurrentSaveObject.Reset(Loaded);
			return true;
		}

		bool SaveCurrentOffline(bool bDebugMode, FString& OutError)
		{
			if (!CurrentSaveObject || CurrentSlotName.IsNone())
			{
				OutError = TEXT("No current save loaded.");
				return false;
			}

			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			const FName SlotBase = NormalizeSlotBaseForMode(CurrentSlotName, bDebugMode);
			int32 ExistingLatest = -1;
			int32 ExistingIndex = INDEX_NONE;
			for (int32 i = 0; i < IndexObj->SlotNames.Num(); ++i)
			{
				if (IndexObj->SlotNames[i].SlotName == SlotBase)
				{
					ExistingLatest = IndexObj->SlotNames[i].SlotNumber;
					ExistingIndex = i;
					break;
				}
			}

			const int32 NewRevision = ExistingLatest >= 0 ? ExistingLatest + 1 : 0;
			CurrentSaveObject->SaveSlot = LogicalSlotBaseForMode(SlotBase, bDebugMode);
			CurrentSaveObject->SaveSlotNumber = NewRevision;
			CurrentSaveObject->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
			CurrentSaveObject->LastSaved = FDateTime::UtcNow();
			CurrentSaveObject->ValidateAndSanitize(nullptr);

			const FName PhysicalName = BuildRevisionSlotName(SlotBase, NewRevision);
			if (!UGameplayStatics::SaveGameToSlot(CurrentSaveObject.Get(), PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex))
			{
				OutError = FString::Printf(TEXT("Failed to write save slot '%s'."), *PhysicalName.ToString());
				return false;
			}

			FARSaveSlotDescriptor Descriptor;
			Descriptor.SlotName = SlotBase;
			Descriptor.SlotNumber = NewRevision;
			Descriptor.SaveVersion = CurrentSaveObject->SaveGameVersion;
			Descriptor.CyclesPlayed = CurrentSaveObject->Cycles;
			Descriptor.LastSavedTime = CurrentSaveObject->LastSaved;
			Descriptor.Money = CurrentSaveObject->Money;

			if (ExistingIndex >= 0)
			{
				IndexObj->SlotNames[ExistingIndex] = Descriptor;
			}
			else
			{
				IndexObj->SlotNames.Add(Descriptor);
			}

			if (!SaveIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			return true;
		}

		bool DeleteSlotOffline(FName InSlotBase, bool bDebugMode, FString& OutError)
		{
			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			const FName SlotBase = NormalizeSlotBaseForMode(InSlotBase, bDebugMode);
			int32 MaxRevision = -1;
			bool bFoundInIndex = false;
			for (const FARSaveSlotDescriptor& Entry : IndexObj->SlotNames)
			{
				if (Entry.SlotName == SlotBase)
				{
					MaxRevision = Entry.SlotNumber;
					bFoundInIndex = true;
					break;
				}
			}
			if (!bFoundInIndex)
			{
				OutError = FString::Printf(TEXT("Slot '%s' not found in index."), *SlotBase.ToString());
				return false;
			}

			for (int32 Revision = 0; Revision <= MaxRevision; ++Revision)
			{
				const FName PhysicalName = BuildRevisionSlotName(SlotBase, Revision);
				if (UGameplayStatics::DoesSaveGameExist(PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex))
				{
					UGameplayStatics::DeleteGameInSlot(PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex);
				}
			}

			IndexObj->SlotNames.RemoveAll([SlotBase](const FARSaveSlotDescriptor& Entry)
			{
				return Entry.SlotName == SlotBase;
			});

			if (!SaveIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			return true;
		}

		bool RemoveIndexEntryExactOffline(FName ExactSlotBase, bool bDebugMode, FString& OutError)
		{
			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			const int32 RemovedCount = IndexObj->SlotNames.RemoveAll([ExactSlotBase](const FARSaveSlotDescriptor& Entry)
			{
				return Entry.SlotName == ExactSlotBase;
			});
			if (RemovedCount <= 0)
			{
				OutError = FString::Printf(TEXT("Slot '%s' not found in index."), *ExactSlotBase.ToString());
				return false;
			}

			if (!SaveIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}
			return true;
		}

		bool DuplicateOrRenameSlotOffline(FName InSourceSlotBase, FName InTargetSlotBase, bool bDebugMode, bool bRename, FString& OutError)
		{
			UARSaveIndexGame* IndexObj = nullptr;
			if (!LoadOrCreateIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			const FName SourceSlotBase = NormalizeSlotBaseForMode(InSourceSlotBase, bDebugMode);
			const FName TargetSlotBase = NormalizeSlotBaseForMode(InTargetSlotBase, bDebugMode);
			if (SourceSlotBase.IsNone() || TargetSlotBase.IsNone())
			{
				OutError = TEXT("Source/target slot names are invalid.");
				return false;
			}

			if (SourceSlotBase == TargetSlotBase)
			{
				OutError = TEXT("Source and target slot names are the same.");
				return false;
			}

			int32 SourceLatestRevision = INDEX_NONE;
			int32 SourceIndexEntry = INDEX_NONE;
			int32 ExistingTargetEntry = INDEX_NONE;
			for (int32 i = 0; i < IndexObj->SlotNames.Num(); ++i)
			{
				const FARSaveSlotDescriptor& Entry = IndexObj->SlotNames[i];
				if (Entry.SlotName == SourceSlotBase)
				{
					SourceLatestRevision = Entry.SlotNumber;
					SourceIndexEntry = i;
				}
				if (Entry.SlotName == TargetSlotBase)
				{
					ExistingTargetEntry = i;
				}
			}

			if (SourceIndexEntry == INDEX_NONE || SourceLatestRevision < 0)
			{
				OutError = FString::Printf(TEXT("Source slot '%s' was not found."), *SourceSlotBase.ToString());
				return false;
			}
			if (ExistingTargetEntry != INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Target slot '%s' already exists."), *TargetSlotBase.ToString());
				return false;
			}

			UARSaveGame* SourceSave = nullptr;
			int32 ResolvedRevision = INDEX_NONE;
			for (int32 TryRevision = SourceLatestRevision; TryRevision >= 0; --TryRevision)
			{
				const FName PhysicalName = BuildRevisionSlotName(SourceSlotBase, TryRevision);
				SourceSave = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromSlot(PhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex));
				if (SourceSave)
				{
					ResolvedRevision = TryRevision;
					break;
				}
			}

			if (!SourceSave)
			{
				OutError = FString::Printf(TEXT("Failed to load source slot '%s'."), *SourceSlotBase.ToString());
				return false;
			}

			TArray<uint8> SaveBytes;
			if (!UGameplayStatics::SaveGameToMemory(SourceSave, SaveBytes) || SaveBytes.Num() == 0)
			{
				OutError = TEXT("Failed to serialize source save.");
				return false;
			}

			UARSaveGame* NewSave = Cast<UARSaveGame>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
			if (!NewSave)
			{
				OutError = TEXT("Failed to clone source save.");
				return false;
			}

			NewSave->SaveSlot = LogicalSlotBaseForMode(TargetSlotBase, bDebugMode);
			NewSave->SaveSlotNumber = 0;
			NewSave->SaveGameVersion = UARSaveGame::GetCurrentSchemaVersion();
			NewSave->LastSaved = FDateTime::UtcNow();
			NewSave->ValidateAndSanitize(nullptr);

			const FName TargetPhysicalName = BuildRevisionSlotName(TargetSlotBase, 0);
			if (!UGameplayStatics::SaveGameToSlot(NewSave, TargetPhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex))
			{
				OutError = FString::Printf(TEXT("Failed to write target save '%s'."), *TargetPhysicalName.ToString());
				return false;
			}

			FARSaveSlotDescriptor TargetDescriptor;
			TargetDescriptor.SlotName = TargetSlotBase;
			TargetDescriptor.SlotNumber = 0;
			TargetDescriptor.SaveVersion = NewSave->SaveGameVersion;
			TargetDescriptor.CyclesPlayed = NewSave->Cycles;
			TargetDescriptor.LastSavedTime = NewSave->LastSaved;
			TargetDescriptor.Money = NewSave->Money;
			IndexObj->SlotNames.Add(TargetDescriptor);

			if (bRename)
			{
				for (int32 Revision = 0; Revision <= SourceLatestRevision; ++Revision)
				{
					const FName SourcePhysicalName = BuildRevisionSlotName(SourceSlotBase, Revision);
					if (UGameplayStatics::DoesSaveGameExist(SourcePhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex))
					{
						UGameplayStatics::DeleteGameInSlot(SourcePhysicalName.ToString(), UARSaveSubsystem::DefaultUserIndex);
					}
				}

				IndexObj->SlotNames.RemoveAtSwap(SourceIndexEntry, 1, EAllowShrinking::No);
			}

			if (!SaveIndexOffline(bDebugMode, IndexObj, OutError))
			{
				return false;
			}

			if (bRename && CurrentSlotName == SourceSlotBase)
			{
				CurrentSlotName = TargetSlotBase;
				CurrentSaveObject.Reset(NewSave);
			}
			else if (!CurrentSaveObject || CurrentSlotName == SourceSlotBase)
			{
				CurrentSlotName = TargetSlotBase;
				CurrentSaveObject.Reset(NewSave);
			}

			return true;
		}

	private:
		TArray<TSharedPtr<FARSaveSlotDescriptor>> SlotItems;

		TSharedPtr<SEditableTextBox> SlotNameTextBox;
		TSharedPtr<STextBlock> StatusText;
		TSharedPtr<STextBlock> CurrentSlotText;
		TSharedPtr<SListView<TSharedPtr<FARSaveSlotDescriptor>>> SlotListView;
		TSharedPtr<SScrollBar> SlotListScrollbar;
		TSharedPtr<IDetailsView> SaveDetailsView;

		FName CurrentSlotName = NAME_None;
		TStrongObjectPtr<UARSaveGame> CurrentSaveObject;
		TArray<uint8> LoadedSnapshotBytes;
		FName LoadedSnapshotSlotName = NAME_None;
		bool bUseDebugSaves = true;
	};
}

class FAlienRamenEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			ARDebugSaveEditor::TabName,
			FOnSpawnTab::CreateRaw(this, &FAlienRamenEditorModule::SpawnDebugSaveTab))
			.SetDisplayName(FText::FromString("Debug Save Tool"))
			.SetTooltipText(FText::FromString("Create, load, edit, and save isolated debug save slots."))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			ARInvaderAuthoringEditor::TabName,
			FOnSpawnTab::CreateStatic(&ARInvaderAuthoringEditor::SpawnTab))
			.SetDisplayName(FText::FromString("Invader Authoring Tool"))
			.SetTooltipText(FText::FromString("Author invader waves/stages, validate, preview, and run PIE tests."))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			ARInvaderEnemyAuthoringEditor::TabName,
			FOnSpawnTab::CreateStatic(&ARInvaderEnemyAuthoringEditor::SpawnTab))
			.SetDisplayName(FText::FromString("Enemy Authoring Tool"))
			.SetTooltipText(FText::FromString("Author enemy rows, tune runtime stats, and validate enemy definitions."))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAlienRamenEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (UToolMenus::TryGet())
		{
			UToolMenus::UnRegisterStartupCallback(this);
		}
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ARDebugSaveEditor::TabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ARInvaderAuthoringEditor::TabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ARInvaderEnemyAuthoringEditor::TabName);
	}

private:
	TSharedRef<SDockTab> SpawnDebugSaveTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(ARDebugSaveEditor::SPanel)
			];
	}

	void RegisterMenus()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntry(
			"OpenARDebugSaveTool",
			FText::FromString("Alien Ramen Debug Save Tool"),
			FText::FromString("Open the Alien Ramen debug save authoring tool."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
			FToolMenuExecuteAction::CreateRaw(this, &FAlienRamenEditorModule::OpenTab));
		Section.AddMenuEntry(
			"OpenARInvaderAuthoringTool",
			FText::FromString("Alien Ramen Invader Authoring"),
			FText::FromString("Open the invader wave/stage authoring tool."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
			FToolMenuExecuteAction::CreateRaw(this, &FAlienRamenEditorModule::OpenInvaderAuthoringTab));
		Section.AddMenuEntry(
			"OpenAREnemyAuthoringTool",
			FText::FromString("Alien Ramen Enemy Authoring"),
			FText::FromString("Open the dedicated enemy authoring tool."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
			FToolMenuExecuteAction::CreateRaw(this, &FAlienRamenEditorModule::OpenEnemyAuthoringTab));
	}

	void OpenTab(const FToolMenuContext&)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(ARDebugSaveEditor::TabName);
	}

	void OpenInvaderAuthoringTab(const FToolMenuContext&)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(ARInvaderAuthoringEditor::TabName);
	}

	void OpenEnemyAuthoringTab(const FToolMenuContext&)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(ARInvaderEnemyAuthoringEditor::TabName);
	}
};

IMPLEMENT_MODULE(FAlienRamenEditorModule, AlienRamenEditor)
