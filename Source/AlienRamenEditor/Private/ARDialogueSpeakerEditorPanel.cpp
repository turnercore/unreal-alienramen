#include "ARDialogueSpeakerEditorPanel.h"

#include "ARDialogueConversationAsset.h"
#include "ARDialogueConversationGraphEditorPanel.h"
#include "ARDialogueEditorSettings.h"
#include "ARDialogueSettings.h"
#include "ARDialogueSubsystem.h"
#include "TagContentResolverSubsystem.h"
#include "TagContentResolverEditorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "PropertyCustomizationHelpers.h"
#include "SGameplayTagCombo.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
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
			PortraitBrush = FSlateBrush();
			PortraitBrush.DrawAs = ESlateBrushDrawType::Image;
			PortraitBrush.ImageSize = FVector2D(20.0f, 20.0f);
			if (Item.IsValid())
			{
				if (UTexture2D* PortraitTexture = Item->Row.DefaultPortrait.PortraitTexture.LoadSynchronous())
				{
					PortraitBrush.SetResourceObject(PortraitTexture);
				}
			}
			SMultiColumnTableRow<TSharedPtr<SDialogueSpeakerEditorPanel::FSpeakerEntry>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock).Text(FText::FromString(TEXT("<Invalid>")));
			}

			if (ColumnName == TEXT("Portrait"))
			{
				if (PortraitBrush.GetResourceObject() != nullptr)
				{
					return SNew(SBox)
						.WidthOverride(20.0f)
						.HeightOverride(20.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(&PortraitBrush)
						];
				}

				return SNew(SBox)
					.WidthOverride(20.0f)
					.HeightOverride(20.0f);
			}

			if (ColumnName == TEXT("DisplayName"))
			{
				return SNew(STextBlock).Text(Item->Row.DisplayName);
			}
			if (ColumnName == TEXT("SpeakerTag"))
			{
				return SNew(STextBlock).Text(FText::FromString(Item->Row.SpeakerTag.ToString()));
			}
			if (ColumnName == TEXT("ConversationCount"))
			{
				return SNew(STextBlock).Text(FText::AsNumber(Item->ConversationCount));
			}
			if (ColumnName == TEXT("EmotionCount"))
			{
				return SNew(STextBlock).Text(FText::AsNumber(Item->EmotionCount));
			}
			return SNew(STextBlock).Text(FText::GetEmpty());
		}

	private:
		TSharedPtr<SDialogueSpeakerEditorPanel::FSpeakerEntry> Item;
		FSlateBrush PortraitBrush;
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

	static FString GetSpeakerAssetNameSegment(const FARDialogueSpeakerRow& SpeakerRow, const FName SpeakerRowName)
	{
		const FString DisplayName = SpeakerRow.DisplayName.ToString().TrimStartAndEnd();
		if (!DisplayName.IsEmpty())
		{
			return SanitizeTagSegment(DisplayName);
		}

		return SanitizeTagSegment(SpeakerRowName.ToString());
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
		SNew(SSplitter)
			+ SSplitter::Slot().Value(0.42f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Refresh"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleRefresh)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).Text(FText::FromString(TEXT("New Speaker"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleNewSpeaker)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SAssignNew(SearchTextBox, SSearchBox)
						.HintText(FText::FromString(TEXT("Search display name / tag")))
						.OnTextChanged_Lambda([this](const FText&){ ApplySpeakerFilterAndSort(); })
					]
					+ SVerticalBox::Slot().FillHeight(0.74f)
					[
						SAssignNew(SpeakerListView, SListView<TSharedPtr<FSpeakerEntry>>)
						.ListItemsSource(&FilteredSpeakerEntries)
						.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGenerateSpeakerRow)
						.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnSpeakerSelectionChanged)
						.OnContextMenuOpening(this, &SDialogueSpeakerEditorPanel::BuildSpeakerListContextMenu)
						.OnKeyDownHandler(FOnKeyDown::CreateSP(this, &SDialogueSpeakerEditorPanel::HandleSpeakerListKeyDown))
						.HeaderRow(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(TEXT("Portrait"))
								.DefaultLabel(FText::FromString(TEXT(" ")))
								.FixedWidth(28.0f)
								.HAlignHeader(HAlign_Center)
								.HAlignCell(HAlign_Center)
								.VAlignCell(VAlign_Center)
							+ SHeaderRow::Column(TEXT("DisplayName")).DefaultLabel(FText::FromString(TEXT("Display Name"))).FillWidth(0.30f)
							+ SHeaderRow::Column(TEXT("SpeakerTag")).DefaultLabel(FText::FromString(TEXT("Speaker Tag"))).FillWidth(0.43f)
							+ SHeaderRow::Column(TEXT("ConversationCount")).DefaultLabel(FText::FromString(TEXT("Conversations"))).FillWidth(0.15f)
							+ SHeaderRow::Column(TEXT("EmotionCount")).DefaultLabel(FText::FromString(TEXT("Emotions"))).FillWidth(0.12f)
						)
					]
					+ SVerticalBox::Slot().FillHeight(0.26f).Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
						.Padding(4.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return FText::FromString(ValidationOutput); })
								.AutoWrapText(true)
							]
						]
					]
				]
			]
			+ SSplitter::Slot().Value(0.36f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Default Portrait")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f).VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(48.0f)
							.HeightOverride(48.0f)
							[
								SNew(SImage)
								.Image(this, &SDialogueSpeakerEditorPanel::GetDefaultPortraitFieldBrush)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(DefaultPortraitTexturePicker, SObjectPropertyEntryBox)
							.AllowedClass(UTexture2D::StaticClass())
							.DisplayThumbnail(true)
							.ObjectPath(this, &SDialogueSpeakerEditorPanel::GetEditedDefaultPortraitTexturePath)
							.OnObjectChanged(this, &SDialogueSpeakerEditorPanel::OnEditedDefaultPortraitTextureChanged)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Name")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(DisplayNameTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Display Name")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Description")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(DescriptionTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Description")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Line Font Style (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(LineFontStyleTextBox, SEditableTextBox).HintText(FText::FromString(TEXT("Rich text style tag (for example: npc_brother)")))
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
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(SGameplayTagCombo)
						.Tag(this, &SDialogueSpeakerEditorPanel::GetEditedFactionTag)
						.OnTagChanged(this, &SDialogueSpeakerEditorPanel::OnEditedFactionTagChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SAssignNew(EmotionsExpandableArea, SExpandableArea)
						.InitiallyCollapsed(false)
						.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
						.OnAreaExpansionChanged_Lambda([this](const bool bExpanded) { bEmotionsExpanded = bExpanded; })
						.HeaderContent()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Emotions")))
						]
						.BodyContent()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().MaxHeight(130.0f).Padding(0.0f, 0.0f, 0.0f, 4.0f)
							[
								SAssignNew(PortraitListView, SListView<TSharedPtr<FPortraitEntry>>)
								.ListItemsSource(&PortraitEntries)
								.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGeneratePortraitRow)
								.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnPortraitSelectionChanged)
								.OnContextMenuOpening(this, &SDialogueSpeakerEditorPanel::BuildEmotionListContextMenu)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("No emotions yet. Add one to start.")))
								.Visibility_Lambda([this]() { return PortraitEntries.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Emotion Tag")))
								.Visibility_Lambda([this]() { return PortraitEntries.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SAssignNew(EmotionTagComboHost, SBox)
								.Visibility_Lambda([this]() { return PortraitEntries.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Emotion Portrait Texture")))
								.Visibility_Lambda([this]() { return PortraitEntries.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
							[
								SAssignNew(PortraitTexturePicker, SObjectPropertyEntryBox)
								.AllowedClass(UTexture2D::StaticClass())
								.DisplayThumbnail(true)
								.ObjectPath(this, &SDialogueSpeakerEditorPanel::GetEditedPortraitTexturePath)
								.OnObjectChanged(this, &SDialogueSpeakerEditorPanel::OnEditedPortraitTextureChanged)
								.Visibility_Lambda([this]() { return PortraitEntries.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SAssignNew(ThresholdsExpandableArea, SExpandableArea)
						.InitiallyCollapsed(false)
						.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
						.OnAreaExpansionChanged_Lambda([this](const bool bExpanded) { bThresholdsExpanded = bExpanded; })
						.HeaderContent()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Relationship Bands")))
						]
						.BodyContent()
						[
							SAssignNew(ThresholdListView, SListView<TSharedPtr<FThresholdEntry>>)
							.ListItemsSource(&ThresholdEntries)
							.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGenerateThresholdRow)
							.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnThresholdSelectionChanged)
							.OnMouseButtonDoubleClick(this, &SDialogueSpeakerEditorPanel::OnThresholdDoubleClicked)
							.OnContextMenuOpening(this, &SDialogueSpeakerEditorPanel::BuildThresholdContextMenu)
							.OnKeyDownHandler(FOnKeyDown::CreateSP(this, &SDialogueSpeakerEditorPanel::HandleThresholdListKeyDown))
						]
					]
				]
			]
			+ SSplitter::Slot().Value(0.22f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Conversation Map")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									int32 ConversationCount = 0;
									TSet<int32> UsedBands;
									for (const TSharedPtr<FConversationEntry>& Entry : ConversationEntries)
									{
										if (!Entry.IsValid() || Entry->bIsBandHeader)
										{
											continue;
										}

										++ConversationCount;
										UsedBands.Add(Entry->RelationshipBand);
									}

									return FText::FromString(FString::Printf(
										TEXT("Primary speaker only | %d conversations in %d bands"),
										ConversationCount,
										UsedBands.Num()));
								})
								.ColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.70f, 0.70f, 1.0f)))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Create Conversation"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleCreateConversation)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Find Broken"))).OnClicked(this, &SDialogueSpeakerEditorPanel::HandleFindBrokenConversations)
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SBorder)
						.Padding(4.0f)
						[
							SAssignNew(ConversationListView, SListView<TSharedPtr<FConversationEntry>>)
							.ListItemsSource(&ConversationEntries)
							.OnGenerateRow(this, &SDialogueSpeakerEditorPanel::OnGenerateConversationRow)
							.OnMouseButtonDoubleClick(this, &SDialogueSpeakerEditorPanel::OnConversationDoubleClicked)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("No conversations found for this speaker yet.")))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f, 1.0f)))
						.Visibility_Lambda([this]()
						{
							for (const TSharedPtr<FConversationEntry>& Entry : ConversationEntries)
							{
								if (Entry.IsValid() && !Entry->bIsBandHeader)
								{
									return EVisibility::Collapsed;
								}
							}
							return EVisibility::Visible;
						})
					]
				]
			]
	];

	SyncSpeakerFieldsFromSelection();
	RebuildEmotionTagCombo();
}

