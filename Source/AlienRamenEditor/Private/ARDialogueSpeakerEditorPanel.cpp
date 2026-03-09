#include "ARDialogueSpeakerEditorPanel.h"

#include "ARContentLookupSettings.h"
#include "ARDialogueConversationAsset.h"
#include "ARDialogueSettings.h"
#include "ARDialogueSubsystem.h"
#include "ContentLookupSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Framework/Docking/TabManager.h"
#include "GameplayTagsManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	class SDialogueSpeakerTableRow final : public SMultiColumnTableRow<TSharedPtr<SDialogueSpeakerEditorPanel::FSpeakerEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SDialogueSpeakerTableRow) {}
			SLATE_ARGUMENT(TSharedPtr<SDialogueSpeakerEditorPanel::FSpeakerEntry>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<SDialogueSpeakerEditorPanel::FSpeakerEntry>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock).Text(FText::FromString(TEXT("<Invalid>")));
			}

			if (ColumnName == TEXT("DisplayName"))
			{
				return SNew(STextBlock).Text(Item->Row.DisplayName);
			}
			if (ColumnName == TEXT("SpeakerTag"))
			{
				return SNew(STextBlock).Text(FText::FromString(Item->Row.SpeakerTag.ToString()));
			}
			if (ColumnName == TEXT("Faction"))
			{
				return SNew(STextBlock).Text(FText::FromString(Item->Row.FactionTag.ToString()));
			}
			if (ColumnName == TEXT("Thresholds"))
			{
				return SNew(STextBlock).Text(FText::FromString(Item->ThresholdSummary));
			}
			if (ColumnName == TEXT("ConversationCount"))
			{
				return SNew(STextBlock).Text(FText::AsNumber(Item->ConversationCount));
			}
			return SNew(STextBlock).Text(FText::GetEmpty());
		}

	private:
		TSharedPtr<SDialogueSpeakerEditorPanel::FSpeakerEntry> Item;
	};

	static UARDialogueSubsystem* GetDialogueSubsystemFromPIE()
	{
		if (!GEditor)
		{
			return nullptr;
		}

		const FWorldContext* PIEContext = GEditor->GetPIEWorldContext();
		if (!PIEContext || !PIEContext->OwningGameInstance)
		{
			return nullptr;
		}

		return PIEContext->OwningGameInstance->GetSubsystem<UARDialogueSubsystem>();
	}

	static UARDialogueSubsystem* GetTransientDialogueValidationSubsystem()
	{
		static TWeakObjectPtr<UARDialogueSubsystem> Cached;
		if (!Cached.IsValid())
		{
			Cached = NewObject<UARDialogueSubsystem>(GetTransientPackage());
		}
		return Cached.Get();
	}

	static void GatherConversationAssets(const UARDialogueSettings* DialogueSettings, TMap<FGameplayTag, UARDialogueConversationAsset*>& OutConversationsByTag)
	{
		OutConversationsByTag.Reset();
		if (!DialogueSettings)
		{
			return;
		}

		auto TryAddConversation = [&OutConversationsByTag](UARDialogueConversationAsset* Conversation, const FGameplayTag& SuggestedTag)
		{
			if (!Conversation)
			{
				return;
			}

			const FGameplayTag Tag = Conversation->Header.ConversationTag.IsValid()
				? Conversation->Header.ConversationTag
				: SuggestedTag;
			if (!Tag.IsValid())
			{
				return;
			}

			if (OutConversationsByTag.Contains(Tag))
			{
				return;
			}

			OutConversationsByTag.Add(Tag, Conversation);
		};

		for (const TSoftObjectPtr<UARDialogueConversationAsset>& ConversationRef : DialogueSettings->ConversationAssets)
		{
			TryAddConversation(ConversationRef.LoadSynchronous(), FGameplayTag());
		}

		const UARContentLookupSettings* LookupSettings = GetDefault<UARContentLookupSettings>();
		if (LookupSettings && DialogueSettings->ConversationDefinitionRootTag.IsValid())
		{
			UContentLookupRegistry* Registry = LookupSettings->RegistryAsset.LoadSynchronous();
			const FContentLookupRoute* Route = Registry
				? Registry->Routes.FindByPredicate([DialogueSettings](const FContentLookupRoute& R)
				{
					return R.RootTag.MatchesTagExact(DialogueSettings->ConversationDefinitionRootTag);
				})
				: nullptr;

			UDataTable* ConversationTable = Route ? Route->DataTable.LoadSynchronous() : nullptr;
			if (ConversationTable && ConversationTable->GetRowStruct() == FDialogueConversationAssetRow::StaticStruct())
			{
				for (const FName RowName : ConversationTable->GetRowNames())
				{
					const FDialogueConversationAssetRow* Row = ConversationTable->FindRow<FDialogueConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorConversations"), false);
					if (!Row)
					{
						continue;
					}

					FGameplayTag RowTag = Row->ConversationTag;
					if (!RowTag.IsValid())
					{
						RowTag = UGameplayTagsManager::Get().RequestGameplayTag(
							FName(*(DialogueSettings->ConversationDefinitionRootTag.ToString() + TEXT(".") + RowName.ToString())),
							false);
					}

					TryAddConversation(Row->Conversation.LoadSynchronous(), RowTag);
				}
			}
		}
	}

	static FString BuildConversationEntryLabel(const UARDialogueConversationAsset* Conversation)
	{
		if (!Conversation)
		{
			return FString();
		}

		const FDialogueConversationHeader& Header = Conversation->Header;
		int32 AddedProgressionTags = 0;
		int32 RemovedProgressionTags = 0;
		for (const FDialogueCompiledNode& Node : Conversation->CompiledData.Nodes)
		{
			if (Node.NodeType != EDialogueNodeType::TagMutation)
			{
				continue;
			}

			const FDialogueTagMutationNodeData* MutationData = Node.NodeData.GetPtr<FDialogueTagMutationNodeData>();
			if (!MutationData)
			{
				continue;
			}

			for (const FDialogueTagMutation& Mutation : MutationData->Mutations)
			{
				if (Mutation.Operation == EDialogueTagMutationOp::Add)
				{
					++AddedProgressionTags;
				}
				else
				{
					++RemovedProgressionTags;
				}
			}
		}

		const FString ReofferSummary = FString::Printf(
			TEXT("SeenGameBlock:%s SeenPlayerBlock:%s CompletedGameBlock:%s"),
			Header.bSeenByGameBlocksReoffer ? TEXT("Y") : TEXT("N"),
			Header.bSeenByPlayerBlocksReoffer ? TEXT("Y") : TEXT("N"),
			Header.bCompletedByGameBlocksReoffer ? TEXT("Y") : TEXT("N"));
		const FString UnlockSummary = FString::Printf(
			TEXT("Locked:%d Blocked:%d Chain:+%d/-%d"),
			Header.LockedConditions.Conditions.Num(),
			Header.BlockedConditions.Conditions.Num(),
			AddedProgressionTags,
			RemovedProgressionTags);

		return FString::Printf(
			TEXT("%s | %s | Pri %d | Important:%s | Repeatable:%s | MinRel %.1f | %s | %s"),
			*Header.DisplayTitle.ToString(),
			*Header.ConversationTag.ToString(),
			Header.Priority,
			Header.bImportant ? TEXT("Y") : TEXT("N"),
			Header.bRepeatable ? TEXT("Y") : TEXT("N"),
			Header.MinimumRelationshipPoints,
			*UnlockSummary,
			*ReofferSummary);
	}
}

