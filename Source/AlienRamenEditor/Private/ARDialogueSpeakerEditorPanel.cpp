#include "ARDialogueSpeakerEditorPanel.h"

#include "ARDialogueConversationAsset.h"
#include "ARDialogueConversationGraphEditorPanel.h"
#include "ARDialogueSettings.h"
#include "ARDialogueSubsystem.h"
#include "TagContentResolverSubsystem.h"
#include "TagContentResolverEditorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Framework/Docking/TabManager.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "PropertyCustomizationHelpers.h"
#include "SGameplayTagCombo.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
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

	static const TCHAR* DialogueAutoTagConfigRelativePath = TEXT("Tags/DialogueConversationGeneratedTags.ini");

	static FString SanitizeTagSegment(const FString& RawSegment)
	{
		FString Sanitized;
		Sanitized.Reserve(RawSegment.Len());

		bool bPreviousWasUnderscore = false;
		for (const TCHAR Char : RawSegment)
		{
			const bool bAllowed = FChar::IsAlnum(Char) || Char == TCHAR('_');
			const TCHAR OutChar = bAllowed ? Char : TCHAR('_');
			if (OutChar == TCHAR('_') && bPreviousWasUnderscore)
			{
				continue;
			}

			Sanitized.AppendChar(OutChar);
			bPreviousWasUnderscore = (OutChar == TCHAR('_'));
		}

		Sanitized.TrimStartAndEndInline();
		Sanitized.RemoveFromStart(TEXT("_"));
		Sanitized.RemoveFromEnd(TEXT("_"));
		return Sanitized.IsEmpty() ? TEXT("Speaker") : Sanitized;
	}

	static FString GetSpeakerLeafSegment(const FGameplayTag SpeakerTag)
	{
		const FString TagString = SpeakerTag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (TagString.FindLastChar(TEXT('.'), DotIndex) && DotIndex + 1 < TagString.Len())
		{
			return SanitizeTagSegment(TagString.Mid(DotIndex + 1));
		}

		return SanitizeTagSegment(TagString);
	}

	static bool EnsureTagLineInConfig(
		const FString& ConfigPath,
		const FString& SectionHeader,
		const FString& TagPath,
		const FString& DevComment,
		const bool bUsePlusPrefix,
		FString& OutError)
	{
		OutError.Empty();
		FString ConfigText;
		const bool bConfigExists = FPaths::FileExists(ConfigPath);
		if (bConfigExists && !FFileHelper::LoadFileToString(ConfigText, *ConfigPath))
		{
			OutError = FString::Printf(TEXT("Failed to load gameplay tag config '%s'."), *ConfigPath);
			return false;
		}

		if (!bConfigExists)
		{
			const FString ConfigDir = FPaths::GetPath(ConfigPath);
			IFileManager::Get().MakeDirectory(*ConfigDir, true);
			ConfigText = SectionHeader;
			ConfigText += LINE_TERMINATOR;
		}
		else if (!ConfigText.Contains(SectionHeader, ESearchCase::IgnoreCase))
		{
			if (!ConfigText.EndsWith(TEXT("\n")) && !ConfigText.EndsWith(TEXT("\r\n")))
			{
				ConfigText += LINE_TERMINATOR;
			}
			ConfigText += SectionHeader;
			ConfigText += LINE_TERMINATOR;
		}

		const FString TagNeedle = FString::Printf(TEXT("Tag=\"%s\""), *TagPath);
		if (ConfigText.Contains(TagNeedle, ESearchCase::CaseSensitive))
		{
			return true;
		}

		FString SafeComment = DevComment;
		SafeComment.ReplaceInline(TEXT("\""), TEXT("\\\""));
		const TCHAR* Prefix = bUsePlusPrefix ? TEXT("+") : TEXT("");
		const FString NewLine = FString::Printf(TEXT("%sGameplayTagList=(Tag=\"%s\",DevComment=\"%s\")"), Prefix, *TagPath, *SafeComment);
		if (!ConfigText.EndsWith(TEXT("\n")) && !ConfigText.EndsWith(TEXT("\r\n")))
		{
			ConfigText += LINE_TERMINATOR;
		}
		ConfigText += NewLine;
		ConfigText += LINE_TERMINATOR;

		if (!FFileHelper::SaveStringToFile(ConfigText, *ConfigPath))
		{
			OutError = FString::Printf(TEXT("Failed to write gameplay tag config '%s'."), *ConfigPath);
			return false;
		}

		return true;
	}

	static FGameplayTag EnsureGameplayTagRegistered(const FString& TagPath, const FString& DevComment, FString& OutError)
	{
		OutError.Empty();
		if (TagPath.IsEmpty())
		{
			OutError = TEXT("Cannot register an empty gameplay tag path.");
			return FGameplayTag();
		}

		const FName TagName(*TagPath);
		FGameplayTag Existing = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);
		if (Existing.IsValid())
		{
			return Existing;
		}

		const FString GeneratedConfigPath = FPaths::ProjectConfigDir() / DialogueAutoTagConfigRelativePath;
		const FString GeneratedSection = TEXT("[/Script/GameplayTags.GameplayTagsList]");
		FString WriteError;
		if (!EnsureTagLineInConfig(
			GeneratedConfigPath,
			GeneratedSection,
			TagPath,
			DevComment,
			false,
			WriteError))
		{
			OutError = WriteError;
			return FGameplayTag();
		}

		UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
		Existing = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);
		if (Existing.IsValid())
		{
			return Existing;
		}

		const FString FallbackConfigPath = FPaths::ProjectConfigDir() / TEXT("DefaultGameplayTags.ini");
		const FString FallbackSection = TEXT("[/Script/GameplayTags.GameplayTagsSettings]");
		if (!EnsureTagLineInConfig(
			FallbackConfigPath,
			FallbackSection,
			TagPath,
			DevComment,
			true,
			WriteError))
		{
			OutError = WriteError;
			return FGameplayTag();
		}

		UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
		Existing = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);
		if (!Existing.IsValid())
		{
			OutError = FString::Printf(TEXT("Gameplay tag '%s' is still invalid after config update/refresh."), *TagPath);
		}
		return Existing;
	}

	static void GatherConversationAssetsFromLookup(const UARDialogueSettings* DialogueSettings, TMap<FGameplayTag, UARDialogueConversationAsset*>& OutConversationsByTag)
	{
		OutConversationsByTag.Reset();
		if (!DialogueSettings || !DialogueSettings->ConversationDefinitionRootTag.IsValid())
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

		UDataTable* ConversationTable = nullptr;
		FString LookupError;
		if (FTagContentResolverEditorHelpers::TryResolveDataTableForRootTag(DialogueSettings->ConversationDefinitionRootTag, ConversationTable, LookupError)
			&& ConversationTable
			&& ConversationTable->GetRowStruct() == FARDialogueConversationAssetRow::StaticStruct())
		{
			for (const FName RowName : ConversationTable->GetRowNames())
			{
				const FARDialogueConversationAssetRow* Row = ConversationTable->FindRow<FARDialogueConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorConversations"), false);
				if (!Row)
				{
					continue;
				}

				FGameplayTag RowTag = Row->ConversationTag;
				if (!RowTag.IsValid())
				{
					const FString RowTagPath = DialogueSettings->ConversationDefinitionRootTag.ToString() + TEXT(".") + RowName.ToString();
					FString EnsureTagError;
					RowTag = EnsureGameplayTagRegistered(
						RowTagPath,
						FString::Printf(TEXT("Auto-generated conversation tag for row '%s'."), *RowName.ToString()),
						EnsureTagError);
					if (!RowTag.IsValid())
					{
						continue;
					}

					FARDialogueConversationAssetRow* MutableRow = ConversationTable->FindRow<FARDialogueConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorConversations"), false);
					if (MutableRow && !MutableRow->ConversationTag.IsValid())
					{
						ConversationTable->Modify();
						MutableRow->ConversationTag = RowTag;
						ConversationTable->MarkPackageDirty();
					}
				}

				UARDialogueConversationAsset* Conversation = Row->Conversation.LoadSynchronous();
				if (Conversation && !Conversation->Header.ConversationTag.IsValid() && RowTag.IsValid())
				{
					Conversation->Modify();
					Conversation->Header.ConversationTag = RowTag;
					Conversation->MarkPackageDirty();
				}

				TryAddConversation(Conversation, RowTag);
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

		return FString::Printf(
			TEXT("%s (%s)"),
			*Header.DisplayTitle.ToString(),
			*Header.ConversationTag.ToString());
	}

	static bool IsConditionTagSource(const EDialogueConditionSource Source)
	{
		switch (Source)
		{
		case EDialogueConditionSource::CombinedTags:
		case EDialogueConditionSource::PlayerTags:
		case EDialogueConditionSource::GameTags:
		case EDialogueConditionSource::TransientConversationTags:
		case EDialogueConditionSource::ActiveCharacter:
		case EDialogueConditionSource::Loadout:
			return true;
		default:
			return false;
		}
	}

	static bool DoesOperatorRequireTagPresence(const EDialogueComparisonOp Op)
	{
		switch (Op)
		{
		case EDialogueComparisonOp::Contains:
		case EDialogueComparisonOp::Equals:
		case EDialogueComparisonOp::Present:
			return true;
		default:
			return false;
		}
	}

	static void GatherConversationChainTags(
		const UARDialogueConversationAsset* Conversation,
		TSet<FGameplayTag>& OutRequiredTags,
		TSet<FGameplayTag>& OutAddedTags,
		TSet<FGameplayTag>& OutRemovedTags)
	{
		OutRequiredTags.Reset();
		OutAddedTags.Reset();
		OutRemovedTags.Reset();
		if (!Conversation)
		{
			return;
		}

		const auto GatherRequiredTagsFromGroup = [&OutRequiredTags](const FDialogueConditionGroup& Group)
		{
			for (const FDialogueCondition& Condition : Group.Conditions)
			{
				if (IsConditionTagSource(Condition.Source)
					&& DoesOperatorRequireTagPresence(Condition.Operator)
					&& Condition.TagValue.IsValid())
				{
					OutRequiredTags.Add(Condition.TagValue);
				}
			}
		};

		GatherRequiredTagsFromGroup(Conversation->Header.LockedConditions);
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
				if (!Mutation.Tag.IsValid())
				{
					continue;
				}

				if (Mutation.Operation == EDialogueTagMutationOp::Add)
				{
					OutAddedTags.Add(Mutation.Tag);
				}
				else
				{
					OutRemovedTags.Add(Mutation.Tag);
				}
			}
		}
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
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Sort Name"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleSortByName)
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
							+ SHeaderRow::Column(TEXT("DisplayName")).DefaultLabel(FText::FromString(TEXT("Display Name"))).FillWidth(0.30f)
							+ SHeaderRow::Column(TEXT("SpeakerTag")).DefaultLabel(FText::FromString(TEXT("Speaker Tag"))).FillWidth(0.34f)
							+ SHeaderRow::Column(TEXT("Thresholds")).DefaultLabel(FText::FromString(TEXT("Relationship Thresholds"))).FillWidth(0.26f)
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
						SNew(STextBlock).Text(FText::FromString(TEXT("Speaker Tag")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(SGameplayTagCombo)
						.Filter(TEXT("Dialogue.Speaker"))
						.Tag(this, &SDialogueSpeakerEditorPanel::GetEditedSpeakerTag)
						.OnTagChanged(this, &SDialogueSpeakerEditorPanel::OnEditedSpeakerTagChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Faction Tag (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SGameplayTagCombo)
						.Tag(this, &SDialogueSpeakerEditorPanel::GetEditedFactionTag)
						.OnTagChanged(this, &SDialogueSpeakerEditorPanel::OnEditedFactionTagChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Default Portrait Texture (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SAssignNew(DefaultPortraitTexturePicker, SObjectPropertyEntryBox)
						.AllowedClass(UTexture2D::StaticClass())
						.ObjectPath(this, &SDialogueSpeakerEditorPanel::GetEditedDefaultPortraitTexturePath)
						.OnObjectChanged(this, &SDialogueSpeakerEditorPanel::OnEditedDefaultPortraitTextureChanged)
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
						SNew(STextBlock).Text(FText::FromString(TEXT("Portrait Tag")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(SGameplayTagCombo)
						.Filter(TEXT("Dialogue.Speaker"))
						.Tag(this, &SDialogueSpeakerEditorPanel::GetEditedPortraitTag)
						.OnTagChanged(this, &SDialogueSpeakerEditorPanel::OnEditedPortraitTagChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Portrait Texture (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(PortraitTexturePicker, SObjectPropertyEntryBox)
						.AllowedClass(UTexture2D::StaticClass())
						.ObjectPath(this, &SDialogueSpeakerEditorPanel::GetEditedPortraitTexturePath)
						.OnObjectChanged(this, &SDialogueSpeakerEditorPanel::OnEditedPortraitTextureChanged)
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
					+ SVerticalBox::Slot().AutoHeight().MaxHeight(110.0f).Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(ThresholdListView, SListView<TSharedPtr<FThresholdEntry>>)
						.ListItemsSource(&ThresholdEntries)
						.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGenerateThresholdRow)
						.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnThresholdSelectionChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SSpinBox<float>)
						.MinValue(-100000.0f)
						.MaxValue(100000.0f)
						.IsEnabled_Lambda([this]() { return SelectedThresholdIndex != INDEX_NONE && EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex); })
						.Value_Lambda([this]()
						{
							return EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex)
								? EditedRelationshipThresholds[SelectedThresholdIndex]
								: 0.0f;
						})
						.OnValueCommitted_Lambda([this](const float NewValue, ETextCommit::Type)
						{
							SetEditedThresholdValue(NewValue);
						})
						.OnValueChanged_Lambda([this](const float NewValue)
						{
							SetEditedThresholdValue(NewValue);
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Add"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleAddThreshold)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Remove"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleRemoveThreshold)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Move Up"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleMoveThresholdUp)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Move Down"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleMoveThresholdDown)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).Text(FText::FromString(TEXT("Reset Defaults"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleResetThresholds)
						]
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

	if (!FTagContentResolverEditorHelpers::TryResolveDataTableForRootTag(DialogueSettings->SpeakerDefinitionRootTag, OutTable, OutError))
	{
		return false;
	}

	if (OutTable->GetRowStruct() != FARDialogueSpeakerRow::StaticStruct())
	{
		OutError = FString::Printf(TEXT("Speaker data table row struct mismatch. Expected '%s', got '%s'."),
			*FARDialogueSpeakerRow::StaticStruct()->GetName(),
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
		TMap<FGameplayTag, UARDialogueConversationAsset*> ConversationsByTag;
		GatherConversationAssetsFromLookup(DialogueSettings, ConversationsByTag);
		for (const TPair<FGameplayTag, UARDialogueConversationAsset*>& Pair : ConversationsByTag)
		{
			UARDialogueConversationAsset* Conversation = Pair.Value;
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
		const FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(RowName, TEXT("DialogueSpeakerEditor"), false);
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

	const TArray<float>& ThresholdsForBandCalc = EditedRelationshipThresholds.IsEmpty()
		? SelectedSpeaker->Row.RelationshipThresholds
		: EditedRelationshipThresholds;

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
	GatherConversationAssetsFromLookup(Settings, ConversationsByTag);

	struct FConversationChainData
	{
		TSet<FGameplayTag> RequiredTags;
		TSet<FGameplayTag> AddedTags;
		TSet<FGameplayTag> RemovedTags;
	};

	TArray<TSharedPtr<FConversationEntry>> RawConversationEntries;
	TMap<const UARDialogueConversationAsset*, FConversationChainData> ChainDataByConversation;
	for (const TPair<FGameplayTag, UARDialogueConversationAsset*>& Pair : ConversationsByTag)
	{
		UARDialogueConversationAsset* Conversation = Pair.Value;
		if (!Conversation || !Conversation->Header.PrimarySpeakerTag.MatchesTagExact(SelectedSpeaker->Row.SpeakerTag))
		{
			continue;
		}

		TSharedPtr<FConversationEntry> Entry = MakeShared<FConversationEntry>();
		Entry->Asset = Conversation;
		Entry->DisplayTitle = Conversation->Header.DisplayTitle.ToString();
		Entry->ConversationTag = Conversation->Header.ConversationTag;
		Entry->PrimarySpeakerTag = Conversation->Header.PrimarySpeakerTag;
		Entry->Priority = Conversation->Header.Priority;
		Entry->MinimumRelationshipPoints = Conversation->Header.MinimumRelationshipPoints;
		Entry->bImportant = Conversation->Header.bImportant;
		Entry->bRepeatable = Conversation->Header.bRepeatable;
		Entry->RelationshipBand = ComputeRelationshipBand(Entry->MinimumRelationshipPoints, ThresholdsForBandCalc);
		Entry->RelationshipBandLabel = BuildRelationshipBandLabel(Entry->RelationshipBand, ThresholdsForBandCalc);
		Entry->Label = BuildConversationEntryLabel(Conversation);
		Entry->GateSummary = FString::Printf(
			TEXT("Gate L:%d B:%d | Reoffer SG:%s SP:%s CG:%s"),
			Conversation->Header.LockedConditions.Conditions.Num(),
			Conversation->Header.BlockedConditions.Conditions.Num(),
			Conversation->Header.bSeenByGameBlocksReoffer ? TEXT("Y") : TEXT("N"),
			Conversation->Header.bSeenByPlayerBlocksReoffer ? TEXT("Y") : TEXT("N"),
			Conversation->Header.bCompletedByGameBlocksReoffer ? TEXT("Y") : TEXT("N"));

		FConversationChainData ChainData;
		GatherConversationChainTags(
			Conversation,
			ChainData.RequiredTags,
			ChainData.AddedTags,
			ChainData.RemovedTags);
		Entry->MutationSummary = FString::Printf(
			TEXT("Tags +%d / -%d | RequiresTags:%d"),
			ChainData.AddedTags.Num(),
			ChainData.RemovedTags.Num(),
			ChainData.RequiredTags.Num());

		RawConversationEntries.Add(Entry);
		ChainDataByConversation.Add(Conversation, MoveTemp(ChainData));
	}

	for (const TSharedPtr<FConversationEntry>& Entry : RawConversationEntries)
	{
		if (!Entry.IsValid() || !Entry->Asset.IsValid())
		{
			continue;
		}

		const FConversationChainData* SourceData = ChainDataByConversation.Find(Entry->Asset.Get());
		if (!SourceData)
		{
			continue;
		}

		TArray<FString> UnlocksConversationTags;
		TArray<FString> RequiresFromConversationTags;
		for (const TSharedPtr<FConversationEntry>& OtherEntry : RawConversationEntries)
		{
			if (!OtherEntry.IsValid() || !OtherEntry->Asset.IsValid() || OtherEntry == Entry)
			{
				continue;
			}

			const FConversationChainData* OtherData = ChainDataByConversation.Find(OtherEntry->Asset.Get());
			if (!OtherData)
			{
				continue;
			}

			bool bUnlocksOther = false;
			for (const FGameplayTag AddedTag : SourceData->AddedTags)
			{
				if (OtherData->RequiredTags.Contains(AddedTag))
				{
					bUnlocksOther = true;
					break;
				}
			}
			if (bUnlocksOther)
			{
				UnlocksConversationTags.AddUnique(OtherEntry->ConversationTag.ToString());
			}

			bool bRequiresFromOther = false;
			for (const FGameplayTag RequiredTag : SourceData->RequiredTags)
			{
				if (OtherData->AddedTags.Contains(RequiredTag))
				{
					bRequiresFromOther = true;
					break;
				}
			}
			if (bRequiresFromOther)
			{
				RequiresFromConversationTags.AddUnique(OtherEntry->ConversationTag.ToString());
			}
		}

		UnlocksConversationTags.Sort();
		RequiresFromConversationTags.Sort();
		const FString UnlocksSummary = UnlocksConversationTags.IsEmpty()
			? TEXT("-")
			: FString::Join(UnlocksConversationTags, TEXT(", "));
		const FString RequiresSummary = RequiresFromConversationTags.IsEmpty()
			? TEXT("-")
			: FString::Join(RequiresFromConversationTags, TEXT(", "));
		Entry->ChainSummary = FString::Printf(
			TEXT("Chain unlocks -> %s | requires <- %s"),
			*UnlocksSummary,
			*RequiresSummary);
	}

	RawConversationEntries.Sort([](const TSharedPtr<FConversationEntry>& Lhs, const TSharedPtr<FConversationEntry>& Rhs)
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

	ConversationEntries.Reset();
	int32 CurrentBand = INDEX_NONE;
	int32 CurrentBandCount = 0;
	TMap<int32, int32> CountByBand;
	for (const TSharedPtr<FConversationEntry>& Entry : RawConversationEntries)
	{
		if (Entry.IsValid())
		{
			CountByBand.FindOrAdd(Entry->RelationshipBand) += 1;
		}
	}

	for (const TSharedPtr<FConversationEntry>& Entry : RawConversationEntries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		if (Entry->RelationshipBand != CurrentBand)
		{
			CurrentBand = Entry->RelationshipBand;
			CurrentBandCount = CountByBand.FindRef(CurrentBand);

			TSharedPtr<FConversationEntry> BandHeader = MakeShared<FConversationEntry>();
			BandHeader->bIsBandHeader = true;
			BandHeader->RelationshipBand = CurrentBand;
			BandHeader->RelationshipBandLabel = BuildRelationshipBandLabel(CurrentBand, ThresholdsForBandCalc);
			BandHeader->DisplayTitle = FString::Printf(TEXT("%s (%d conversations)"), *BandHeader->RelationshipBandLabel, CurrentBandCount);
			ConversationEntries.Add(BandHeader);
		}

		ConversationEntries.Add(Entry);
	}

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

	const FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

bool SDialogueSpeakerEditorPanel::ValidateSpeakerWithBestAvailable(const FARDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const
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

bool SDialogueSpeakerEditorPanel::CommitEditedSpeakerRow(FString& OutError)
{
	OutError.Empty();

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		OutError = TEXT("No speaker selected.");
		return false;
	}

	FARDialogueSpeakerRow* MutableRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!MutableRow)
	{
		OutError = TEXT("Selected speaker row could not be resolved in table.");
		return false;
	}

	const FString DisplayNameText = DisplayNameTextBox.IsValid() ? DisplayNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString DescriptionText = DescriptionTextBox.IsValid() ? DescriptionTextBox->GetText().ToString() : FString();

	if (DisplayNameText.IsEmpty())
	{
		OutError = TEXT("Display Name is required.");
		return false;
	}
	if (!EditedSpeakerTag.IsValid())
	{
		OutError = TEXT("Speaker Tag is required.");
		return false;
	}

	if (EditedRelationshipThresholds.IsEmpty())
	{
		OutError = TEXT("Relationship thresholds must not be empty.");
		return false;
	}

	float LastThresholdValue = -FLT_MAX;
	for (const float ThresholdValue : EditedRelationshipThresholds)
	{
		if (ThresholdValue <= LastThresholdValue)
		{
			OutError = TEXT("Relationship thresholds must be strictly ascending.");
			return false;
		}
		LastThresholdValue = ThresholdValue;
	}

	SpeakerTable->Modify();
	MutableRow->DisplayName = FText::FromString(DisplayNameText);
	MutableRow->Description = FText::FromString(DescriptionText);
	MutableRow->SpeakerTag = EditedSpeakerTag;
	MutableRow->FactionTag = EditedFactionTag;
	MutableRow->DefaultPortrait.PortraitTexture = EditedDefaultPortraitTexture;
	MutableRow->RelationshipThresholds = EditedRelationshipThresholds;
	SpeakerTable->MarkPackageDirty();
	return true;
}

FGameplayTag SDialogueSpeakerEditorPanel::GetEditedSpeakerTag() const
{
	return EditedSpeakerTag;
}

void SDialogueSpeakerEditorPanel::OnEditedSpeakerTagChanged(FGameplayTag NewTag)
{
	EditedSpeakerTag = NewTag;
}

FGameplayTag SDialogueSpeakerEditorPanel::GetEditedFactionTag() const
{
	return EditedFactionTag;
}

void SDialogueSpeakerEditorPanel::OnEditedFactionTagChanged(FGameplayTag NewTag)
{
	EditedFactionTag = NewTag;
}

FString SDialogueSpeakerEditorPanel::GetEditedDefaultPortraitTexturePath() const
{
	return EditedDefaultPortraitTexture.ToSoftObjectPath().ToString();
}

void SDialogueSpeakerEditorPanel::OnEditedDefaultPortraitTextureChanged(const FAssetData& AssetData)
{
	EditedDefaultPortraitTexture = AssetData.IsValid()
		? TSoftObjectPtr<UTexture2D>(AssetData.ToSoftObjectPath())
		: TSoftObjectPtr<UTexture2D>();
}

FGameplayTag SDialogueSpeakerEditorPanel::GetEditedPortraitTag() const
{
	return EditedPortraitTag;
}

void SDialogueSpeakerEditorPanel::OnEditedPortraitTagChanged(FGameplayTag NewTag)
{
	EditedPortraitTag = NewTag;
}

FString SDialogueSpeakerEditorPanel::GetEditedPortraitTexturePath() const
{
	return EditedPortraitTexture.ToSoftObjectPath().ToString();
}

void SDialogueSpeakerEditorPanel::OnEditedPortraitTextureChanged(const FAssetData& AssetData)
{
	EditedPortraitTexture = AssetData.IsValid()
		? TSoftObjectPtr<UTexture2D>(AssetData.ToSoftObjectPath())
		: TSoftObjectPtr<UTexture2D>();
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
	FARDialogueSpeakerRow SelectedRow;
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
		EditedSpeakerTag = FGameplayTag();
		EditedFactionTag = FGameplayTag();
		EditedDefaultPortraitTexture = TSoftObjectPtr<UTexture2D>();
		EditedPortraitTag = FGameplayTag();
		EditedPortraitTexture = TSoftObjectPtr<UTexture2D>();
		EditedRelationshipThresholds.Reset();
		SelectedThresholdIndex = INDEX_NONE;
		RefreshThresholdList();
		RefreshPortraitList();
		RefreshConversationMap();
		return;
	}

	if (DisplayNameTextBox.IsValid()) { DisplayNameTextBox->SetText(SelectedRow.DisplayName); }
	if (DescriptionTextBox.IsValid()) { DescriptionTextBox->SetText(SelectedRow.Description); }
	EditedSpeakerTag = SelectedRow.SpeakerTag;
	EditedFactionTag = SelectedRow.FactionTag;
	EditedDefaultPortraitTexture = SelectedRow.DefaultPortrait.PortraitTexture;
	EditedPortraitTag = FGameplayTag();
	EditedPortraitTexture = TSoftObjectPtr<UTexture2D>();
	EditedRelationshipThresholds = SelectedRow.RelationshipThresholds;
	if (EditedRelationshipThresholds.IsEmpty())
	{
		EditedRelationshipThresholds = { 50.0f, 150.0f, 300.0f, 500.0f };
	}
	SelectedThresholdIndex = EditedRelationshipThresholds.IsEmpty() ? INDEX_NONE : 0;
	RefreshThresholdList();

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
		const TGuardValue<bool> IgnoreSelectionChangedGuard(bIgnoreSpeakerSelectionChanged, true);
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

void SDialogueSpeakerEditorPanel::RefreshThresholdList()
{
	ThresholdEntries.Reset();
	for (int32 ThresholdIndex = 0; ThresholdIndex < EditedRelationshipThresholds.Num(); ++ThresholdIndex)
	{
		TSharedPtr<FThresholdEntry> Entry = MakeShared<FThresholdEntry>();
		Entry->ThresholdIndex = ThresholdIndex;
		Entry->Value = EditedRelationshipThresholds[ThresholdIndex];
		ThresholdEntries.Add(Entry);
	}

	if (ThresholdListView.IsValid())
	{
		ThresholdListView->RequestListRefresh();
		ThresholdListView->ClearSelection();
		if (ThresholdEntries.IsValidIndex(SelectedThresholdIndex))
		{
			ThresholdListView->SetSelection(ThresholdEntries[SelectedThresholdIndex]);
		}
	}
}

void SDialogueSpeakerEditorPanel::SetEditedThresholdValue(float NewValue)
{
	if (!EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex))
	{
		return;
	}

	if (FMath::IsNearlyEqual(EditedRelationshipThresholds[SelectedThresholdIndex], NewValue))
	{
		return;
	}

	EditedRelationshipThresholds[SelectedThresholdIndex] = NewValue;
	RefreshThresholdList();
	RefreshConversationMap();
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

	FARDialogueSpeakerRow NewRow;
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

	const FARDialogueSpeakerRow* SourceRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	FARDialogueSpeakerRow NewRow = *SourceRow;
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

	const FARDialogueSpeakerRow* CurrentRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	FARDialogueSpeakerRow* SpeakerRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	const UARDialogueSettings* Settings = GetDefault<UARDialogueSettings>();
	if (!Settings || !Settings->ConversationDefinitionRootTag.IsValid())
	{
		AppendLogLine(TEXT("Dialogue settings/content lookup configuration are unavailable."));
		return FReply::Handled();
	}

	UDataTable* ConversationLookupTable = nullptr;
	FString LookupError;
	if (!FTagContentResolverEditorHelpers::TryResolveDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationLookupTable, LookupError))
	{
		AppendLogLine(LookupError);
		return FReply::Handled();
	}

	if (ConversationLookupTable->GetRowStruct() != FARDialogueConversationAssetRow::StaticStruct())
	{
		AppendLogLine(FString::Printf(TEXT("Conversation lookup table row struct mismatch. Expected '%s', got '%s'."),
			*FARDialogueConversationAssetRow::StaticStruct()->GetName(),
			*GetNameSafe(ConversationLookupTable->GetRowStruct())));
		return FReply::Handled();
	}

	FString PackageFolder = TEXT("/Game/Data/Dialogue/Conversations");
	const FString ConversationTablePackageName = FPackageName::ObjectPathToPackageName(ConversationLookupTable->GetPathName());
	if (!ConversationTablePackageName.IsEmpty())
	{
		PackageFolder = FPackageName::GetLongPackagePath(ConversationTablePackageName);
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

	FName LookupRowName(*AssetName);
	for (int32 Suffix = 1; ConversationLookupTable->GetRowMap().Contains(LookupRowName); ++Suffix)
	{
		LookupRowName = FName(*FString::Printf(TEXT("%s_%d"), *AssetName, Suffix));
	}

	TSet<FGameplayTag> UsedConversationTags;
	for (const FName ExistingRowName : ConversationLookupTable->GetRowNames())
	{
		const FARDialogueConversationAssetRow* ExistingRow = ConversationLookupTable->FindRow<FARDialogueConversationAssetRow>(
			ExistingRowName,
			TEXT("DialogueSpeakerEditorConversationTagScan"),
			false);
		if (!ExistingRow)
		{
			continue;
		}

		if (ExistingRow->ConversationTag.IsValid())
		{
			UsedConversationTags.Add(ExistingRow->ConversationTag);
		}

		if (UARDialogueConversationAsset* ExistingConversation = ExistingRow->Conversation.LoadSynchronous())
		{
			if (ExistingConversation->Header.ConversationTag.IsValid())
			{
				UsedConversationTags.Add(ExistingConversation->Header.ConversationTag);
			}
		}
	}

	const FString SpeakerSegment = GetSpeakerLeafSegment(SpeakerRow->SpeakerTag);
	const FString ConversationTagPrefix = FString::Printf(
		TEXT("%s.Id.%s"),
		*Settings->ConversationDefinitionRootTag.ToString(),
		*SpeakerSegment);

	FString EnsureTagError;
	FGameplayTag ConversationTag;
	for (int32 Index = 1; Index < 100000; ++Index)
	{
		const FString CandidateTagPath = FString::Printf(TEXT("%s.%d"), *ConversationTagPrefix, Index);
		const FGameplayTag CandidateTag = EnsureGameplayTagRegistered(
			CandidateTagPath,
			FString::Printf(TEXT("Auto-created conversation tag for speaker '%s'."), *SpeakerRow->SpeakerTag.ToString()),
			EnsureTagError);
		if (!CandidateTag.IsValid())
		{
			break;
		}

		if (!UsedConversationTags.Contains(CandidateTag))
		{
			ConversationTag = CandidateTag;
			break;
		}
	}

	if (!ConversationTag.IsValid())
	{
		AppendLogLine(FString::Printf(TEXT("Failed to create auto conversation tag under '%s': %s"), *ConversationTagPrefix, *EnsureTagError));
		return FReply::Handled();
	}
	NewConversation->Header.ConversationTag = ConversationTag;

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

	FARDialogueConversationAssetRow NewLookupRow;
	NewLookupRow.ConversationTag = NewConversation->Header.ConversationTag;
	NewLookupRow.Conversation = TSoftObjectPtr<UARDialogueConversationAsset>(NewConversation);

	ConversationLookupTable->Modify();
	ConversationLookupTable->AddRow(LookupRowName, NewLookupRow);
	ConversationLookupTable->MarkPackageDirty();

	AppendLogLine(FString::Printf(TEXT("Created conversation asset '%s' and lookup row '%s' for speaker '%s' (band %d, min rel %.1f)."),
		*PackageName,
		*LookupRowName.ToString(),
		*SpeakerRow->SpeakerTag.ToString(),
		TargetRelationshipBand,
		NewConversation->Header.MinimumRelationshipPoints));

	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
	{
		AssetEditorSubsystem->OpenEditorForAsset(NewConversation);
	}
	SDialogueConversationGraphEditorPanel::RequestOpenConversation(NewConversation);
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
	SDialogueConversationGraphEditorPanel::RequestOpenConversation(SelectedItems[0]->Asset.Get());
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
	TMap<FGameplayTag, UARDialogueConversationAsset*> ConversationsByTag;
	GatherConversationAssetsFromLookup(Settings, ConversationsByTag);
	for (const TPair<FGameplayTag, UARDialogueConversationAsset*>& Pair : ConversationsByTag)
	{
		UARDialogueConversationAsset* Conversation = Pair.Value;
		if (!Conversation)
		{
			AppendLogLine(FString::Printf(TEXT("ERROR: Failed to load conversation for tag '%s'."), *Pair.Key.ToString()));
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

FReply SDialogueSpeakerEditorPanel::HandleSortByConversationCount()
{
	SetSortMode(ESpeakerSortMode::ConversationCount);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleResetThresholds()
{
	EditedRelationshipThresholds = { 50.0f, 150.0f, 300.0f, 500.0f };
	SelectedThresholdIndex = EditedRelationshipThresholds.IsEmpty() ? INDEX_NONE : 0;
	RefreshThresholdList();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleAddThreshold()
{
	if (EditedRelationshipThresholds.IsEmpty())
	{
		EditedRelationshipThresholds = { 0.0f };
		SelectedThresholdIndex = 0;
		RefreshThresholdList();
		RefreshConversationMap();
		return FReply::Handled();
	}

	const int32 InsertIndex = EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex)
		? SelectedThresholdIndex + 1
		: EditedRelationshipThresholds.Num();
	const float BaseValue = EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex)
		? EditedRelationshipThresholds[SelectedThresholdIndex]
		: EditedRelationshipThresholds.Last();
	EditedRelationshipThresholds.Insert(BaseValue + 50.0f, InsertIndex);
	SelectedThresholdIndex = InsertIndex;
	RefreshThresholdList();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleRemoveThreshold()
{
	if (!EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex))
	{
		return FReply::Handled();
	}

	EditedRelationshipThresholds.RemoveAt(SelectedThresholdIndex, 1, EAllowShrinking::No);
	if (EditedRelationshipThresholds.IsEmpty())
	{
		SelectedThresholdIndex = INDEX_NONE;
	}
	else
	{
		SelectedThresholdIndex = FMath::Clamp(SelectedThresholdIndex, 0, EditedRelationshipThresholds.Num() - 1);
	}
	RefreshThresholdList();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleMoveThresholdUp()
{
	if (!EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex) || SelectedThresholdIndex <= 0)
	{
		return FReply::Handled();
	}

	EditedRelationshipThresholds.Swap(SelectedThresholdIndex, SelectedThresholdIndex - 1);
	--SelectedThresholdIndex;
	RefreshThresholdList();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleMoveThresholdDown()
{
	if (!EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex)
		|| SelectedThresholdIndex >= EditedRelationshipThresholds.Num() - 1)
	{
		return FReply::Handled();
	}

	EditedRelationshipThresholds.Swap(SelectedThresholdIndex, SelectedThresholdIndex + 1);
	++SelectedThresholdIndex;
	RefreshThresholdList();
	RefreshConversationMap();
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

	FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!Row)
	{
		AppendLogLine(TEXT("Selected speaker row could not be loaded."));
		return FReply::Handled();
	}

	if (!EditedPortraitTag.IsValid())
	{
		AppendLogLine(TEXT("Portrait tag is required."));
		return FReply::Handled();
	}

	FSpeakerPortraitEntry NewPortrait;
	NewPortrait.PortraitTag = EditedPortraitTag;
	NewPortrait.Portrait.PortraitTexture = EditedPortraitTexture;

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

	FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("<Invalid Conversation Entry>")))
		];
	}

	if (Item->bIsBandHeader)
	{
		return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->DisplayTitle))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		];
	}

	const FString MetaLine = FString::Printf(
		TEXT("Tag: %s | Pri %d | MinRel %.1f | Important:%s | Repeatable:%s"),
		*Item->ConversationTag.ToString(),
		Item->Priority,
		Item->MinimumRelationshipPoints,
		Item->bImportant ? TEXT("Y") : TEXT("N"),
		Item->bRepeatable ? TEXT("Y") : TEXT("N"));

	return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Label))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(MetaLine)).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(Item->GateSummary)).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(Item->MutationSummary)).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(Item->ChainSummary)).AutoWrapText(true)
		]
	];
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGeneratePortraitRow(TSharedPtr<FPortraitEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FPortraitEntry>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Label : FString(TEXT("<Invalid Portrait Entry>")))).AutoWrapText(true)
	];
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGenerateThresholdRow(TSharedPtr<FThresholdEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString Label = Item.IsValid()
		? FString::Printf(TEXT("%d: %.1f"), Item->ThresholdIndex + 1, Item->Value)
		: FString(TEXT("<Invalid Threshold Entry>"));

	return SNew(STableRow<TSharedPtr<FThresholdEntry>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromString(Label)).AutoWrapText(true)
	];
}