SDialogueSpeakerEditorPanel::~SDialogueSpeakerEditorPanel()
{
	bIsTearingDown = true;
	HandleSaveSpeaker();
}

FReply SDialogueSpeakerEditorPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;
	if ((InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown())
		&& !InKeyEvent.IsAltDown()
		&& InKeyEvent.GetKey() == EKeys::S)
	{
		HandleSaveSpeaker();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
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
		Entry->EmotionCount = 1 + Entry->Row.Portraits.Num();
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
		Entry->LockedConditionCount = Conversation->Header.LockedConditions.Conditions.Num();
		Entry->BlockedConditionCount = Conversation->Header.BlockedConditions.Conditions.Num();
		Entry->bSeenByGameBlocksReoffer = Conversation->Header.bSeenByGameBlocksReoffer;
		Entry->bSeenByPlayerBlocksReoffer = Conversation->Header.bSeenByPlayerBlocksReoffer;
		Entry->bCompletedByGameBlocksReoffer = Conversation->Header.bCompletedByGameBlocksReoffer;
		Entry->RelationshipBand = ComputeRelationshipBand(Entry->MinimumRelationshipPoints, ThresholdsForBandCalc);
		Entry->RelationshipBandLabel = BuildRelationshipBandLabel(Entry->RelationshipBand, ThresholdsForBandCalc);
		Entry->Label = BuildConversationEntryLabel(Conversation);
		Entry->GateSummary = FString::Printf(
			TEXT("Locked %d | Blocked %d"),
			Entry->LockedConditionCount,
			Entry->BlockedConditionCount);

		FConversationChainData ChainData;
		GatherConversationChainTags(
			Conversation,
			ChainData.RequiredTags,
			ChainData.AddedTags,
			ChainData.RemovedTags);
		Entry->AddedTagCount = ChainData.AddedTags.Num();
		Entry->RemovedTagCount = ChainData.RemovedTags.Num();
		Entry->RequiredTagCount = ChainData.RequiredTags.Num();
		Entry->MutationSummary = FString::Printf(
			TEXT("Tag mutations +%d / -%d | Requires %d"),
			Entry->AddedTagCount,
			Entry->RemovedTagCount,
			Entry->RequiredTagCount);

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
		Entry->UnlocksSummary = UnlocksSummary;
		Entry->RequiresSummary = RequiresSummary;
		Entry->ChainSummary = FString::Printf(
			TEXT("Unlocks -> %s | Requires <- %s"),
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
		Entry->PortraitTag = Portrait.PortraitTag;
		Entry->PortraitTexture = Portrait.Portrait.PortraitTexture;
		Entry->Label = Portrait.PortraitTag.IsValid() ? Portrait.PortraitTag.ToString() : FString(TEXT("<No Tag>"));
		Entry->PortraitBrush.DrawAs = ESlateBrushDrawType::Image;
		Entry->PortraitBrush.ImageSize = FVector2D(32.0f, 32.0f);
		if (UTexture2D* PortraitTexture = Entry->PortraitTexture.LoadSynchronous())
		{
			Entry->PortraitBrush.SetResourceObject(PortraitTexture);
		}
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

bool SDialogueSpeakerEditorPanel::BuildEditedSpeakerRow(FARDialogueSpeakerRow& OutRow, FString& OutError) const
{
	OutError.Empty();

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		OutError = TEXT("No speaker selected.");
		return false;
	}

	const FARDialogueSpeakerRow* CurrentRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!CurrentRow)
	{
		OutError = TEXT("Selected speaker row could not be resolved in table.");
		return false;
	}

	const FString DisplayNameText = DisplayNameTextBox.IsValid() ? DisplayNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString DescriptionText = DescriptionTextBox.IsValid() ? DescriptionTextBox->GetText().ToString() : FString();
	const FString LineFontStyleTagText = LineFontStyleTextBox.IsValid()
		? LineFontStyleTextBox->GetText().ToString().TrimStartAndEnd()
		: FString();

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

	OutRow = *CurrentRow;
	OutRow.DisplayName = FText::FromString(DisplayNameText);
	OutRow.Description = FText::FromString(DescriptionText);
	OutRow.SpeakerTag = EditedSpeakerTag;
	OutRow.FactionTag = EditedFactionTag;
	OutRow.LineFontStyleTag = LineFontStyleTagText.IsEmpty() ? NAME_None : FName(*LineFontStyleTagText);
	OutRow.DefaultPortrait.PortraitTexture = EditedDefaultPortraitTexture;
	OutRow.RelationshipThresholds = EditedRelationshipThresholds;
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

	FARDialogueSpeakerRow* MutableRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!MutableRow)
	{
		OutError = TEXT("Selected speaker row could not be resolved in table.");
		return false;
	}

	FARDialogueSpeakerRow EditedRow;
	if (!BuildEditedSpeakerRow(EditedRow, OutError))
	{
		return false;
	}

	SpeakerTable->Modify();
	*MutableRow = EditedRow;
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
	EnsureSpeakerDefaultEmotionTag(NewTag);
	RebuildEmotionTagCombo();

	if (EditedPortraitTag.IsValid() && NewTag.IsValid())
	{
		const FString RootPath = NewTag.ToString() + TEXT(".");
		if (!EditedPortraitTag.ToString().StartsWith(RootPath, ESearchCase::CaseSensitive))
		{
			EditedPortraitTag = FGameplayTag();
		}
	}
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