void SDialogueSpeakerEditorPanel::Construct(const FArguments& InArgs)
{
	(void)InArgs;
	RefreshData();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Refresh"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleRefresh)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("New Speaker"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleNewSpeaker)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Duplicate Speaker"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleDuplicateSpeaker)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Delete Speaker"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleDeleteSpeaker)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Validate Speaker"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleValidateSpeaker)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Save Speaker"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleSaveSpeaker)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Create Conversation"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleCreateConversation)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Open Conversation"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleOpenConversation)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("Find Broken Conversations"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleFindBrokenConversations)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.42f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SAssignNew(SearchTextBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Search display name / tag")))
						.OnTextChanged_Lambda([this](const FText&){ ApplySpeakerFilterAndSort(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SAssignNew(FactionFilterTextBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Filter by faction tag")))
						.OnTextChanged_Lambda([this](const FText&){ ApplySpeakerFilterAndSort(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Sort Name"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleSortByName)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Sort Tag"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleSortByTag)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).Text(FText::FromString(TEXT("Sort Conversations"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleSortByConversationCount)
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SAssignNew(SpeakerListView, SListView<TSharedPtr<FSpeakerEntry>>)
						.ListItemsSource(&FilteredSpeakerEntries)
						.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGenerateSpeakerRow)
						.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnSpeakerSelectionChanged)
						.HeaderRow(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(TEXT("DisplayName")).DefaultLabel(FText::FromString(TEXT("Display Name"))).FillWidth(0.25f)
							+ SHeaderRow::Column(TEXT("SpeakerTag")).DefaultLabel(FText::FromString(TEXT("Speaker Tag"))).FillWidth(0.28f)
							+ SHeaderRow::Column(TEXT("Faction")).DefaultLabel(FText::FromString(TEXT("Faction"))).FillWidth(0.18f)
							+ SHeaderRow::Column(TEXT("Thresholds")).DefaultLabel(FText::FromString(TEXT("Relationship Thresholds"))).FillWidth(0.19f)
							+ SHeaderRow::Column(TEXT("ConversationCount")).DefaultLabel(FText::FromString(TEXT("Conversations"))).FillWidth(0.10f)
						)
					]
				]
			]
			+ SSplitter::Slot().Value(0.58f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Speaker Details")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(DisplayNameTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Display Name")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(DescriptionTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Description")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(SpeakerTagTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Speaker Tag (Dialogue.Speaker.*)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SAssignNew(FactionTagTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Faction Tag (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Portrait Configuration")))
					]
					+ SVerticalBox::Slot().AutoHeight().MaxHeight(120.0f).Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(PortraitListView, SListView<TSharedPtr<FPortraitEntry>>)
						.ListItemsSource(&PortraitEntries)
						.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGeneratePortraitRow)
						.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnPortraitSelectionChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(PortraitTagTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Portrait Tag (Dialogue.Speaker.*.*)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(PortraitTexturePathTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Portrait Texture Soft Path (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Add/Update Portrait"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleAddPortrait)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).Text(FText::FromString(TEXT("Remove Portrait"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleRemovePortrait)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Relationship Thresholds")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(ThresholdsTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Comma-separated floats (e.g. 50,150,300,500)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SButton).Text(FText::FromString(TEXT("Reset Thresholds To Defaults"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleResetThresholds)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Conversation Map (Primary Speaker only)")))
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SAssignNew(ConversationListView, SListView<TSharedPtr<FConversationEntry>>)
						.ListItemsSource(&ConversationEntries)
						.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGenerateConversationRow)
						.OnMouseButtonDoubleClick(this, &SDialogueSpeakerEditorPanel::OnConversationDoubleClicked)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FText::FromString(ValidationOutput); })
						.AutoWrapText(true)
					]
				]
			]
		]
	];

	SyncSpeakerFieldsFromSelection();
}

bool SDialogueSpeakerEditorPanel::ResolveSpeakerDataTable(UDataTable*& OutTable, FString& OutError) const
{
	OutTable = nullptr;
	OutError.Empty();

	const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
	if (!DialogueSettings)
	{
		OutError = TEXT("Dialogue settings are missing.");
		return false;
	}
	if (!DialogueSettings->SpeakerDefinitionRootTag.IsValid())
	{
		OutError = TEXT("Dialogue SpeakerDefinitionRootTag is invalid.");
		return false;
	}

	const UARContentLookupSettings* LookupSettings = GetDefault<UARContentLookupSettings>();
	if (!LookupSettings || LookupSettings->RegistryAsset.IsNull())
	{
		OutError = TEXT("Content lookup registry asset is not configured.");
		return false;
	}

	UContentLookupRegistry* Registry = LookupSettings->RegistryAsset.LoadSynchronous();
	if (!Registry)
	{
		OutError = TEXT("Failed to load content lookup registry asset.");
		return false;
	}

	const FContentLookupRoute* SpeakerRoute = Registry->Routes.FindByPredicate([DialogueSettings](const FContentLookupRoute& Route)
	{
		return Route.RootTag.MatchesTagExact(DialogueSettings->SpeakerDefinitionRootTag);
	});
	if (!SpeakerRoute)
	{
		OutError = FString::Printf(TEXT("No content lookup route matches speaker root '%s'."), *DialogueSettings->SpeakerDefinitionRootTag.ToString());
		return false;
	}

	OutTable = SpeakerRoute->DataTable.LoadSynchronous();
	if (!OutTable)
	{
		OutError = FString::Printf(TEXT("Failed to load speaker data table '%s'."), *SpeakerRoute->DataTable.ToString());
		return false;
	}

	if (OutTable->GetRowStruct() != FDialogueSpeakerRow::StaticStruct())
	{
		OutError = FString::Printf(TEXT("Speaker data table row struct mismatch. Expected '%s', got '%s'."),
			*FDialogueSpeakerRow::StaticStruct()->GetName(),
			*GetNameSafe(OutTable->GetRowStruct()));
		return false;
	}

	return true;
}

