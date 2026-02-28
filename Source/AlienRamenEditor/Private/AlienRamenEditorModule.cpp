#include "AREnemyAuthoringPanel.h"
#include "ARInvaderAuthoringPanel.h"
#include "ARLog.h"
#include "ARSaveSubsystem.h"
#include "ARSaveGame.h"
#include "ARSaveTypes.h"

#include "IDetailsView.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "GameplayTagsManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "GameFramework/SaveGame.h"

namespace ARDebugSaveEditor
{
	static const FName TabName(TEXT("AR_DebugSaveTool"));
	static const FString DebugSlotSuffix = TEXT("_debug");

	class SPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPanel) {}
		SLATE_END_ARGS()

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
						SNew(SVerticalBox)

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(STextBlock).Text(FText::FromString("Debug Slot Base Name"))
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							SAssignNew(SlotNameTextBox, SEditableTextBox)
							.HintText(FText::FromString("example: sammy_test"))
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(SButton)
								.Text(FText::FromString("Refresh Slots"))
								.OnClicked(this, &SPanel::OnRefreshSlots)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
							[
								SNew(SButton)
								.Text(FText::FromString("Create Slot"))
								.OnClicked(this, &SPanel::OnCreateSlot)
							]
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(SButton)
								.Text(FText::FromString("Load Selected"))
								.OnClicked(this, &SPanel::OnLoadSelected)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
							[
								SNew(SButton)
								.Text(FText::FromString("Delete Selected"))
								.OnClicked(this, &SPanel::OnDeleteSelected)
							]
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Save Current"))
							.OnClicked(this, &SPanel::OnSaveCurrent)
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(SButton)
							.Text(FText::FromString("Unlock All (Current)"))
							.OnClicked(this, &SPanel::OnUnlockAllCurrent)
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
							SAssignNew(SlotListView, SListView<TSharedPtr<FARSaveSlotDescriptor>>)
							.ListItemsSource(&SlotItems)
							.OnGenerateRow(this, &SPanel::OnGenerateRow)
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.65f)
				[
					SNew(SBorder)
					.Padding(8.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
						[
							SNew(STextBlock).Text(FText::FromString("Loaded Save (Auto Property Editor)"))
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SaveDetailsView.ToSharedRef()
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
				*Item->SlotName.ToString(), Item->SlotNumber, Item->Money);
			return SNew(STableRow<TSharedPtr<FARSaveSlotDescriptor>>, OwnerTable)
				[
					SNew(STextBlock).Text(FText::FromString(Label))
				];
		}

		void RefreshSlots()
		{
			FString Error;
			UARSaveSubsystem* Subsystem = GetSaveSubsystem(Error);
			if (!Subsystem)
			{
				SetStatus(Error);
				return;
			}

			TArray<FARSaveSlotDescriptor> Slots;
			FARSaveResult Result;
			if (!Subsystem->ListSaves(Slots, Result))
			{
				SetStatus(FString::Printf(TEXT("Refresh failed: %s"), *Result.Error));
				return;
			}

			SlotItems.Reset();
			for (const FARSaveSlotDescriptor& Entry : Slots)
			{
				if (!IsDebugSlot(Entry.SlotName))
				{
					continue;
				}
				SlotItems.Add(MakeShared<FARSaveSlotDescriptor>(Entry));
			}
			SlotListView->RequestListRefresh();
			SetStatus(FString::Printf(TEXT("Loaded %d debug slots."), SlotItems.Num()));
		}

		FReply OnRefreshSlots()
		{
			RefreshSlots();
			return FReply::Handled();
		}

		FReply OnCreateSlot()
		{
			FString Error;
			UARSaveSubsystem* Subsystem = GetSaveSubsystem(Error);
			if (!Subsystem)
			{
				SetStatus(Error);
				return FReply::Handled();
			}

			FName BaseName(*SlotNameTextBox->GetText().ToString());
			BaseName = NormalizeDebugSlot(BaseName, Subsystem);

			FARSaveSlotDescriptor NewSlot;
			FARSaveResult Result;
			if (!Subsystem->CreateNewSave(BaseName, NewSlot, Result))
			{
				SetStatus(FString::Printf(TEXT("Create failed: %s"), *Result.Error));
				return FReply::Handled();
			}

			CurrentSlotName = NewSlot.SlotName;
			CurrentSaveObject.Reset(Subsystem->GetCurrentSaveGame());
			SlotNameTextBox->SetText(FText::GetEmpty());
			BindCurrentSaveToDetails();
			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Created slot base '%s' at revision %d."), *NewSlot.SlotName.ToString(), NewSlot.SlotNumber));
			return FReply::Handled();
		}

		FReply OnLoadSelected()
		{
			const TArray<TSharedPtr<FARSaveSlotDescriptor>> Selected = SlotListView->GetSelectedItems();
			if (Selected.IsEmpty() || !Selected[0].IsValid())
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}

			FString Error;
			UARSaveSubsystem* Subsystem = GetSaveSubsystem(Error);
			if (!Subsystem)
			{
				SetStatus(Error);
				return FReply::Handled();
			}

			FARSaveResult Result;
			if (!Subsystem->LoadGame(Selected[0]->SlotName, -1, Result))
			{
				SetStatus(FString::Printf(TEXT("Load failed: %s"), *Result.Error));
				return FReply::Handled();
			}

			CurrentSlotName = Selected[0]->SlotName;
			CurrentSaveObject.Reset(Subsystem->GetCurrentSaveGame());
			BindCurrentSaveToDetails();
			SetStatus(FString::Printf(TEXT("Loaded slot '%s'."), *CurrentSlotName.ToString()));
			return FReply::Handled();
		}

		FReply OnSaveCurrent()
		{
			if (!CurrentSaveObject)
			{
				SetStatus(TEXT("No save object loaded."));
				return FReply::Handled();
			}

			FString Error;
			UARSaveSubsystem* Subsystem = GetSaveSubsystem(Error);
			if (!Subsystem)
			{
				SetStatus(Error);
				return FReply::Handled();
			}

			FARSaveResult Result;
			if (!Subsystem->SaveCurrentGame(CurrentSlotName, true, Result))
			{
				SetStatus(FString::Printf(TEXT("Save failed: %s"), *Result.Error));
				return FReply::Handled();
			}

			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Saved slot '%s'."), *CurrentSlotName.ToString()));
			return FReply::Handled();
		}

		FReply OnDeleteSelected()
		{
			const TArray<TSharedPtr<FARSaveSlotDescriptor>> Selected = SlotListView->GetSelectedItems();
			if (Selected.IsEmpty() || !Selected[0].IsValid())
			{
				SetStatus(TEXT("No slot selected."));
				return FReply::Handled();
			}

			const FName SlotToDelete = Selected[0]->SlotName;
			FString Error;
			UARSaveSubsystem* Subsystem = GetSaveSubsystem(Error);
			if (!Subsystem)
			{
				SetStatus(Error);
				return FReply::Handled();
			}

			FARSaveResult Result;
			if (!Subsystem->DeleteSave(SlotToDelete, Result))
			{
				SetStatus(FString::Printf(TEXT("Delete failed: %s"), *Result.Error));
				return FReply::Handled();
			}

			if (CurrentSlotName == SlotToDelete)
			{
				CurrentSlotName = NAME_None;
				CurrentSaveObject.Reset();
				BindCurrentSaveToDetails();
			}

			RefreshSlots();
			SetStatus(FString::Printf(TEXT("Deleted slot '%s'."), *SlotToDelete.ToString()));
			return FReply::Handled();
		}

		FReply OnUnlockAllCurrent()
		{
			if (!CurrentSaveObject)
			{
				SetStatus(TEXT("No save object loaded."));
				return FReply::Handled();
			}

			// Gather all unlock tags and apply directly.
			FGameplayTagContainer UnlockTags;
			FGameplayTagContainer AllTags;
			UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, /*OnlyIncludeDictionaryTags*/ false);
			for (const FGameplayTag& GameplayTag : AllTags)
			{
				if (GameplayTag.ToString().StartsWith(TEXT("Unlock."), ESearchCase::IgnoreCase))
				{
					UnlockTags.AddTag(GameplayTag);
				}
			}

			if (UnlockTags.Num() == 0)
			{
				SetStatus(TEXT("Unlock All failed: no Unlock.* tags found."));
				return FReply::Handled();
			}

			CurrentSaveObject->Unlocks = UnlockTags;

			if (SaveDetailsView.IsValid())
			{
				SaveDetailsView->ForceRefresh();
			}

			SetStatus(FString::Printf(TEXT("Unlocks set to all known unlock tags (%d). Press Save Current to persist."), UnlockTags.Num()));
			return FReply::Handled();
		}

		void BindCurrentSaveToDetails()
		{
			if (CurrentSlotText.IsValid())
			{
				CurrentSlotText->SetText(FText::FromString(
					CurrentSlotName.IsNone()
						? TEXT("Current Slot: <none>")
						: FString::Printf(TEXT("Current Slot: %s"), *CurrentSlotName.ToString())));
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

		UARSaveSubsystem* GetSaveSubsystem(FString& OutError) const
		{
			OutError.Reset();

			if (!GEditor)
			{
				OutError = TEXT("Editor context is not available.");
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
				OutError = TEXT("No active world/game instance. Start PIE to use Debug Save Tool.");
				return nullptr;
			}

			UARSaveSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UARSaveSubsystem>();
			if (!Subsystem)
			{
				OutError = TEXT("UARSaveSubsystem is not available in this world.");
			}
			return Subsystem;
		}

		static bool IsDebugSlot(FName SlotName)
		{
			return SlotName.ToString().EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase);
		}

		static FName NormalizeDebugSlot(FName InName, UARSaveSubsystem* Subsystem)
		{
			FString Base = InName.ToString().TrimStartAndEnd();
			if (Base.IsEmpty() && Subsystem)
			{
				Base = Subsystem->GenerateRandomSlotBaseName(true).ToString();
			}
			if (!Base.EndsWith(DebugSlotSuffix, ESearchCase::IgnoreCase))
			{
				Base += DebugSlotSuffix;
			}
			return FName(*Base);
		}

	private:
		TArray<TSharedPtr<FARSaveSlotDescriptor>> SlotItems;

		TSharedPtr<SEditableTextBox> SlotNameTextBox;
		TSharedPtr<STextBlock> StatusText;
		TSharedPtr<STextBlock> CurrentSlotText;
		TSharedPtr<SListView<TSharedPtr<FARSaveSlotDescriptor>>> SlotListView;
		TSharedPtr<IDetailsView> SaveDetailsView;

		FName CurrentSlotName = NAME_None;
		TStrongObjectPtr<UARSaveGame> CurrentSaveObject;
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