FString SDialogueSpeakerEditorPanel::GetEmotionTagFilter() const
{
	if (!EditedSpeakerTag.IsValid())
	{
		return TEXT("Dialogue.Speaker");
	}

	return EditedSpeakerTag.ToString();
}

void SDialogueSpeakerEditorPanel::RebuildEmotionTagCombo()
{
	if (bIsTearingDown || !EmotionTagComboHost.IsValid())
	{
		return;
	}

	EmotionTagComboHost->SetContent(
		SNew(SGameplayTagCombo)
		.Filter(GetEmotionTagFilter())
		.Tag(this, &SDialogueSpeakerEditorPanel::GetEditedPortraitTag)
		.OnTagChanged(this, &SDialogueSpeakerEditorPanel::OnEditedPortraitTagChanged));
}

void SDialogueSpeakerEditorPanel::EnsureSpeakerDefaultEmotionTag(const FGameplayTag& SpeakerTag)
{
	if (!SpeakerTag.IsValid())
	{
		return;
	}

	FString EnsureTagError;
	const FString DefaultTagPath = SpeakerTag.ToString() + TEXT(".Default");
	const FGameplayTag DefaultTag = EnsureGameplayTagRegistered(
		DefaultTagPath,
		FString::Printf(TEXT("Auto-created default emotion tag for speaker '%s'."), *SpeakerTag.ToString()),
		EnsureTagError);

	if (!DefaultTag.IsValid() && !EnsureTagError.IsEmpty())
	{
		AppendLogLine(FString::Printf(TEXT("Failed to ensure speaker default emotion tag '%s': %s"), *DefaultTagPath, *EnsureTagError));
	}
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
		if (LineFontStyleTextBox.IsValid()) { LineFontStyleTextBox->SetText(FText::GetEmpty()); }
		EditedSpeakerTag = FGameplayTag();
		EditedFactionTag = FGameplayTag();
		EditedLineFontStyleTag = NAME_None;
		EditedDefaultPortraitTexture = TSoftObjectPtr<UTexture2D>();
		EditedPortraitTag = FGameplayTag();
		EditedPortraitTexture = TSoftObjectPtr<UTexture2D>();
		EditedRelationshipThresholds.Reset();
		SelectedThresholdIndex = INDEX_NONE;
		RefreshThresholdList();
		RefreshPortraitList();
		RefreshConversationMap();
		RebuildEmotionTagCombo();
		return;
	}

	if (DisplayNameTextBox.IsValid()) { DisplayNameTextBox->SetText(SelectedRow.DisplayName); }
	if (DescriptionTextBox.IsValid()) { DescriptionTextBox->SetText(SelectedRow.Description); }
	if (LineFontStyleTextBox.IsValid()) { LineFontStyleTextBox->SetText(FText::FromName(SelectedRow.LineFontStyleTag)); }
	OnEditedSpeakerTagChanged(SelectedRow.SpeakerTag);
	EditedFactionTag = SelectedRow.FactionTag;
	EditedLineFontStyleTag = SelectedRow.LineFontStyleTag;
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