void SDialogueSpeakerEditorPanel::RefreshData()
{
	AllSpeakerEntries.Reset();
	FilteredSpeakerEntries.Reset();
	ConversationEntries.Reset();
	PortraitEntries.Reset();
	ValidationOutput.Empty();
	SpeakerDataTable.Reset();
	UDataTable* SpeakerTable = nullptr;
	FString ResolveError;
	if (!ResolveSpeakerDataTable(SpeakerTable, ResolveError))
	{
		AppendLogLine(ResolveError);
		if (SpeakerListView.IsValid())
		{
			SpeakerListView->RequestListRefresh();
		}
		if (ConversationListView.IsValid())
		{
			ConversationListView->RequestListRefresh();
		}
		if (PortraitListView.IsValid())
		{
			PortraitListView->RequestListRefresh();
		}
		return;
	}

	SpeakerDataTable = SpeakerTable;
	const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();

	TMap<FGameplayTag, int32> ConversationCountBySpeaker;
	if (DialogueSettings)
	{
		for (const TSoftObjectPtr<UARDialogueConversationAsset>& ConversationRef : DialogueSettings->ConversationAssets)
		{
			UARDialogueConversationAsset* Conversation = ConversationRef.LoadSynchronous();
			if (!Conversation || !Conversation->Header.PrimarySpeakerTag.IsValid())
			{
				continue;
			}
			ConversationCountBySpeaker.FindOrAdd(Conversation->Header.PrimarySpeakerTag) += 1;
		}
	}

	const TArray<FName> RowNames = SpeakerTable->GetRowNames();
	for (const FName RowName : RowNames)
	{
		const FDialogueSpeakerRow* Row = SpeakerTable->FindRow<FDialogueSpeakerRow>(RowName, TEXT("DialogueSpeakerEditor"), false);
		if (!Row)
		{
			continue;
		}

		TSharedPtr<FSpeakerEntry> Entry = MakeShared<FSpeakerEntry>();
		Entry->RowName = RowName;
		Entry->Row = *Row;
		if (!Entry->Row.SpeakerTag.IsValid() && DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
		{
			const FString BuiltPath = FString::Printf(TEXT("%s.%s"), *DialogueSettings->SpeakerDefinitionRootTag.ToString(), *RowName.ToString());
			Entry->Row.SpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*BuiltPath), false);
		}
		Entry->ConversationCount = ConversationCountBySpeaker.FindRef(Entry->Row.SpeakerTag);
		Entry->ThresholdSummary = BuildThresholdSummary(Entry->Row.RelationshipThresholds);
		AllSpeakerEntries.Add(Entry);
	}

	ApplySpeakerFilterAndSort();

	const bool bSelectionStillValid = AllSpeakerEntries.ContainsByPredicate([this](const TSharedPtr<FSpeakerEntry>& Entry)
	{
		return Entry.IsValid() && Entry->RowName == SelectedSpeakerRowName;
	});

	if (!bSelectionStillValid)
	{
		SelectedSpeakerRowName = FilteredSpeakerEntries.IsEmpty() || !FilteredSpeakerEntries[0].IsValid()
			? NAME_None
			: FilteredSpeakerEntries[0]->RowName;
	}

	SetSelectedSpeakerRow(SelectedSpeakerRowName);
}

void SDialogueSpeakerEditorPanel::RefreshConversationMap()
{
	ConversationEntries.Reset();

	TSharedPtr<FSpeakerEntry> SelectedSpeaker;
	for (const TSharedPtr<FSpeakerEntry>& Entry : AllSpeakerEntries)
	{
		if (Entry.IsValid() && Entry->RowName == SelectedSpeakerRowName)
		{
			SelectedSpeaker = Entry;
			break;
		}
	}

	if (!SelectedSpeaker.IsValid())
	{
		if (ConversationListView.IsValid())
		{
			ConversationListView->RequestListRefresh();
		}
		return;
	}

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	if (!Settings)
	{
		if (ConversationListView.IsValid())
		{
			ConversationListView->RequestListRefresh();
		}
		return;
	}

	TMap<FGameplayTag, UARDialogueConversationAsset*> ConversationsByTag;
	GatherConversationAssets(Settings, ConversationsByTag);

	for (const TPair<FGameplayTag, UARDialogueConversationAsset*>& Pair : ConversationsByTag)
	{
		UARDialogueConversationAsset* Conversation = Pair.Value;
		if (!Conversation || !Conversation->Header.PrimarySpeakerTag.MatchesTagExact(SelectedSpeaker->Row.SpeakerTag))
		{
			continue;
		}

		TSharedPtr<FConversationEntry> Entry = MakeShared<FConversationEntry>();
		Entry->Asset = Conversation;
		Entry->ConversationTag = Conversation->Header.ConversationTag;
		Entry->PrimarySpeakerTag = Conversation->Header.PrimarySpeakerTag;
		Entry->Priority = Conversation->Header.Priority;
		Entry->MinimumRelationshipPoints = Conversation->Header.MinimumRelationshipPoints;
		Entry->bImportant = Conversation->Header.bImportant;
		Entry->bRepeatable = Conversation->Header.bRepeatable;
		Entry->RelationshipBand = ComputeRelationshipBand(Entry->MinimumRelationshipPoints, SelectedSpeaker->Row.RelationshipThresholds);
		Entry->RelationshipBandLabel = BuildRelationshipBandLabel(Entry->RelationshipBand, SelectedSpeaker->Row.RelationshipThresholds);
		Entry->Label = BuildConversationEntryLabel(Conversation);
		ConversationEntries.Add(Entry);
	}

	ConversationEntries.Sort([](const TSharedPtr<FConversationEntry>& Lhs, const TSharedPtr<FConversationEntry>& Rhs)
	{
		if (!Lhs.IsValid() || !Rhs.IsValid())
		{
			return Lhs.IsValid();
		}
		if (Lhs->RelationshipBand != Rhs->RelationshipBand)
		{
			return Lhs->RelationshipBand < Rhs->RelationshipBand;
		}
		if (Lhs->Priority != Rhs->Priority)
		{
			return Lhs->Priority > Rhs->Priority;
		}
		return Lhs->ConversationTag.ToString() < Rhs->ConversationTag.ToString();
	});

	if (ConversationListView.IsValid())
	{
		ConversationListView->RequestListRefresh();
	}
}