void SDialogueSpeakerEditorPanel::OnSpeakerSelectionChanged(TSharedPtr<FSpeakerEntry> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (bIgnoreSpeakerSelectionChanged)
	{
		return;
	}

	if (!Item.IsValid())
	{
		SelectedSpeakerRowName = NAME_None;
		SelectedPortraitIndex = INDEX_NONE;
		SyncSpeakerFieldsFromSelection();
		return;
	}

	SelectedSpeakerRowName = Item->RowName;
	SelectedPortraitIndex = INDEX_NONE;
	SyncSpeakerFieldsFromSelection();
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
	SDialogueConversationGraphEditorPanel::RequestOpenConversation(Item->Asset.Get());
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("AR_DialogueConversationGraphEditor")));
}

void SDialogueSpeakerEditorPanel::OnPortraitSelectionChanged(TSharedPtr<FPortraitEntry> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (!Item.IsValid())
	{
		SelectedPortraitIndex = INDEX_NONE;
		EditedPortraitTag = FGameplayTag();
		EditedPortraitTexture = TSoftObjectPtr<UTexture2D>();
		return;
	}

	SelectedPortraitIndex = Item->PortraitIndex;

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		return;
	}
	const FARDialogueSpeakerRow* Row = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!Row || !Row->Portraits.IsValidIndex(SelectedPortraitIndex))
	{
		return;
	}

	const FSpeakerPortraitEntry& Portrait = Row->Portraits[SelectedPortraitIndex];
	EditedPortraitTag = Portrait.PortraitTag;
	EditedPortraitTexture = Portrait.Portrait.PortraitTexture;
}

void SDialogueSpeakerEditorPanel::OnThresholdSelectionChanged(TSharedPtr<FThresholdEntry> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	SelectedThresholdIndex = Item.IsValid() ? Item->ThresholdIndex : INDEX_NONE;
}