TSharedPtr<SWidget> SDialogueSpeakerEditorPanel::BuildSpeakerListContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Copy")),
		FText::FromString(TEXT("Copy selected speaker row to local clipboard.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SDialogueSpeakerEditorPanel::HandleCopySpeaker)));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Cut")),
		FText::FromString(TEXT("Copy selected speaker row and delete it.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SDialogueSpeakerEditorPanel::HandleCutSpeaker)));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Paste")),
		FText::FromString(TEXT("Paste speaker row from local clipboard as a new row.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SDialogueSpeakerEditorPanel::HandlePasteSpeaker),
			FCanExecuteAction::CreateSP(this, &SDialogueSpeakerEditorPanel::CanPasteSpeaker)));

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Duplicate")),
		FText::FromString(TEXT("Duplicate selected speaker row.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleDuplicateSpeaker(); })));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Delete")),
		FText::FromString(TEXT("Delete selected speaker row.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleDeleteSpeaker(); })));

	return MenuBuilder.MakeWidget();
}

void SDialogueSpeakerEditorPanel::HandleCopySpeaker()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		return;
	}

	const FARDialogueSpeakerRow* SourceRow = SpeakerTable->FindRow<FARDialogueSpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!SourceRow)
	{
		return;
	}

	SpeakerClipboardSourceRowName = SelectedSpeakerRowName;
	SpeakerClipboardRow = *SourceRow;
	bHasSpeakerClipboard = true;
	AppendLogLine(FString::Printf(TEXT("Copied speaker '%s'."), *SelectedSpeakerRowName.ToString()));
}