void SDialogueSpeakerEditorPanel::RefreshPortraitList()
{
	PortraitEntries.Reset();
	SelectedPortraitIndex = INDEX_NONE;
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		if (PortraitListView.IsValid())
		{
			PortraitListView->RequestListRefresh();
		}
		return;
	}

	const FDialogueSpeakerRow* Row = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!Row)
	{
		if (PortraitListView.IsValid())
		{
			PortraitListView->RequestListRefresh();
		}
		return;
	}

	for (int32 Index = 0; Index < Row->Portraits.Num(); ++Index)
	{
		const FSpeakerPortraitEntry& Portrait = Row->Portraits[Index];
		TSharedPtr<FPortraitEntry> Entry = MakeShared<FPortraitEntry>();
		Entry->PortraitIndex = Index;
		Entry->Label = FString::Printf(TEXT("%d | %s | %s"), Index, *Portrait.PortraitTag.ToString(), *Portrait.Portrait.PortraitTexture.ToString());
		PortraitEntries.Add(Entry);
	}

	if (PortraitListView.IsValid())
	{
		PortraitListView->RequestListRefresh();
	}
}

void SDialogueSpeakerEditorPanel::ApplySpeakerFilterAndSort()
{
	FilteredSpeakerEntries.Reset();

	const FString SearchQuery = SearchTextBox.IsValid() ? SearchTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString FactionFilter = FactionFilterTextBox.IsValid() ? FactionFilterTextBox->GetText().ToString().TrimStartAndEnd() : FString();

	for (const TSharedPtr<FSpeakerEntry>& Entry : AllSpeakerEntries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		bool bMatchesSearch = true;
		if (!SearchQuery.IsEmpty())
		{
			const FString Display = Entry->Row.DisplayName.ToString();
			const FString SpeakerTagString = Entry->Row.SpeakerTag.ToString();
			const FString RowName = Entry->RowName.ToString();
			bMatchesSearch = Display.Contains(SearchQuery, ESearchCase::IgnoreCase)
				|| SpeakerTagString.Contains(SearchQuery, ESearchCase::IgnoreCase)
				|| RowName.Contains(SearchQuery, ESearchCase::IgnoreCase);
		}
		if (!bMatchesSearch)
		{
			continue;
		}

		if (!FactionFilter.IsEmpty())
		{
			const FString Faction = Entry->Row.FactionTag.ToString();
			if (!Faction.Contains(FactionFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		FilteredSpeakerEntries.Add(Entry);
	}

	FilteredSpeakerEntries.Sort([this](const TSharedPtr<FSpeakerEntry>& Lhs, const TSharedPtr<FSpeakerEntry>& Rhs)
	{
		if (!Lhs.IsValid() || !Rhs.IsValid())
		{
			return Lhs.IsValid();
		}

		int32 CompareResult = 0;
		switch (CurrentSortMode)
		{
		case ESpeakerSortMode::DisplayName:
			CompareResult = Lhs->Row.DisplayName.ToString().Compare(Rhs->Row.DisplayName.ToString(), ESearchCase::IgnoreCase);
			break;
		case ESpeakerSortMode::SpeakerTag:
			CompareResult = Lhs->Row.SpeakerTag.ToString().Compare(Rhs->Row.SpeakerTag.ToString(), ESearchCase::IgnoreCase);
			break;
		case ESpeakerSortMode::ConversationCount:
			if (Lhs->ConversationCount != Rhs->ConversationCount)
			{
				CompareResult = Lhs->ConversationCount < Rhs->ConversationCount ? -1 : 1;
			}
			break;
		default:
			break;
		}

		if (CompareResult == 0)
		{
			CompareResult = Lhs->RowName.ToString().Compare(Rhs->RowName.ToString(), ESearchCase::IgnoreCase);
		}

		return bSortAscending ? CompareResult < 0 : CompareResult > 0;
	});

	if (SpeakerListView.IsValid())
	{
		SpeakerListView->RequestListRefresh();
	}
}

bool SDialogueSpeakerEditorPanel::ValidateConversationWithBestAvailable(UARDialogueConversationAsset* Conversation, FDialogueValidationReport& OutReport) const
{
	if (UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemFromPIE())
	{
		return DialogueSubsystem->ValidateConversation(Conversation, OutReport);
	}

	if (UARDialogueSubsystem* TempValidationSubsystem = GetTransientDialogueValidationSubsystem())
	{
		return TempValidationSubsystem->ValidateConversation(Conversation, OutReport);
	}

	OutReport = FDialogueValidationReport();
	return false;
}

bool SDialogueSpeakerEditorPanel::ValidateSpeakerWithBestAvailable(const FDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const
{
	if (UARDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemFromPIE())
	{
		return DialogueSubsystem->ValidateSpeaker(SpeakerRow, OutReport);
	}

	if (UARDialogueSubsystem* TempValidationSubsystem = GetTransientDialogueValidationSubsystem())
	{
		return TempValidationSubsystem->ValidateSpeaker(SpeakerRow, OutReport);
	}

	OutReport = FDialogueValidationReport();
	return false;
}

int32 SDialogueSpeakerEditorPanel::ComputeRelationshipBand(float MinimumRelationshipPoints, const TArray<float>& Thresholds) const
{
	int32 Band = 0;
	for (const float Threshold : Thresholds)
	{
		if (MinimumRelationshipPoints >= Threshold)
		{
			++Band;
		}
	}
	return Band;
}
FString SDialogueSpeakerEditorPanel::BuildRelationshipBandLabel(int32 BandIndex, const TArray<float>& Thresholds) const
{
	if (BandIndex <= 0)
	{
		return TEXT("Band 0 (Base)");
	}

	const int32 ThresholdIndex = FMath::Clamp(BandIndex - 1, 0, Thresholds.Num() - 1);
	if (Thresholds.IsValidIndex(ThresholdIndex))
	{
		return FString::Printf(TEXT("Band %d (>= %.1f)"), BandIndex, Thresholds[ThresholdIndex]);
	}

	return FString::Printf(TEXT("Band %d"), BandIndex);
}

FString SDialogueSpeakerEditorPanel::BuildThresholdSummary(const TArray<float>& Thresholds) const
{
	if (Thresholds.IsEmpty())
	{
		return TEXT("<none>");
	}

	FString Result;
	for (int32 Index = 0; Index < Thresholds.Num(); ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(", ");
		}
		Result += FString::SanitizeFloat(Thresholds[Index]);
	}
	return Result;
}

bool SDialogueSpeakerEditorPanel::ParseThresholdsText(const FString& SourceText, TArray<float>& OutThresholds, FString& OutError) const
{
	OutThresholds.Reset();
	OutError.Empty();

	TArray<FString> Tokens;
	SourceText.ParseIntoArray(Tokens, TEXT(","), true);
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}

		if (!Token.IsNumeric())
		{
			OutError = FString::Printf(TEXT("Threshold '%s' is not numeric."), *Token);
			return false;
		}

		OutThresholds.Add(FCString::Atof(*Token));
	}

	if (OutThresholds.IsEmpty())
	{
		OutError = TEXT("Relationship thresholds must not be empty.");
		return false;
	}

	float LastValue = -FLT_MAX;
	for (const float Value : OutThresholds)
	{
		if (Value <= LastValue)
		{
			OutError = TEXT("Relationship thresholds must be strictly ascending.");
			return false;
		}
		LastValue = Value;
	}

	return true;
}

bool SDialogueSpeakerEditorPanel::CommitEditedSpeakerRow(FString& OutError)
{
	OutError.Empty();

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		OutError = TEXT("No speaker selected.");
		return false;
	}

	FDialogueSpeakerRow* MutableRow = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!MutableRow)
	{
		OutError = TEXT("Selected speaker row could not be resolved in table.");
		return false;
	}

	const FString DisplayNameText = DisplayNameTextBox.IsValid() ? DisplayNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString DescriptionText = DescriptionTextBox.IsValid() ? DescriptionTextBox->GetText().ToString() : FString();
	const FString SpeakerTagText = SpeakerTagTextBox.IsValid() ? SpeakerTagTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString FactionTagText = FactionTagTextBox.IsValid() ? FactionTagTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString ThresholdsText = ThresholdsTextBox.IsValid() ? ThresholdsTextBox->GetText().ToString() : FString();

	if (DisplayNameText.IsEmpty())
	{
		OutError = TEXT("Display Name is required.");
		return false;
	}
	if (SpeakerTagText.IsEmpty())
	{
		OutError = TEXT("Speaker Tag is required.");
		return false;
	}

	const FGameplayTag SpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*SpeakerTagText), false);
	if (!SpeakerTag.IsValid())
	{
		OutError = FString::Printf(TEXT("Speaker Tag '%s' is not a registered gameplay tag."), *SpeakerTagText);
		return false;
	}

	FGameplayTag FactionTag;
	if (!FactionTagText.IsEmpty())
	{
		FactionTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*FactionTagText), false);
		if (!FactionTag.IsValid())
		{
			OutError = FString::Printf(TEXT("Faction Tag '%s' is not a registered gameplay tag."), *FactionTagText);
			return false;
		}
	}

	TArray<float> ParsedThresholds;
	if (!ParseThresholdsText(ThresholdsText, ParsedThresholds, OutError))
	{
		return false;
	}

	SpeakerTable->Modify();
	MutableRow->DisplayName = FText::FromString(DisplayNameText);
	MutableRow->Description = FText::FromString(DescriptionText);
	MutableRow->SpeakerTag = SpeakerTag;
	MutableRow->FactionTag = FactionTag;
	MutableRow->RelationshipThresholds = ParsedThresholds;
	SpeakerTable->MarkPackageDirty();
	return true;
}

void SDialogueSpeakerEditorPanel::AppendLogLine(const FString& Message)
{
	if (!ValidationOutput.IsEmpty())
	{
		ValidationOutput += TEXT("\n");
	}
	ValidationOutput += Message;
}

void SDialogueSpeakerEditorPanel::SyncSpeakerFieldsFromSelection()
{
	FDialogueSpeakerRow SelectedRow;
	bool bHasSelection = false;

	for (const TSharedPtr<FSpeakerEntry>& Entry : AllSpeakerEntries)
	{
		if (Entry.IsValid() && Entry->RowName == SelectedSpeakerRowName)
		{
			SelectedRow = Entry->Row;
			bHasSelection = true;
			break;
		}
	}

	if (!bHasSelection)
	{
		if (DisplayNameTextBox.IsValid()) { DisplayNameTextBox->SetText(FText::GetEmpty()); }
		if (DescriptionTextBox.IsValid()) { DescriptionTextBox->SetText(FText::GetEmpty()); }
		if (SpeakerTagTextBox.IsValid()) { SpeakerTagTextBox->SetText(FText::GetEmpty()); }
		if (FactionTagTextBox.IsValid()) { FactionTagTextBox->SetText(FText::GetEmpty()); }
		if (ThresholdsTextBox.IsValid()) { ThresholdsTextBox->SetText(FText::GetEmpty()); }
		RefreshPortraitList();
		RefreshConversationMap();
		return;
	}

	if (DisplayNameTextBox.IsValid()) { DisplayNameTextBox->SetText(SelectedRow.DisplayName); }
	if (DescriptionTextBox.IsValid()) { DescriptionTextBox->SetText(SelectedRow.Description); }
	if (SpeakerTagTextBox.IsValid()) { SpeakerTagTextBox->SetText(FText::FromString(SelectedRow.SpeakerTag.ToString())); }
	if (FactionTagTextBox.IsValid()) { FactionTagTextBox->SetText(FText::FromString(SelectedRow.FactionTag.ToString())); }
	if (ThresholdsTextBox.IsValid()) { ThresholdsTextBox->SetText(FText::FromString(BuildThresholdSummary(SelectedRow.RelationshipThresholds))); }

	RefreshPortraitList();
	RefreshConversationMap();
}

void SDialogueSpeakerEditorPanel::SetSortMode(const ESpeakerSortMode NewMode)
{
	if (CurrentSortMode == NewMode)
	{
		bSortAscending = !bSortAscending;
	}
	else
	{
		CurrentSortMode = NewMode;
		bSortAscending = true;
	}

	ApplySpeakerFilterAndSort();
}