void SDialogueSpeakerEditorPanel::HandleCutSpeaker()
{
	HandleCopySpeaker();
	if (bHasSpeakerClipboard)
	{
		HandleDeleteSpeaker();
	}
}

void SDialogueSpeakerEditorPanel::HandlePasteSpeaker()
{
	if (!CanPasteSpeaker())
	{
		return;
	}

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable)
	{
		return;
	}

	const FString SourceName = SpeakerClipboardSourceRowName.IsNone() ? TEXT("Speaker") : SpeakerClipboardSourceRowName.ToString();
	FName NewRowName(*FString::Printf(TEXT("%s_Copy"), *SourceName));
	for (int32 Suffix = 1; SpeakerTable->GetRowMap().Contains(NewRowName); ++Suffix)
	{
		NewRowName = FName(*FString::Printf(TEXT("%s_Copy%d"), *SourceName, Suffix));
	}

	FARDialogueSpeakerRow NewRow = SpeakerClipboardRow;
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

	AppendLogLine(FString::Printf(TEXT("Pasted speaker as '%s'."), *NewRowName.ToString()));
	RefreshData();
	SetSelectedSpeakerRow(NewRowName);
}

bool SDialogueSpeakerEditorPanel::CanPasteSpeaker() const
{
	return bHasSpeakerClipboard;
}

TSharedPtr<SWidget> SDialogueSpeakerEditorPanel::BuildThresholdContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Add Band")),
		FText::FromString(TEXT("Insert a new relationship band after the selected band.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleAddThreshold(); })));
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Remove Band")),
		FText::FromString(TEXT("Remove the selected relationship band.")),
		FSlateIcon(),
	FUIAction(FExecuteAction::CreateLambda([this]() { HandleRemoveThreshold(); })));
	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SDialogueSpeakerEditorPanel::BuildEmotionListContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Add Emotion")),
		FText::FromString(TEXT("Add an emotion entry (or apply edits to selected entry).")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			if (PortraitEntries.IsEmpty())
			{
				HandleAddEmotionSlot();
			}
			else
			{
				HandleAddPortrait();
			}
		})));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Remove Emotion")),
		FText::FromString(TEXT("Remove the currently selected emotion entry.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleRemovePortrait(); })));

	return MenuBuilder.MakeWidget();
}

void SDialogueSpeakerEditorPanel::BeginInlineThresholdEdit(const int32 ThresholdIndex)
{
	if (!ThresholdEntries.IsValidIndex(ThresholdIndex))
	{
		return;
	}

	EditingThresholdIndex = ThresholdIndex;
	SelectedThresholdIndex = ThresholdIndex;
	if (ThresholdListView.IsValid())
	{
		ThresholdListView->RequestListRefresh();
	}
}

void SDialogueSpeakerEditorPanel::CommitInlineThresholdEdit(const int32 ThresholdIndex, const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnCleared)
	{
		EditingThresholdIndex = INDEX_NONE;
		if (ThresholdListView.IsValid())
		{
			ThresholdListView->RequestListRefresh();
		}
		return;
	}

	const FString TextValue = NewText.ToString().TrimStartAndEnd();
	double ParsedValue = 0.0;
	if (!LexTryParseString(ParsedValue, *TextValue))
	{
		AppendLogLine(FString::Printf(TEXT("Invalid band value '%s'."), *TextValue));
		return;
	}

	SelectedThresholdIndex = ThresholdIndex;
	SetEditedThresholdValue(static_cast<float>(ParsedValue));
	EditingThresholdIndex = INDEX_NONE;
	if (ThresholdListView.IsValid())
	{
		ThresholdListView->RequestListRefresh();
	}
}