void SDialogueSpeakerEditorPanel::SetSelectedSpeakerRow(const FName RowName)
{
	SelectedSpeakerRowName = RowName;
	SelectedPortraitIndex = INDEX_NONE;

	if (SpeakerListView.IsValid())
	{
		SpeakerListView->ClearSelection();
		for (const TSharedPtr<FSpeakerEntry>& Entry : FilteredSpeakerEntries)
		{
			if (Entry.IsValid() && Entry->RowName == SelectedSpeakerRowName)
			{
				SpeakerListView->SetSelection(Entry);
				break;
			}
		}
	}

	SyncSpeakerFieldsFromSelection();
}
FReply SDialogueSpeakerEditorPanel::HandleRefresh()
{
	RefreshData();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleNewSpeaker()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable)
	{
		AppendLogLine(TEXT("Speaker data table is unavailable."));
		return FReply::Handled();
	}

	const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
	const FGameplayTag RootTag = DialogueSettings ? DialogueSettings->SpeakerDefinitionRootTag : FGameplayTag();

	FName NewRowName(TEXT("Speaker"));
	for (int32 Suffix = 1; SpeakerTable->GetRowMap().Contains(NewRowName); ++Suffix)
	{
		NewRowName = FName(*FString::Printf(TEXT("Speaker_%d"), Suffix));
	}

	FDialogueSpeakerRow NewRow;
	NewRow.DisplayName = FText::FromString(NewRowName.ToString());
	NewRow.Description = FText::GetEmpty();
	NewRow.RelationshipThresholds = { 50.0f, 150.0f, 300.0f, 500.0f };
	if (RootTag.IsValid())
	{
		const FString SpeakerPath = FString::Printf(TEXT("%s.%s"), *RootTag.ToString(), *NewRowName.ToString());
		NewRow.SpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*SpeakerPath), false);
	}

	SpeakerTable->Modify();
	SpeakerTable->AddRow(NewRowName, NewRow);
	SpeakerTable->MarkPackageDirty();

	AppendLogLine(FString::Printf(TEXT("Created speaker row '%s'."), *NewRowName.ToString()));
	RefreshData();
	SetSelectedSpeakerRow(NewRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleDuplicateSpeaker()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	const FDialogueSpeakerRow* SourceRow = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!SourceRow)
	{
		AppendLogLine(TEXT("Selected speaker row could not be loaded."));
		return FReply::Handled();
	}

	FName NewRowName(*FString::Printf(TEXT("%s_Copy"), *SelectedSpeakerRowName.ToString()));
	for (int32 Suffix = 1; SpeakerTable->GetRowMap().Contains(NewRowName); ++Suffix)
	{
		NewRowName = FName(*FString::Printf(TEXT("%s_Copy%d"), *SelectedSpeakerRowName.ToString(), Suffix));
	}

	FDialogueSpeakerRow NewRow = *SourceRow;
	NewRow.DisplayName = FText::FromString(NewRow.DisplayName.ToString() + TEXT(" Copy"));

	const UARDialogueSettings* DialogueSettings = GetDefault<UARDialogueSettings>();
	if (DialogueSettings && DialogueSettings->SpeakerDefinitionRootTag.IsValid())
	{
		const FString SpeakerPath = FString::Printf(TEXT("%s.%s"), *DialogueSettings->SpeakerDefinitionRootTag.ToString(), *NewRowName.ToString());
		const FGameplayTag DuplicatedTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*SpeakerPath), false);
		if (DuplicatedTag.IsValid())
		{
			NewRow.SpeakerTag = DuplicatedTag;
		}
	}

	SpeakerTable->Modify();
	SpeakerTable->AddRow(NewRowName, NewRow);
	SpeakerTable->MarkPackageDirty();

	AppendLogLine(FString::Printf(TEXT("Duplicated '%s' to '%s'."), *SelectedSpeakerRowName.ToString(), *NewRowName.ToString()));
	RefreshData();
	SetSelectedSpeakerRow(NewRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleDeleteSpeaker()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	const EAppReturnType::Type Confirm = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString(FString::Printf(TEXT("Delete speaker row '%s'?"), *SelectedSpeakerRowName.ToString())));
	if (Confirm != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	SpeakerTable->Modify();
	SpeakerTable->RemoveRow(SelectedSpeakerRowName);
	SpeakerTable->MarkPackageDirty();
	AppendLogLine(FString::Printf(TEXT("Deleted speaker row '%s'."), *SelectedSpeakerRowName.ToString()));

	SelectedSpeakerRowName = NAME_None;
	RefreshData();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleValidateSpeaker()
{
	ValidationOutput.Empty();

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	FString ParseError;
	if (!CommitEditedSpeakerRow(ParseError))
	{
		AppendLogLine(FString::Printf(TEXT("Save-before-validate failed: %s"), *ParseError));
		return FReply::Handled();
	}

	const FDialogueSpeakerRow* CurrentRow = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!CurrentRow)
	{
		AppendLogLine(TEXT("Speaker row became unavailable after commit."));
		return FReply::Handled();
	}

	FDialogueValidationReport Report;
	const bool bValid = ValidateSpeakerWithBestAvailable(*CurrentRow, Report);
	AppendLogLine(bValid ? TEXT("Speaker validation succeeded.") : TEXT("Speaker validation failed."));

	for (const FDialogueValidationIssue& Issue : Report.Issues)
	{
		const TCHAR* Severity = Issue.Severity == EDialogueValidationSeverity::Error
			? TEXT("ERROR")
			: (Issue.Severity == EDialogueValidationSeverity::Warning ? TEXT("WARN") : TEXT("INFO"));
		AppendLogLine(FString::Printf(TEXT("[%s] %s"), Severity, *Issue.Message.ToString()));
	}

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleSaveSpeaker()
{
	FString Error;
	if (!CommitEditedSpeakerRow(Error))
	{
		AppendLogLine(FString::Printf(TEXT("Save failed: %s"), *Error));
		return FReply::Handled();
	}

	AppendLogLine(FString::Printf(TEXT("Saved speaker row '%s'."), *SelectedSpeakerRowName.ToString()));
	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleCreateConversation()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	FDialogueSpeakerRow* SpeakerRow = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!SpeakerRow || !SpeakerRow->SpeakerTag.IsValid())
	{
		AppendLogLine(TEXT("Selected speaker is invalid. Save/validate the speaker first."));
		return FReply::Handled();
	}

	int32 TargetRelationshipBand = 0;
	if (ConversationListView.IsValid())
	{
		const TArray<TSharedPtr<FConversationEntry>> SelectedConversations = ConversationListView->GetSelectedItems();
		if (!SelectedConversations.IsEmpty() && SelectedConversations[0].IsValid())
		{
			TargetRelationshipBand = FMath::Max(0, SelectedConversations[0]->RelationshipBand);
		}
	}

	UARDialogueSettings* MutableSettings = GetMutableDefault<UARDialogueSettings>();
	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	if (!MutableSettings || !Settings)
	{
		AppendLogLine(TEXT("Dialogue settings are unavailable."));
		return FReply::Handled();
	}

	FString PackageFolder = TEXT("/Game/Data/Dialogue/Conversations");
	if (Settings->ConversationAssets.Num() > 0)
	{
		const FString ExistingLongPackageName = Settings->ConversationAssets[0].ToSoftObjectPath().GetLongPackageName();
		if (!ExistingLongPackageName.IsEmpty())
		{
			PackageFolder = FPackageName::GetLongPackagePath(ExistingLongPackageName);
		}
	}
	FString AssetNameBase = FString::Printf(TEXT("DA_DialogueConv_%s"), *SelectedSpeakerRowName.ToString());
	FString AssetName = AssetNameBase;
	FString PackageName = FString::Printf(TEXT("%s/%s"), *PackageFolder, *AssetName);
	for (int32 Suffix = 1; FPackageName::DoesPackageExist(PackageName); ++Suffix)
	{
		AssetName = FString::Printf(TEXT("%s_%d"), *AssetNameBase, Suffix);
		PackageName = FString::Printf(TEXT("%s/%s"), *PackageFolder, *AssetName);
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		AppendLogLine(FString::Printf(TEXT("Failed to create package '%s'."), *PackageName));
		return FReply::Handled();
	}

	UARDialogueConversationAsset* NewConversation = NewObject<UARDialogueConversationAsset>(
		Package,
		UARDialogueConversationAsset::StaticClass(),
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);
	if (!NewConversation)
	{
		AppendLogLine(TEXT("Failed to create conversation asset object."));
		return FReply::Handled();
	}

	NewConversation->Header.DisplayTitle = FText::FromString(AssetName);
	NewConversation->Header.PrimarySpeakerTag = SpeakerRow->SpeakerTag;
	NewConversation->Header.ParticipatingSpeakerTags.AddUnique(SpeakerRow->SpeakerTag);
	if (TargetRelationshipBand > 0 && SpeakerRow->RelationshipThresholds.IsValidIndex(TargetRelationshipBand - 1))
	{
		NewConversation->Header.MinimumRelationshipPoints = SpeakerRow->RelationshipThresholds[TargetRelationshipBand - 1];
	}

	if (Settings->ConversationDefinitionRootTag.IsValid())
	{
		const FString ConversationTagPath = FString::Printf(TEXT("%s.%s"), *Settings->ConversationDefinitionRootTag.ToString(), *AssetName);
		NewConversation->Header.ConversationTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*ConversationTagPath), false);
	}

	FDialogueCompiledNode& EnterNode = NewConversation->CompiledData.Nodes.AddDefaulted_GetRef();
	EnterNode.NodeId = FGuid::NewGuid();
	EnterNode.NodeType = EDialogueNodeType::Enter;

	FDialogueCompiledNode& CompletedNode = NewConversation->CompiledData.Nodes.AddDefaulted_GetRef();
	CompletedNode.NodeId = FGuid::NewGuid();
	CompletedNode.NodeType = EDialogueNodeType::Completed;

	EnterNode.NextNodeId = CompletedNode.NodeId;
	NewConversation->CompiledData.EnterNodeId = EnterNode.NodeId;

	FDialogueValidationReport Validation;
	const bool bCompiled = ValidateConversationWithBestAvailable(NewConversation, Validation);
	NewConversation->LastCompileValidation = Validation;
	NewConversation->bLastCompileSucceeded = bCompiled;
	NewConversation->CompileVersion = 1;

	FAssetRegistryModule::AssetCreated(NewConversation);
	Package->MarkPackageDirty();
	NewConversation->MarkPackageDirty();

	const TSoftObjectPtr<UARDialogueConversationAsset> NewConversationRef(NewConversation);
	if (!MutableSettings->ConversationAssets.ContainsByPredicate([&NewConversationRef](const TSoftObjectPtr<UARDialogueConversationAsset>& Existing)
	{
		return Existing.ToSoftObjectPath() == NewConversationRef.ToSoftObjectPath();
	}))
	{
		MutableSettings->ConversationAssets.Add(NewConversationRef);
		MutableSettings->SaveConfig();
	}

	AppendLogLine(FString::Printf(TEXT("Created conversation asset '%s' for speaker '%s' (band %d, min rel %.1f)."),
		*PackageName,
		*SpeakerRow->SpeakerTag.ToString(),
		TargetRelationshipBand,
		NewConversation->Header.MinimumRelationshipPoints));

	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
	{
		AssetEditorSubsystem->OpenEditorForAsset(NewConversation);
	}
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("AR_DialogueConversationGraphEditor")));

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleOpenConversation()
{
	if (!ConversationListView.IsValid())
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FConversationEntry>> SelectedItems = ConversationListView->GetSelectedItems();
	if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid() || !SelectedItems[0]->Asset.IsValid())
	{
		AppendLogLine(TEXT("No conversation selected."));
		return FReply::Handled();
	}

	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
	{
		AssetEditorSubsystem->OpenEditorForAsset(SelectedItems[0]->Asset.Get());
	}
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("AR_DialogueConversationGraphEditor")));
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleFindBrokenConversations()
{
	ValidationOutput.Empty();

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	if (!Settings)
	{
		AppendLogLine(TEXT("Dialogue settings are unavailable."));
		return FReply::Handled();
	}

	int32 ErrorConversationCount = 0;
	for (const TSoftObjectPtr<UARDialogueConversationAsset>& ConversationRef : Settings->ConversationAssets)
	{
		UARDialogueConversationAsset* Conversation = ConversationRef.LoadSynchronous();
		if (!Conversation)
		{
			AppendLogLine(FString::Printf(TEXT("ERROR: Failed to load conversation '%s'."), *ConversationRef.ToString()));
			++ErrorConversationCount;
			continue;
		}

		FDialogueValidationReport Report;
		const bool bValid = ValidateConversationWithBestAvailable(Conversation, Report);
		if (bValid && Report.Issues.IsEmpty())
		{
			continue;
		}

		if (Report.HasErrors())
		{
			++ErrorConversationCount;
		}

		AppendLogLine(FString::Printf(TEXT("%s: %s"), Report.HasErrors() ? TEXT("ERROR") : TEXT("WARN"), *Conversation->Header.ConversationTag.ToString()));
		for (const FDialogueValidationIssue& Issue : Report.Issues)
		{
			if (Issue.Severity == EDialogueValidationSeverity::Info)
			{
				continue;
			}
			const TCHAR* Severity = Issue.Severity == EDialogueValidationSeverity::Error ? TEXT("ERROR") : TEXT("WARN");
			AppendLogLine(FString::Printf(TEXT("  - [%s] %s"), Severity, *Issue.Message.ToString()));
		}
	}

	if (ErrorConversationCount == 0)
	{
		AppendLogLine(TEXT("No broken conversations found."));
	}
	return FReply::Handled();
}
FReply SDialogueSpeakerEditorPanel::HandleSortByName()
{
	SetSortMode(ESpeakerSortMode::DisplayName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleSortByTag()
{
	SetSortMode(ESpeakerSortMode::SpeakerTag);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleSortByConversationCount()
{
	SetSortMode(ESpeakerSortMode::ConversationCount);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleResetThresholds()
{
	if (ThresholdsTextBox.IsValid())
	{
		ThresholdsTextBox->SetText(FText::FromString(TEXT("50, 150, 300, 500")));
	}
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleAddPortrait()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	FDialogueSpeakerRow* Row = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!Row)
	{
		AppendLogLine(TEXT("Selected speaker row could not be loaded."));
		return FReply::Handled();
	}

	const FString PortraitTagText = PortraitTagTextBox.IsValid() ? PortraitTagTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (PortraitTagText.IsEmpty())
	{
		AppendLogLine(TEXT("Portrait tag is required."));
		return FReply::Handled();
	}

	const FGameplayTag PortraitTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*PortraitTagText), false);
	if (!PortraitTag.IsValid())
	{
		AppendLogLine(FString::Printf(TEXT("Portrait tag '%s' is invalid."), *PortraitTagText));
		return FReply::Handled();
	}

	FSpeakerPortraitEntry NewPortrait;
	NewPortrait.PortraitTag = PortraitTag;

	const FString TexturePath = PortraitTexturePathTextBox.IsValid() ? PortraitTexturePathTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!TexturePath.IsEmpty())
	{
		NewPortrait.Portrait.PortraitTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TexturePath));
	}

	SpeakerTable->Modify();
	if (SelectedPortraitIndex != INDEX_NONE && Row->Portraits.IsValidIndex(SelectedPortraitIndex))
	{
		Row->Portraits[SelectedPortraitIndex] = NewPortrait;
		AppendLogLine(FString::Printf(TEXT("Updated portrait index %d."), SelectedPortraitIndex));
	}
	else
	{
		Row->Portraits.Add(NewPortrait);
		AppendLogLine(TEXT("Added new portrait entry."));
	}
	SpeakerTable->MarkPackageDirty();

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleRemovePortrait()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	FDialogueSpeakerRow* Row = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!Row)
	{
		AppendLogLine(TEXT("Selected speaker row could not be loaded."));
		return FReply::Handled();
	}

	if (SelectedPortraitIndex == INDEX_NONE || !Row->Portraits.IsValidIndex(SelectedPortraitIndex))
	{
		AppendLogLine(TEXT("No portrait selected."));
		return FReply::Handled();
	}

	SpeakerTable->Modify();
	Row->Portraits.RemoveAt(SelectedPortraitIndex);
	SpeakerTable->MarkPackageDirty();
	AppendLogLine(FString::Printf(TEXT("Removed portrait index %d."), SelectedPortraitIndex));

	SelectedPortraitIndex = INDEX_NONE;
	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	return FReply::Handled();
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGenerateSpeakerRow(TSharedPtr<FSpeakerEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SDialogueSpeakerTableRow, OwnerTable).Item(Item);
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGenerateConversationRow(TSharedPtr<FConversationEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString Label = Item.IsValid()
		? FString::Printf(TEXT("%s | %s"), *Item->RelationshipBandLabel, *Item->Label)
		: FString(TEXT("<Invalid Conversation Entry>"));

	return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromString(Label)).AutoWrapText(true)
	];
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGeneratePortraitRow(TSharedPtr<FPortraitEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FPortraitEntry>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Label : FString(TEXT("<Invalid Portrait Entry>")))).AutoWrapText(true)
	];
}

void SDialogueSpeakerEditorPanel::OnSpeakerSelectionChanged(TSharedPtr<FSpeakerEntry> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (!Item.IsValid())
	{
		return;
	}
	SetSelectedSpeakerRow(Item->RowName);
}

void SDialogueSpeakerEditorPanel::OnConversationDoubleClicked(TSharedPtr<FConversationEntry> Item)
{
	if (!Item.IsValid() || !Item->Asset.IsValid())
	{
		return;
	}

	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
	{
		AssetEditorSubsystem->OpenEditorForAsset(Item->Asset.Get());
	}
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("AR_DialogueConversationGraphEditor")));
}

void SDialogueSpeakerEditorPanel::OnPortraitSelectionChanged(TSharedPtr<FPortraitEntry> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (!Item.IsValid())
	{
		SelectedPortraitIndex = INDEX_NONE;
		return;
	}

	SelectedPortraitIndex = Item->PortraitIndex;

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		return;
	}
	const FDialogueSpeakerRow* Row = SpeakerTable->FindRow<FDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!Row || !Row->Portraits.IsValidIndex(SelectedPortraitIndex))
	{
		return;
	}

	const FSpeakerPortraitEntry& Portrait = Row->Portraits[SelectedPortraitIndex];
	if (PortraitTagTextBox.IsValid())
	{
		PortraitTagTextBox->SetText(FText::FromString(Portrait.PortraitTag.ToString()));
	}
	if (PortraitTexturePathTextBox.IsValid())
	{
		PortraitTexturePathTextBox->SetText(FText::FromString(Portrait.Portrait.PortraitTexture.ToString()));
	}
}