FReply SDialogueSpeakerEditorPanel::HandleThresholdListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;
	if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
	{
		HandleRemoveThreshold();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDialogueSpeakerEditorPanel::HandleSpeakerListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;

	const bool bCtrl = InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown();
	if (bCtrl && !InKeyEvent.IsAltDown())
	{
		if (InKeyEvent.GetKey() == EKeys::C)
		{
			HandleCopySpeaker();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::V)
		{
			HandlePasteSpeaker();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::D)
		{
			HandleDuplicateSpeaker();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::X)
		{
			HandleCutSpeaker();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::S)
		{
			HandleSaveSpeaker();
			return FReply::Handled();
		}
	}

	if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
	{
		HandleDeleteSpeaker();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

const FSlateBrush* SDialogueSpeakerEditorPanel::GetDefaultPortraitFieldBrush() const
{
	DefaultPortraitFieldBrush.DrawAs = ESlateBrushDrawType::Image;
	DefaultPortraitFieldBrush.ImageSize = FVector2D(48.0f, 48.0f);
	DefaultPortraitFieldBrush.SetResourceObject(nullptr);
	if (UTexture2D* Texture = EditedDefaultPortraitTexture.LoadSynchronous())
	{
		DefaultPortraitFieldBrush.SetResourceObject(Texture);
		return &DefaultPortraitFieldBrush;
	}

	return FAppStyle::GetBrush(TEXT("Graph.StateNode.Icon"));
}

void SDialogueSpeakerEditorPanel::RefreshThresholdList()
{
	ThresholdEntries.Reset();
	if (!EditedRelationshipThresholds.IsValidIndex(EditingThresholdIndex))
	{
		EditingThresholdIndex = INDEX_NONE;
	}
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

	FString ParseError;
	FARDialogueSpeakerRow EditedRow;
	if (!BuildEditedSpeakerRow(EditedRow, ParseError))
	{
		AppendLogLine(FString::Printf(TEXT("Validation failed: %s"), *ParseError));
		return FReply::Handled();
	}

	FDialogueValidationReport Report;
	const bool bValid = ValidateSpeakerWithBestAvailable(EditedRow, Report);
	AppendLogLine(bValid ? TEXT("Speaker validation succeeded.") : TEXT("Speaker validation failed."));

	for (const FDialogueValidationIssue& Issue : Report.Issues)
	{
		const TCHAR* Severity = Issue.Severity == EDialogueValidationSeverity::Error
			? TEXT("ERROR")
			: (Issue.Severity == EDialogueValidationSeverity::Warning ? TEXT("WARN") : TEXT("INFO"));
		AppendLogLine(FString::Printf(TEXT("[%s] %s"), Severity, *Issue.Message.ToString()));
	}
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleSaveSpeaker()
{
	FString ParseError;
	FARDialogueSpeakerRow EditedRow;
	if (!BuildEditedSpeakerRow(EditedRow, ParseError))
	{
		AppendLogLine(FString::Printf(TEXT("Save failed: %s"), *ParseError));
		return FReply::Handled();
	}

	FDialogueValidationReport Report;
	const bool bValid = ValidateSpeakerWithBestAvailable(EditedRow, Report);
	for (const FDialogueValidationIssue& Issue : Report.Issues)
	{
		if (Issue.Severity == EDialogueValidationSeverity::Info)
		{
			continue;
		}

		const TCHAR* Severity = Issue.Severity == EDialogueValidationSeverity::Error ? TEXT("ERROR") : TEXT("WARN");
		AppendLogLine(FString::Printf(TEXT("[%s] %s"), Severity, *Issue.Message.ToString()));
	}
	if (!bValid || Report.HasErrors())
	{
		AppendLogLine(TEXT("Save blocked: validation failed."));
		return FReply::Handled();
	}

	FString Error;
	if (!CommitEditedSpeakerRow(Error))
	{
		AppendLogLine(FString::Printf(TEXT("Save failed: %s"), *Error));
		return FReply::Handled();
	}

	AppendLogLine(FString::Printf(TEXT("Saved speaker row '%s'."), *SelectedSpeakerRowName.ToString()));
	if (!bIsTearingDown)
	{
		RefreshData();
		SetSelectedSpeakerRow(SelectedSpeakerRowName);
	}
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

	const UARDialogueEditorSettings* DialogueEditorSettings = GetDefault<UARDialogueEditorSettings>();
	FString PackageFolder = DialogueEditorSettings ? DialogueEditorSettings->ConversationAssetsFolder.Path : FString();
	if (PackageFolder.IsEmpty())
	{
		PackageFolder = TEXT("/Game/Data/Conversations");
	}
	if (!PackageFolder.StartsWith(TEXT("/")))
	{
		PackageFolder = FString(TEXT("/")) + PackageFolder;
	}
	PackageFolder.RemoveFromEnd(TEXT("/"));

	if (!FPackageName::IsValidLongPackageName(PackageFolder))
	{
		AppendLogLine(FString::Printf(
			TEXT("Invalid ConversationAssetsFolder '%s'. Use a package path like '/Game/Data/Conversations' in Project Settings -> Alien Ramen -> Dialogue Tooling."),
			*PackageFolder));
		return FReply::Handled();
	}

	const FString SpeakerNameSegment = GetSpeakerAssetNameSegment(*SpeakerRow, SelectedSpeakerRowName);
	FString AssetNameBase = FString::Printf(TEXT("Conversation_%s"), *SpeakerNameSegment);
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

FReply SDialogueSpeakerEditorPanel::HandleAddEmotionSlot()
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

	if (EditedDefaultPortraitTexture.IsNull())
	{
		AppendLogLine(TEXT("Set Default Portrait before adding emotions."));
		return FReply::Handled();
	}

	FSpeakerPortraitEntry NewPortrait;
	SpeakerTable->Modify();
	Row->Portraits.Add(NewPortrait);
	const int32 NewPortraitIndex = Row->Portraits.Num() - 1;
	SpeakerTable->MarkPackageDirty();

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	SelectedPortraitIndex = NewPortraitIndex;
	if (PortraitEntries.IsValidIndex(SelectedPortraitIndex) && PortraitListView.IsValid())
	{
		PortraitListView->SetSelection(PortraitEntries[SelectedPortraitIndex]);
	}
	AppendLogLine(TEXT("Added empty emotion entry."));
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

	if (EditedDefaultPortraitTexture.IsNull())
	{
		AppendLogLine(TEXT("Set Default Portrait Texture before adding emotion portraits."));
		return FReply::Handled();
	}

	if (!EditedPortraitTag.IsValid())
	{
		AppendLogLine(TEXT("Emotion tag is required."));
		return FReply::Handled();
	}

	FSpeakerPortraitEntry NewPortrait;
	NewPortrait.PortraitTag = EditedPortraitTag;
	NewPortrait.Portrait.PortraitTexture = EditedPortraitTexture;

	SpeakerTable->Modify();
	if (SelectedPortraitIndex != INDEX_NONE && Row->Portraits.IsValidIndex(SelectedPortraitIndex))
	{
		Row->Portraits[SelectedPortraitIndex] = NewPortrait;
		AppendLogLine(FString::Printf(TEXT("Updated emotion index %d."), SelectedPortraitIndex));
	}
	else
	{
		Row->Portraits.Add(NewPortrait);
		AppendLogLine(TEXT("Added new emotion entry."));
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
		AppendLogLine(TEXT("No emotion selected."));
		return FReply::Handled();
	}

	SpeakerTable->Modify();
	Row->Portraits.RemoveAt(SelectedPortraitIndex);
	SpeakerTable->MarkPackageDirty();
	AppendLogLine(FString::Printf(TEXT("Removed emotion index %d."), SelectedPortraitIndex));

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
		const FLinearColor BandColor = (Item->RelationshipBand % 2 == 0)
			? FLinearColor(0.11f, 0.18f, 0.28f, 1.0f)
			: FLinearColor(0.12f, 0.14f, 0.22f, 1.0f);

		return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
		[
			SNew(SBorder)
			.BorderBackgroundColor(BandColor)
			.Padding(FMargin(8.0f, 5.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayTitle))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
		];
	}

	return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.075f, 0.075f, 0.075f, 1.0f))
		.Padding(FMargin(7.0f, 6.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(
					Item->bImportant
						? FLinearColor(0.70f, 0.40f, 0.10f, 1.0f)
						: (Item->bRepeatable
							? FLinearColor(0.14f, 0.45f, 0.42f, 1.0f)
							: FLinearColor(0.20f, 0.30f, 0.45f, 1.0f)))
				.Padding(FMargin(2.0f, 30.0f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Label))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->ConversationTag.ToString()))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.67f, 0.67f, 0.67f, 1.0f)))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(SWrapBox)
					.UseAllottedSize(true)
					+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0.20f, 0.24f, 0.32f, 1.0f))
						.Padding(FMargin(6.0f, 2.0f))
						[
							SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Pri %d"), Item->Priority)))
						]
					]
					+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0.20f, 0.24f, 0.32f, 1.0f))
						.Padding(FMargin(6.0f, 2.0f))
						[
							SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("MinRel %.1f"), Item->MinimumRelationshipPoints)))
						]
					]
					+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0.20f, 0.24f, 0.32f, 1.0f))
						.Padding(FMargin(6.0f, 2.0f))
						[
							SNew(STextBlock).Text(FText::FromString(Item->RelationshipBandLabel))
						]
					]
					+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(Item->bImportant ? FLinearColor(0.40f, 0.26f, 0.12f, 1.0f) : FLinearColor(0.13f, 0.13f, 0.13f, 1.0f))
						.Padding(FMargin(6.0f, 2.0f))
						[
							SNew(STextBlock).Text(FText::FromString(Item->bImportant ? TEXT("Important") : TEXT("Normal")))
						]
					]
					+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(Item->bRepeatable ? FLinearColor(0.15f, 0.30f, 0.28f, 1.0f) : FLinearColor(0.13f, 0.13f, 0.13f, 1.0f))
						.Padding(FMargin(6.0f, 2.0f))
						[
							SNew(STextBlock).Text(FText::FromString(Item->bRepeatable ? TEXT("Repeatable") : TEXT("One-shot")))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("Gates  Locked %d  |  Blocked %d"),
						Item->LockedConditionCount,
						Item->BlockedConditionCount)))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("Reoffer Blocks  SeenGame:%s  SeenPlayer:%s  CompletedGame:%s"),
						Item->bSeenByGameBlocksReoffer ? TEXT("Yes") : TEXT("No"),
						Item->bSeenByPlayerBlocksReoffer ? TEXT("Yes") : TEXT("No"),
						Item->bCompletedByGameBlocksReoffer ? TEXT("Yes") : TEXT("No"))))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("Mutations  +%d  -%d  |  RequiresTags %d"),
						Item->AddedTagCount,
						Item->RemovedTagCount,
						Item->RequiredTagCount)))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Chain Unlocks -> %s"), *Item->UnlocksSummary)))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Chain Requires <- %s"), *Item->RequiresSummary)))
					.AutoWrapText(true)
				]
			]
		]
	];
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGeneratePortraitRow(TSharedPtr<FPortraitEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FPortraitEntry>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("<Invalid Emotion Entry>"))).AutoWrapText(true)
		];
	}

	const FString TextureLabel = Item->PortraitTexture.IsNull()
		? FString(TEXT("No Texture"))
		: FPackageName::GetLongPackageAssetName(Item->PortraitTexture.ToString());

	const FSlateBrush* RowBrush = Item->PortraitBrush.GetResourceObject() != nullptr
		? &Item->PortraitBrush
		: FAppStyle::GetBrush(TEXT("Graph.StateNode.Icon"));

	return SNew(STableRow<TSharedPtr<FPortraitEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(32.0f)
			.HeightOverride(32.0f)
			[
				SNew(SImage)
				.Image(RowBrush)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Item->Label))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(TextureLabel))
			]
		]
	];
}

TSharedRef<ITableRow> SDialogueSpeakerEditorPanel::OnGenerateThresholdRow(TSharedPtr<FThresholdEntry> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FThresholdEntry>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("<Invalid Threshold Entry>"))).AutoWrapText(true)
		];
	}

	const FString BandLabel = FString::Printf(TEXT("Band %d"), Item->ThresholdIndex + 1);
	const FString ValueLabel = FString::Printf(TEXT("%.1f"), Item->Value);
	const FLinearColor BandTint = (Item->ThresholdIndex % 2 == 0)
		? FLinearColor(0.18f, 0.32f, 0.50f, 1.0f)
		: FLinearColor(0.22f, 0.25f, 0.40f, 1.0f);
	const bool bIsEditing = (Item->ThresholdIndex == EditingThresholdIndex);

	return SNew(STableRow<TSharedPtr<FThresholdEntry>>, OwnerTable)
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
		.Padding(FMargin(6.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(BandTint)
				.Padding(FMargin(6.0f, 2.0f))
				[
					SNew(STextBlock).Text(FText::FromString(BandLabel))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				bIsEditing
				? StaticCastSharedRef<SWidget>(
					SNew(SEditableTextBox)
					.Text(FText::FromString(ValueLabel))
					.SelectAllTextWhenFocused(true)
					.OnTextCommitted_Lambda([this, ThresholdIndex = Item->ThresholdIndex](const FText& NewText, ETextCommit::Type CommitType)
					{
						const_cast<SDialogueSpeakerEditorPanel*>(this)->CommitInlineThresholdEdit(ThresholdIndex, NewText, CommitType);
					}))
				: StaticCastSharedRef<SWidget>(
					SNew(STextBlock)
					.Text(FText::FromString(ValueLabel))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)))
			]
		]
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
		if (!SelectedSpeakerRowName.IsNone())
		{
			HandleSaveSpeaker();
		}
		SelectedSpeakerRowName = NAME_None;
		SelectedPortraitIndex = INDEX_NONE;
		SyncSpeakerFieldsFromSelection();
		return;
	}

	if (!SelectedSpeakerRowName.IsNone() && Item->RowName != SelectedSpeakerRowName)
	{
		HandleSaveSpeaker();
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

void SDialogueSpeakerEditorPanel::OnThresholdDoubleClicked(TSharedPtr<FThresholdEntry> Item)
{
	if (!Item.IsValid())
	{
		return;
	}

	BeginInlineThresholdEdit(Item->ThresholdIndex);
}

