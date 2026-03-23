#include "ParleyDialogueSpeakerEditorPanel.h"

#include "ParleyConversationAsset.h"
#include "ParleyDialogueConversationGraphEditorPanel.h"
#include "ParleyDialogueEditorSettings.h"
#include "ParleyDialogueSettings.h"
#include "ParleyDialogueSubsystem.h"
#include "TagKeySubsystem.h"
#include "TagKeyEditorHelpers.h"
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
#include "SGameplayTagContainerCombo.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "ScopedTransaction.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "Engine/Font.h"
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
#include "DragAndDrop/DecoratedDragDropOp.h"

namespace
{
	static TStrongObjectPtr<UParleyDialogueSubsystem>& GetCachedValidationDialogueSubsystem()
	{
		static TStrongObjectPtr<UParleyDialogueSubsystem> CachedValidationSubsystem;
		return CachedValidationSubsystem;
	}

	static FString GetTagLeafLabel(const FGameplayTag Tag)
	{
		const FString TagPath = Tag.ToString();
		int32 LastDotIndex = INDEX_NONE;
		if (TagPath.FindLastChar(TEXT('.'), LastDotIndex) && LastDotIndex + 1 < TagPath.Len())
		{
			return TagPath.Mid(LastDotIndex + 1);
		}

		return TagPath;
	}

	static const FTableRowStyle& GetConversationBandHeaderRowStyle()
	{
		static const FSlateColorBrush TransparentBrush(FLinearColor::Transparent);
		static const FSlateNoResource NoResourceBrush;
		static const FTableRowStyle RowStyle = []
		{
			FTableRowStyle Style = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
			Style.SetEvenRowBackgroundBrush(TransparentBrush);
			Style.SetEvenRowBackgroundHoveredBrush(TransparentBrush);
			Style.SetOddRowBackgroundBrush(TransparentBrush);
			Style.SetOddRowBackgroundHoveredBrush(TransparentBrush);
			Style.SetSelectorFocusedBrush(NoResourceBrush);
			Style.SetActiveBrush(NoResourceBrush);
			Style.SetActiveHoveredBrush(NoResourceBrush);
			Style.SetInactiveBrush(NoResourceBrush);
			Style.SetInactiveHoveredBrush(NoResourceBrush);
			return Style;
		}();
		return RowStyle;
	}

	class FConversationBandDragDropOp final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FConversationBandDragDropOp, FDecoratedDragDropOp)

		static TSharedRef<FConversationBandDragDropOp> New(
			const TWeakObjectPtr<UParleyConversationAsset> InConversationAsset,
			const int32 InSourceBand,
			const FString& InLabel)
		{
			TSharedRef<FConversationBandDragDropOp> Operation = MakeShareable(new FConversationBandDragDropOp);
			Operation->ConversationAsset = InConversationAsset;
			Operation->SourceBand = InSourceBand;
			Operation->DefaultHoverText = FText::FromString(InLabel);
			Operation->CurrentHoverText = Operation->DefaultHoverText;
			Operation->Construct();
			return Operation;
		}

		TWeakObjectPtr<UParleyConversationAsset> ConversationAsset;
		int32 SourceBand = 0;
	};

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
				return SNew(STextBlock).Text(FText::FromString(GetTagLeafLabel(Item->Row.SpeakerTag)));
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

	static UParleyDialogueSubsystem* GetDialogueSubsystemFromPIESpeakerPanel()
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

		return PIEContext->OwningGameInstance->GetSubsystem<UParleyDialogueSubsystem>();
	}

	static UParleyDialogueSubsystem* GetTransientDialogueValidationSubsystem()
	{
		TStrongObjectPtr<UParleyDialogueSubsystem>& CachedValidationSubsystem = GetCachedValidationDialogueSubsystem();
		if (!CachedValidationSubsystem.IsValid())
		{
			UParleyDialogueSubsystem* ValidationSubsystem = NewObject<UParleyDialogueSubsystem>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!ValidationSubsystem)
			{
				return nullptr;
			}

			// Validation helpers only need dialogue subsystem logic plus config lookup fallback paths, not a live GameInstance.
			CachedValidationSubsystem.Reset(ValidationSubsystem);
		}

		return CachedValidationSubsystem.Get();
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

	static FString GetSpeakerAssetNameSegment(const FParleySpeakerRow& SpeakerRow, const FName SpeakerRowName)
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

	static bool RemoveTagFromConditionGroup(FDialogueConditionGroup& Group, const TSet<FGameplayTag>& RemovedTags)
	{
		if (RemovedTags.IsEmpty() || Group.Conditions.IsEmpty())
		{
			return false;
		}

		const int32 OriginalNum = Group.Conditions.Num();
		Group.Conditions.RemoveAll([&RemovedTags](const FDialogueCondition& Condition)
		{
			return Condition.TagValue.IsValid() && RemovedTags.Contains(Condition.TagValue);
		});
		return Group.Conditions.Num() != OriginalNum;
	}

	static bool RemoveConversationTagReferencesFromConversation(
		UParleyConversationAsset* Conversation,
		const TSet<FGameplayTag>& RemovedTags)
	{
		if (!Conversation || RemovedTags.IsEmpty())
		{
			return false;
		}

		Conversation->Modify();
		bool bChanged = false;
		bChanged |= RemoveTagFromConditionGroup(Conversation->Header.LockedConditions, RemovedTags);
		bChanged |= RemoveTagFromConditionGroup(Conversation->Header.BlockedConditions, RemovedTags);

		for (FDialogueCompiledNode& Node : Conversation->CompiledData.Nodes)
		{
			switch (Node.NodeType)
			{
			case EDialogueNodeType::Line:
			{
				if (FDialogueLineNodeData* LineData = Node.NodeData.GetMutablePtr<FDialogueLineNodeData>())
				{
					bChanged |= RemoveTagFromConditionGroup(LineData->SkipLockedConditions, RemovedTags);
					bChanged |= RemoveTagFromConditionGroup(LineData->SkipBlockedConditions, RemovedTags);
				}
				break;
			}
			case EDialogueNodeType::MultiLine:
			case EDialogueNodeType::SplitLine:
			{
				if (FDialogueMultiLineNodeData* MultiLineData = Node.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>())
				{
					for (FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
					{
						bChanged |= RemoveTagFromConditionGroup(Entry.LineData.SkipLockedConditions, RemovedTags);
						bChanged |= RemoveTagFromConditionGroup(Entry.LineData.SkipBlockedConditions, RemovedTags);
					}
				}
				break;
			}
			default:
				break;
			}

			for (FDialogueCompiledChoiceBranch& Branch : Node.ChoiceBranches)
			{
				bChanged |= RemoveTagFromConditionGroup(Branch.LockedConditions, RemovedTags);
				bChanged |= RemoveTagFromConditionGroup(Branch.BlockedConditions, RemovedTags);
			}
			for (FDialogueCompiledSwitchBranch& Branch : Node.SwitchBranches)
			{
				bChanged |= RemoveTagFromConditionGroup(Branch.LockedConditions, RemovedTags);
				bChanged |= RemoveTagFromConditionGroup(Branch.BlockedConditions, RemovedTags);
			}
		}

		if (bChanged)
		{
			Conversation->MarkPackageDirty();
		}

		return bChanged;
	}

	static UParleyConversationAsset* TryLoadConversationAssetFromAssetData(const FAssetData& AssetData)
	{
		if (UParleyConversationAsset* LoadedAsset = Cast<UParleyConversationAsset>(AssetData.GetAsset()))
		{
			return LoadedAsset;
		}

		return Cast<UParleyConversationAsset>(AssetData.ToSoftObjectPath().TryLoad());
	}

	static FName BuildLookupRowNameForConversationAsset(const UParleyConversationAsset* Conversation)
	{
		if (!Conversation)
		{
			return NAME_None;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(Conversation->GetPathName());
		return AssetName.IsEmpty() ? Conversation->GetFName() : FName(*AssetName);
	}

	static void GatherConversationAssetsFromRegistry(
		const UParleyDialogueSettings* DialogueSettings,
		TMap<FGameplayTag, UParleyConversationAsset*>& OutConversationsByTag)
	{
		if (!DialogueSettings || !DialogueSettings->ConversationDefinitionRootTag.IsValid())
		{
			return;
		}

		FARFilter ConversationAssetFilter;
		ConversationAssetFilter.ClassPaths.Add(UParleyConversationAsset::StaticClass()->GetClassPathName());
		ConversationAssetFilter.bRecursiveClasses = true;
		ConversationAssetFilter.PackagePaths.Add(TEXT("/Game"));
		ConversationAssetFilter.bRecursivePaths = true;

		TArray<FAssetData> ConversationAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().GetAssets(ConversationAssetFilter, ConversationAssetData);

		for (const FAssetData& AssetData : ConversationAssetData)
		{
			UParleyConversationAsset* Conversation = TryLoadConversationAssetFromAssetData(AssetData);
			if (!Conversation)
			{
				continue;
			}

			FGameplayTag AssetConversationTag = Conversation->Header.ConversationTag;
			if (!AssetConversationTag.IsValid())
			{
				const FString AssetName = AssetData.AssetName.ToString();
				if (!AssetName.IsEmpty())
				{
					const FString BuiltTagPath = FString::Printf(TEXT("%s.%s"), *DialogueSettings->ConversationDefinitionRootTag.ToString(), *AssetName);
					AssetConversationTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*BuiltTagPath), false);
				}
			}

			if (!AssetConversationTag.IsValid() || OutConversationsByTag.Contains(AssetConversationTag))
			{
				continue;
			}

			OutConversationsByTag.Add(AssetConversationTag, Conversation);
		}
	}

	static void RepairConversationLookupTableFromRegistry(
		const UParleyDialogueSettings* DialogueSettings,
		UDataTable* ConversationTable,
		const TMap<FGameplayTag, UParleyConversationAsset*>& RegistryConversationsByTag)
	{
		if (!DialogueSettings
			|| !DialogueSettings->ConversationDefinitionRootTag.IsValid()
			|| !ConversationTable
			|| ConversationTable->GetRowStruct() != FParleyConversationAssetRow::StaticStruct()
			|| RegistryConversationsByTag.IsEmpty())
		{
			return;
		}

		bool bTableChanged = false;
		TSet<FName> ExistingRowNames;
		for (const FName ExistingRowName : ConversationTable->GetRowNames())
		{
			ExistingRowNames.Add(ExistingRowName);
		}

		TSet<FGameplayTag> TagsAlreadyRepresented;
		for (const FName RowName : ConversationTable->GetRowNames())
		{
			FParleyConversationAssetRow* MutableRow = ConversationTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorRepairRows"), false);
			if (!MutableRow)
			{
				continue;
			}

			UParleyConversationAsset* LoadedConversation = MutableRow->Conversation.LoadSynchronous();
			if (!LoadedConversation && MutableRow->ConversationTag.IsValid())
			{
				if (UParleyConversationAsset* RegistryConversation = RegistryConversationsByTag.FindRef(MutableRow->ConversationTag))
				{
					ConversationTable->Modify();
					MutableRow->Conversation = TSoftObjectPtr<UParleyConversationAsset>(RegistryConversation);
					LoadedConversation = RegistryConversation;
					bTableChanged = true;
				}
			}

			if (LoadedConversation)
			{
				const FGameplayTag EffectiveTag = LoadedConversation->Header.ConversationTag.IsValid()
					? LoadedConversation->Header.ConversationTag
					: MutableRow->ConversationTag;
				if (EffectiveTag.IsValid())
				{
					TagsAlreadyRepresented.Add(EffectiveTag);
					if (!MutableRow->ConversationTag.IsValid() || !MutableRow->ConversationTag.MatchesTagExact(EffectiveTag))
					{
						ConversationTable->Modify();
						MutableRow->ConversationTag = EffectiveTag;
						bTableChanged = true;
					}
				}
			}
		}

		for (const TPair<FGameplayTag, UParleyConversationAsset*>& Pair : RegistryConversationsByTag)
		{
			if (!Pair.Key.IsValid() || !Pair.Value || TagsAlreadyRepresented.Contains(Pair.Key))
			{
				continue;
			}

			FName DesiredRowName = BuildLookupRowNameForConversationAsset(Pair.Value);
			if (DesiredRowName.IsNone())
			{
				continue;
			}

			if (ExistingRowNames.Contains(DesiredRowName))
			{
				int32 Suffix = 1;
				FName CandidateRowName = DesiredRowName;
				while (ExistingRowNames.Contains(CandidateRowName))
				{
					CandidateRowName = FName(*FString::Printf(TEXT("%s_%d"), *DesiredRowName.ToString(), Suffix++));
				}
				DesiredRowName = CandidateRowName;
			}

			FParleyConversationAssetRow NewRow;
			NewRow.ConversationTag = Pair.Key;
			NewRow.Conversation = TSoftObjectPtr<UParleyConversationAsset>(Pair.Value);
			ConversationTable->Modify();
			ConversationTable->AddRow(DesiredRowName, NewRow);
			ExistingRowNames.Add(DesiredRowName);
			TagsAlreadyRepresented.Add(Pair.Key);
			bTableChanged = true;
		}

		if (bTableChanged)
		{
			ConversationTable->MarkPackageDirty();
		}
	}

	static void GatherConversationAssetsFromLookup(const UParleyDialogueSettings* DialogueSettings, TMap<FGameplayTag, UParleyConversationAsset*>& OutConversationsByTag)
	{
		OutConversationsByTag.Reset();
		if (!DialogueSettings || !DialogueSettings->ConversationDefinitionRootTag.IsValid())
		{
			return;
		}

		auto TryAddConversation = [&OutConversationsByTag](UParleyConversationAsset* Conversation, const FGameplayTag& SuggestedTag)
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

		TMap<FGameplayTag, UParleyConversationAsset*> RegistryConversationsByTag;
		GatherConversationAssetsFromRegistry(DialogueSettings, RegistryConversationsByTag);

		UDataTable* ConversationTable = nullptr;
		FString LookupError;
		if (FTagKeyEditorHelpers::TryResolveDataTableForRootTag(DialogueSettings->ConversationDefinitionRootTag, ConversationTable, LookupError)
			&& ConversationTable
			&& ConversationTable->GetRowStruct() == FParleyConversationAssetRow::StaticStruct())
		{
			for (const FName RowName : ConversationTable->GetRowNames())
			{
				const FParleyConversationAssetRow* Row = ConversationTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorConversations"), false);
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

					FParleyConversationAssetRow* MutableRow = ConversationTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorConversations"), false);
					if (MutableRow && !MutableRow->ConversationTag.IsValid())
					{
						ConversationTable->Modify();
						MutableRow->ConversationTag = RowTag;
						ConversationTable->MarkPackageDirty();
					}
				}

				UParleyConversationAsset* Conversation = Row->Conversation.LoadSynchronous();
				if (!Conversation)
				{
					if (RowTag.IsValid())
					{
						if (UParleyConversationAsset* RegistryConversation = RegistryConversationsByTag.FindRef(RowTag))
						{
							FParleyConversationAssetRow* MutableRow = ConversationTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorConversations"), false);
							if (MutableRow)
							{
								ConversationTable->Modify();
								MutableRow->Conversation = TSoftObjectPtr<UParleyConversationAsset>(RegistryConversation);
								ConversationTable->MarkPackageDirty();
								Conversation = RegistryConversation;
							}
						}
					}
				}

				if (!Conversation)
				{
					continue;
				}

				if (Conversation && !Conversation->Header.ConversationTag.IsValid() && RowTag.IsValid())
				{
					Conversation->Modify();
					Conversation->Header.ConversationTag = RowTag;
					Conversation->MarkPackageDirty();
				}

				TryAddConversation(Conversation, RowTag);
			}

			RepairConversationLookupTableFromRegistry(DialogueSettings, ConversationTable, RegistryConversationsByTag);
		}

		for (const TPair<FGameplayTag, UParleyConversationAsset*>& Pair : RegistryConversationsByTag)
		{
			TryAddConversation(Pair.Value, Pair.Key);
		}
	}

	static FString BuildConversationEntryLabel(const UParleyConversationAsset* Conversation)
	{
		if (!Conversation)
		{
			return FString();
		}

		const FString DisplayTitle = Conversation->Header.DisplayTitle.ToString().TrimStartAndEnd();
		if (!DisplayTitle.IsEmpty())
		{
			return DisplayTitle;
		}

		return FPackageName::GetLongPackageAssetName(Conversation->GetPathName());
	}

	static bool EnsureConversationDisplayTitle(UParleyConversationAsset* Conversation)
	{
		if (!Conversation)
		{
			return false;
		}

		if (!Conversation->Header.DisplayTitle.IsEmpty())
		{
			return true;
		}

		const FString FallbackTitle = FPackageName::GetLongPackageAssetName(Conversation->GetPathName());
		if (FallbackTitle.IsEmpty())
		{
			return false;
		}

		Conversation->Modify();
		Conversation->Header.DisplayTitle = FText::FromString(FallbackTitle);
		Conversation->MarkPackageDirty();
		return true;
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

	static bool IsEditableRequiredTagCondition(const FDialogueCondition& Condition)
	{
		return Condition.TagValue.IsValid()
			&& IsConditionTagSource(Condition.Source)
			&& DoesOperatorRequireTagPresence(Condition.Operator);
	}

	static FString BuildSpeakerConversationTagFilter(const UParleyDialogueSettings* Settings, const FGameplayTag SpeakerTag)
	{
		if (!Settings || !Settings->ConversationDefinitionRootTag.IsValid() || !SpeakerTag.IsValid())
		{
			return FString();
		}

		return FString::Printf(
			TEXT("%s.%s"),
			*Settings->ConversationDefinitionRootTag.ToString(),
			*GetSpeakerLeafSegment(SpeakerTag));
	}

	static bool IsTagWithinFilterRoot(const FGameplayTag Tag, const FString& FilterRoot)
	{
		if (!Tag.IsValid() || FilterRoot.IsEmpty())
		{
			return false;
		}

		const FString TagPath = Tag.ToString();
		return TagPath.Equals(FilterRoot, ESearchCase::CaseSensitive)
			|| TagPath.StartsWith(FilterRoot + TEXT("."), ESearchCase::CaseSensitive);
	}

	static void GatherLockedConversationTagsForRoot(
		const FDialogueConditionGroup& LockedConditions,
		const FString& FilterRoot,
		TArray<FGameplayTag>& OutTags)
	{
		OutTags.Reset();
		if (FilterRoot.IsEmpty())
		{
			return;
		}

		for (const FDialogueCondition& Condition : LockedConditions.Conditions)
		{
			if (!IsEditableRequiredTagCondition(Condition))
			{
				continue;
			}

			if (IsTagWithinFilterRoot(Condition.TagValue, FilterRoot))
			{
				OutTags.AddUnique(Condition.TagValue);
			}
		}
	}

	static void ReplaceLockedConversationTagsForRoot(
		FDialogueConditionGroup& LockedConditions,
		const FString& FilterRoot,
		const TArray<FGameplayTag>& NewTags)
	{
		TArray<FDialogueCondition> UpdatedConditions;
		UpdatedConditions.Reserve(LockedConditions.Conditions.Num() + NewTags.Num());
		for (const FDialogueCondition& Condition : LockedConditions.Conditions)
		{
			if (IsEditableRequiredTagCondition(Condition) && IsTagWithinFilterRoot(Condition.TagValue, FilterRoot))
			{
				continue;
			}

			UpdatedConditions.Add(Condition);
		}

		TSet<FGameplayTag> UniqueNewTags;
		for (const FGameplayTag Tag : NewTags)
		{
			if (!Tag.IsValid() || !IsTagWithinFilterRoot(Tag, FilterRoot) || UniqueNewTags.Contains(Tag))
			{
				continue;
			}

			UniqueNewTags.Add(Tag);
			FDialogueCondition& NewCondition = UpdatedConditions.AddDefaulted_GetRef();
			NewCondition.Source = EDialogueConditionSource::CombinedTags;
			NewCondition.Operator = EDialogueComparisonOp::Present;
			NewCondition.TagValue = Tag;
		}

		const bool bHasAnyRequired = UpdatedConditions.ContainsByPredicate([](const FDialogueCondition& Condition)
		{
			return IsEditableRequiredTagCondition(Condition);
		});

		LockedConditions.Conditions = MoveTemp(UpdatedConditions);
		if (bHasAnyRequired)
		{
			LockedConditions.MatchMode = EDialogueConditionMatchMode::All;
		}
	}

	static void GatherConversationChainTags(
		const UParleyConversationAsset* Conversation,
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

	static void GatherHeaderRequiredTags(
		const UParleyConversationAsset* Conversation,
		const FString& ExcludedTagRoot,
		TArray<FGameplayTag>& OutRequiredTags)
	{
		OutRequiredTags.Reset();
		if (!Conversation)
		{
			return;
		}

		TSet<FGameplayTag> UniqueRequiredTags;
		for (const FDialogueCondition& Condition : Conversation->Header.LockedConditions.Conditions)
		{
			if (!Condition.TagValue.IsValid())
			{
				continue;
			}
			if (!IsConditionTagSource(Condition.Source))
			{
				continue;
			}
			if (!DoesOperatorRequireTagPresence(Condition.Operator))
			{
				continue;
			}
			if (!ExcludedTagRoot.IsEmpty() && IsTagWithinFilterRoot(Condition.TagValue, ExcludedTagRoot))
			{
				continue;
			}

			UniqueRequiredTags.Add(Condition.TagValue);
		}

		OutRequiredTags = UniqueRequiredTags.Array();
		OutRequiredTags.Sort([](const FGameplayTag& Lhs, const FGameplayTag& Rhs)
		{
			return Lhs.ToString() < Rhs.ToString();
		});
	}

}

void SDialogueSpeakerEditorPanel::ResetValidationSubsystemCache()
{
	GetCachedValidationDialogueSubsystem().Reset();
}

void SDialogueSpeakerEditorPanel::Construct(const FArguments& InArgs)
{
	(void)InArgs;
	RefreshData();
	const float DetailFieldSpacing = 6.0f;
	const float DetailLabelToFieldSpacing = 3.0f;
	const float PortraitPreviewSize = 72.0f;

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
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailFieldSpacing)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f).VAlign(VAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(PortraitPreviewSize)
								.HeightOverride(PortraitPreviewSize)
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
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailLabelToFieldSpacing)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Name")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailFieldSpacing)
					[
						SAssignNew(DisplayNameTextBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Display Name")))
						.OnTextCommitted_Lambda([this](const FText&, const ETextCommit::Type CommitType)
						{
							if (CommitType == ETextCommit::OnEnter)
							{
								HandleSaveSpeaker();
							}
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailLabelToFieldSpacing)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Description")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailFieldSpacing)
					[
						SAssignNew(DescriptionTextBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Description")))
						.OnTextCommitted_Lambda([this](const FText&, const ETextCommit::Type CommitType)
						{
							if (CommitType == ETextCommit::OnEnter)
							{
								HandleSaveSpeaker();
							}
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailLabelToFieldSpacing)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Line Font (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailFieldSpacing)
					[
						SAssignNew(LineFontPicker, SObjectPropertyEntryBox)
						.AllowedClass(UFont::StaticClass())
						.ObjectPath(this, &SDialogueSpeakerEditorPanel::GetEditedLineFontPath)
						.OnObjectChanged(this, &SDialogueSpeakerEditorPanel::OnEditedLineFontChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailLabelToFieldSpacing)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Speaker Tag")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailFieldSpacing)
					[
						SNew(SGameplayTagCombo)
						.Filter(TEXT("Parley.Speaker"))
						.Tag(this, &SDialogueSpeakerEditorPanel::GetEditedSpeakerTag)
						.OnTagChanged(this, &SDialogueSpeakerEditorPanel::OnEditedSpeakerTagChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailLabelToFieldSpacing)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Faction Tag (optional)")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, DetailFieldSpacing)
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
								.Text(FText::FromString(TEXT("No emotions yet. Use Add New below to create one.")))
								.Visibility_Lambda([this]() { return PortraitEntries.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Emotion Tag")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SAssignNew(EmotionTagComboHost, SBox)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Emotion Portrait Texture")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
							[
								SAssignNew(PortraitTexturePicker, SObjectPropertyEntryBox)
								.AllowedClass(UTexture2D::StaticClass())
								.DisplayThumbnail(true)
								.ObjectPath(this, &SDialogueSpeakerEditorPanel::GetEditedPortraitTexturePath)
								.OnObjectChanged(this, &SDialogueSpeakerEditorPanel::OnEditedPortraitTextureChanged)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.Text(FText::FromString(TEXT("Add New")))
									.ToolTipText(FText::FromString(TEXT("Create a new emotion portrait entry from the current Emotion Tag + Texture fields.")))
									.OnClicked(this, &SDialogueSpeakerEditorPanel::HandleCreateEmotionFromFields)
									.IsEnabled_Lambda([this]() { return CanCreateEmotionFromFields(); })
								]
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
								SNew(STextBlock).Text(FText::FromString(TEXT("Relationship Levels")))
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
			]
			+ SSplitter::Slot().Value(0.22f)
			[
				SNew(SBorder)
				.Padding(6.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Conversation Map")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FMargin(8.0f, 4.0f))
							.OnClicked(this, &SDialogueSpeakerEditorPanel::HandleCreateConversation)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("New")))
								.Justification(ETextJustify::Center)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f, 2.0f, 0.0f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FMargin(8.0f, 4.0f))
							.OnClicked(this, &SDialogueSpeakerEditorPanel::HandleFindBrokenConversations)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Find Broken")))
								.Justification(ETextJustify::Center)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FMargin(8.0f, 4.0f))
							.OnClicked(this, &SDialogueSpeakerEditorPanel::HandleCleanupGeneratedConversationTags)
							.ToolTipText(FText::FromString(TEXT("Explicit cleanup pass for generated conversation gameplay tags. Removes stale generated tags no longer referenced by lookup rows/conversation headers.")))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Cleanup Tags")))
								.Justification(ETextJustify::Center)
							]
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
							.OnContextMenuOpening(this, &SDialogueSpeakerEditorPanel::BuildConversationListContextMenu)
							.OnSelectionChanged(this, &SDialogueSpeakerEditorPanel::OnConversationSelectionChanged)
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
								if (Entry.IsValid() && !Entry->bIsBandHeader && !Entry->bIsLevelZeroDropTarget && Entry->Asset.IsValid())
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
	if (!InKeyEvent.IsControlDown()
		&& !InKeyEvent.IsCommandDown()
		&& !InKeyEvent.IsAltDown()
		&& InKeyEvent.GetKey() == EKeys::Enter)
	{
		HandleSaveSpeaker();
		return FReply::Handled();
	}

	if ((InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown()) && !InKeyEvent.IsAltDown())
	{
		if (!InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::Z)
		{
			if (GEditor && GEditor->UndoTransaction())
			{
				RefreshData();
				SetSelectedSpeakerRow(SelectedSpeakerRowName);
			}
			return FReply::Handled();
		}

		if (InKeyEvent.GetKey() == EKeys::Y || (InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::Z))
		{
			if (GEditor && GEditor->RedoTransaction())
			{
				RefreshData();
				SetSelectedSpeakerRow(SelectedSpeakerRowName);
			}
			return FReply::Handled();
		}
	}

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

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
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

	if (!FTagKeyEditorHelpers::TryResolveDataTableForRootTag(DialogueSettings->SpeakerDefinitionRootTag, OutTable, OutError))
	{
		return false;
	}

	if (OutTable->GetRowStruct() != FParleySpeakerRow::StaticStruct())
	{
		OutError = FString::Printf(TEXT("Speaker data table row struct mismatch. Expected '%s', got '%s'."),
			*FParleySpeakerRow::StaticStruct()->GetName(),
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
	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();

	TMap<FGameplayTag, int32> ConversationCountBySpeaker;
	if (DialogueSettings)
	{
		TMap<FGameplayTag, UParleyConversationAsset*> ConversationsByTag;
		GatherConversationAssetsFromLookup(DialogueSettings, ConversationsByTag);
		for (const TPair<FGameplayTag, UParleyConversationAsset*>& Pair : ConversationsByTag)
		{
			UParleyConversationAsset* Conversation = Pair.Value;
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
		const FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(RowName, TEXT("DialogueSpeakerEditor"), false);
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

	const TArray<float> ThresholdsForBandCalc = GetActiveThresholdsForConversationMap();

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings)
	{
		if (ConversationListView.IsValid())
		{
			ConversationListView->RequestListRefresh();
		}
		return;
	}

	const FString SelectedSpeakerConversationTagFilter = BuildSpeakerConversationTagFilter(Settings, SelectedSpeaker->Row.SpeakerTag);

	TMap<FGameplayTag, UParleyConversationAsset*> ConversationsByTag;
	GatherConversationAssetsFromLookup(Settings, ConversationsByTag);

	TSet<FGameplayTag> CompletedConversationTagsByGame;
	if (GEditor)
	{
		const FWorldContext* PIEContext = GEditor->GetPIEWorldContext();
		if (PIEContext && PIEContext->OwningGameInstance)
		{
			if (UParleyDialogueSubsystem* DialogueSubsystem = PIEContext->OwningGameInstance->GetSubsystem<UParleyDialogueSubsystem>())
			{
				FGameplayTagContainer CompletedTags;
				DialogueSubsystem->GetCompletedConversationTagsByGame(CompletedTags);
				for (const FGameplayTag CompletedTag : CompletedTags)
				{
					if (CompletedTag.IsValid())
					{
						CompletedConversationTagsByGame.Add(CompletedTag);
					}
				}
			}
		}
	}

	struct FConversationChainData
	{
		TSet<FGameplayTag> RequiredTags;
		TSet<FGameplayTag> AddedTags;
		TSet<FGameplayTag> RemovedTags;
	};

	TArray<TSharedPtr<FConversationEntry>> RawConversationEntries;
	TMap<const UParleyConversationAsset*, FConversationChainData> ChainDataByConversation;
	for (const TPair<FGameplayTag, UParleyConversationAsset*>& Pair : ConversationsByTag)
	{
		UParleyConversationAsset* Conversation = Pair.Value;
		if (!Conversation || !Conversation->Header.PrimarySpeakerTag.MatchesTagExact(SelectedSpeaker->Row.SpeakerTag))
		{
			continue;
		}

		EnsureConversationDisplayTitle(Conversation);

		TSharedPtr<FConversationEntry> Entry = MakeShared<FConversationEntry>();
		Entry->Asset = Conversation;
		Entry->DisplayTitle = Conversation->Header.DisplayTitle.ToString();
		Entry->ConversationTag = Conversation->Header.ConversationTag;
		Entry->PrimarySpeakerTag = Conversation->Header.PrimarySpeakerTag;
		Entry->Priority = Conversation->Header.Priority;
		Entry->MinimumRelationshipPoints = Conversation->Header.MinimumRelationshipPoints;
		Entry->bImportant = Conversation->Header.bImportant;
		Entry->bRepeatable = Conversation->Header.bRepeatable;
		Entry->bCompletedByGame = Entry->ConversationTag.IsValid() && CompletedConversationTagsByGame.Contains(Entry->ConversationTag);
		Entry->LockedConditionCount = Conversation->Header.LockedConditions.Conditions.Num();
		Entry->BlockedConditionCount = Conversation->Header.BlockedConditions.Conditions.Num();
		Entry->bSeenByGameBlocksReoffer = Conversation->Header.bSeenByGameBlocksReoffer;
		Entry->bSeenByPlayerBlocksReoffer = Conversation->Header.bSeenByPlayerBlocksReoffer;
		Entry->bCompletedByGameBlocksReoffer = Conversation->Header.bCompletedByGameBlocksReoffer;
		Entry->RelationshipBand = ComputeRelationshipBand(Entry->MinimumRelationshipPoints, ThresholdsForBandCalc);
		Entry->RelationshipBandLabel = FString::Printf(TEXT("Level %d"), Entry->RelationshipBand);
		Entry->Label = BuildConversationEntryLabel(Conversation);

		Entry->ConversationTagFilter = SelectedSpeakerConversationTagFilter;
		TArray<FGameplayTag> HeaderRequiredTags;
		GatherHeaderRequiredTags(Conversation, Entry->ConversationTagFilter, HeaderRequiredTags);
		for (const FGameplayTag RequiredTag : HeaderRequiredTags)
		{
			if (RequiredTag.IsValid())
			{
				Entry->RequiredTags.AddTag(RequiredTag);
			}
		}
		Entry->RequiredTagCount = HeaderRequiredTags.Num();
		GatherLockedConversationTagsForRoot(Conversation->Header.LockedConditions, Entry->ConversationTagFilter, Entry->LockedByConversationTags);

		FConversationChainData ChainData;
		GatherConversationChainTags(
			Conversation,
			ChainData.RequiredTags,
			ChainData.AddedTags,
			ChainData.RemovedTags);
		Entry->AddedTagCount = ChainData.AddedTags.Num();
		Entry->RemovedTagCount = ChainData.RemovedTags.Num();
		Entry->RequiredTagCount = FMath::Max(Entry->RequiredTagCount, ChainData.RequiredTags.Num());

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

		TArray<FString> UnlocksConversationNames;
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
				UnlocksConversationNames.AddUnique(OtherEntry->DisplayTitle);
			}
		}

		UnlocksConversationNames.Sort();
		const FString UnlocksSummary = UnlocksConversationNames.IsEmpty()
			? TEXT("-")
			: FString::Join(UnlocksConversationNames, TEXT(", "));
		Entry->UnlocksSummary = UnlocksSummary;
	}

	ConversationEntries.Reset();
	const int32 LevelCount = FMath::Max(1, ThresholdsForBandCalc.Num() + 1);
	TArray<TArray<TSharedPtr<FConversationEntry>>> EntriesByLevel;
	EntriesByLevel.SetNum(LevelCount);

	for (const TSharedPtr<FConversationEntry>& Entry : RawConversationEntries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		Entry->RelationshipBand = FMath::Clamp(Entry->RelationshipBand, 0, LevelCount - 1);
		Entry->RelationshipBandLabel = FString::Printf(TEXT("Level %d"), Entry->RelationshipBand);
		EntriesByLevel[Entry->RelationshipBand].Add(Entry);
	}

	auto SortEntriesForDisplay = [](TArray<TSharedPtr<FConversationEntry>>& Entries)
	{
		Entries.Sort([](const TSharedPtr<FConversationEntry>& Lhs, const TSharedPtr<FConversationEntry>& Rhs)
		{
			if (!Lhs.IsValid() || !Rhs.IsValid())
			{
				return Lhs.IsValid();
			}

			const int32 LhsEffectivePriority = (Lhs->bRepeatable && Lhs->bCompletedByGame)
				? TNumericLimits<int32>::Lowest()
				: Lhs->Priority;
			const int32 RhsEffectivePriority = (Rhs->bRepeatable && Rhs->bCompletedByGame)
				? TNumericLimits<int32>::Lowest()
				: Rhs->Priority;
			if (LhsEffectivePriority != RhsEffectivePriority)
			{
				return LhsEffectivePriority > RhsEffectivePriority;
			}

			return Lhs->DisplayTitle < Rhs->DisplayTitle;
		});
	};

	for (TArray<TSharedPtr<FConversationEntry>>& LevelEntries : EntriesByLevel)
	{
		SortEntriesForDisplay(LevelEntries);
	}

	// Level 0 intentionally has no header; cards appear at the top.
	if (EntriesByLevel[0].IsEmpty())
	{
		TSharedPtr<FConversationEntry> LevelZeroDropTarget = MakeShared<FConversationEntry>();
		LevelZeroDropTarget->bIsLevelZeroDropTarget = true;
		LevelZeroDropTarget->RelationshipBand = 0;
		LevelZeroDropTarget->DisplayTitle = TEXT("No Requirement (drop here)");
		ConversationEntries.Add(LevelZeroDropTarget);
	}

	for (const TSharedPtr<FConversationEntry>& Entry : EntriesByLevel[0])
	{
		ConversationEntries.Add(Entry);
	}

	for (int32 LevelIndex = 1; LevelIndex < LevelCount; ++LevelIndex)
	{
		TSharedPtr<FConversationEntry> LevelHeader = MakeShared<FConversationEntry>();
		LevelHeader->bIsBandHeader = true;
		LevelHeader->RelationshipBand = LevelIndex;
		LevelHeader->RelationshipBandLabel = FString::Printf(TEXT("Level %d"), LevelIndex);
		LevelHeader->DisplayTitle = FString::Printf(TEXT("Level %d (%d conversations)"), LevelIndex, EntriesByLevel[LevelIndex].Num());
		ConversationEntries.Add(LevelHeader);

		for (const TSharedPtr<FConversationEntry>& Entry : EntriesByLevel[LevelIndex])
		{
			ConversationEntries.Add(Entry);
		}
	}

	if (RenamingConversationAsset.IsValid())
	{
		const bool bRenameTargetStillVisible = ConversationEntries.ContainsByPredicate([this](const TSharedPtr<FConversationEntry>& Entry)
		{
			return Entry.IsValid() && !Entry->bIsBandHeader && !Entry->bIsLevelZeroDropTarget && Entry->Asset == RenamingConversationAsset;
		});
		if (!bRenameTargetStillVisible)
		{
			RenamingConversationAsset.Reset();
		}
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

	const FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

		const TGuardValue<bool> IgnoreSelectionChangedGuard(bIgnoreSpeakerSelectionChanged, true);
		SpeakerListView->ClearSelection();

		TSharedPtr<FSpeakerEntry> SelectedEntry;
		for (const TSharedPtr<FSpeakerEntry>& Entry : FilteredSpeakerEntries)
		{
			if (Entry.IsValid() && Entry->RowName == SelectedSpeakerRowName)
			{
				SelectedEntry = Entry;
				break;
			}
		}

		if (!SelectedEntry.IsValid() && FilteredSpeakerEntries.Num() > 0 && FilteredSpeakerEntries[0].IsValid())
		{
			SelectedEntry = FilteredSpeakerEntries[0];
			SelectedSpeakerRowName = SelectedEntry->RowName;
		}

		if (SelectedEntry.IsValid())
		{
			SpeakerListView->SetSelection(SelectedEntry);
			SpeakerListView->RequestScrollIntoView(SelectedEntry);
		}
	}
}

bool SDialogueSpeakerEditorPanel::ValidateConversationWithBestAvailable(UParleyConversationAsset* Conversation, FDialogueValidationReport& OutReport) const
{
	if (UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemFromPIESpeakerPanel())
	{
		return DialogueSubsystem->ValidateConversation(Conversation, OutReport);
	}

	if (UParleyDialogueSubsystem* TempValidationSubsystem = GetTransientDialogueValidationSubsystem())
	{
		return TempValidationSubsystem->ValidateConversation(Conversation, OutReport);
	}

	OutReport = FDialogueValidationReport();
	return false;
}

bool SDialogueSpeakerEditorPanel::ValidateSpeakerWithBestAvailable(const FParleySpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const
{
	if (UParleyDialogueSubsystem* DialogueSubsystem = GetDialogueSubsystemFromPIESpeakerPanel())
	{
		return DialogueSubsystem->ValidateSpeaker(SpeakerRow, OutReport);
	}

	if (UParleyDialogueSubsystem* TempValidationSubsystem = GetTransientDialogueValidationSubsystem())
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
		return TEXT("Level 0 (Base)");
	}

	const int32 ThresholdIndex = FMath::Clamp(BandIndex - 1, 0, Thresholds.Num() - 1);
	if (Thresholds.IsValidIndex(ThresholdIndex))
	{
		return FString::Printf(TEXT("Level %d (>= %.1f)"), BandIndex, Thresholds[ThresholdIndex]);
	}

	return FString::Printf(TEXT("Level %d"), BandIndex);
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

bool SDialogueSpeakerEditorPanel::TryFindConflictingSpeakerTagRow(const FGameplayTag& CandidateTag, FName& OutConflictingRowName) const
{
	OutConflictingRowName = NAME_None;
	if (!CandidateTag.IsValid())
	{
		return false;
	}

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable)
	{
		return false;
	}

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
	const FGameplayTag SpeakerRootTag = DialogueSettings ? DialogueSettings->SpeakerDefinitionRootTag : FGameplayTag();

	for (const FName RowName : SpeakerTable->GetRowNames())
	{
		if (RowName == SelectedSpeakerRowName)
		{
			continue;
		}

		const FParleySpeakerRow* ExistingRow = SpeakerTable->FindRow<FParleySpeakerRow>(RowName, TEXT("DialogueSpeakerEditor"), false);
		if (!ExistingRow)
		{
			continue;
		}

		FGameplayTag ExistingSpeakerTag = ExistingRow->SpeakerTag;
		if (!ExistingSpeakerTag.IsValid() && SpeakerRootTag.IsValid())
		{
			const FString BuiltPath = FString::Printf(TEXT("%s.%s"), *SpeakerRootTag.ToString(), *RowName.ToString());
			ExistingSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*BuiltPath), false);
		}

		if (ExistingSpeakerTag.IsValid() && ExistingSpeakerTag.MatchesTagExact(CandidateTag))
		{
			OutConflictingRowName = RowName;
			return true;
		}
	}

	return false;
}

bool SDialogueSpeakerEditorPanel::BuildEditedSpeakerRow(FParleySpeakerRow& OutRow, FString& OutError) const
{
	OutError.Empty();

	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		OutError = TEXT("No speaker selected.");
		return false;
	}

	const FParleySpeakerRow* CurrentRow = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!CurrentRow)
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

	FName ConflictingRowName = NAME_None;
	if (TryFindConflictingSpeakerTagRow(EditedSpeakerTag, ConflictingRowName))
	{
		OutError = FString::Printf(
			TEXT("Speaker Tag '%s' is already assigned to row '%s'."),
			*EditedSpeakerTag.ToString(),
			*ConflictingRowName.ToString());
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
	OutRow.LineFont = EditedLineFontAsset;
	OutRow.LineFontStyleTag = NAME_None;
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

	FParleySpeakerRow* MutableRow = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
	if (!MutableRow)
	{
		OutError = TEXT("Selected speaker row could not be resolved in table.");
		return false;
	}

	FParleySpeakerRow EditedRow;
	if (!BuildEditedSpeakerRow(EditedRow, OutError))
	{
		return false;
	}

	const FName CurrentRowName = SelectedSpeakerRowName;
	const FName DesiredRowName(*GetSpeakerLeafSegment(EditedRow.SpeakerTag));
	const bool bNeedsRename = !DesiredRowName.IsNone() && DesiredRowName != CurrentRowName;
	if (bNeedsRename && SpeakerTable->GetRowMap().Contains(DesiredRowName))
	{
		OutError = FString::Printf(
			TEXT("Cannot rename row to '%s' because that row already exists."),
			*DesiredRowName.ToString());
		return false;
	}

	SpeakerTable->Modify();
	if (bNeedsRename)
	{
		SpeakerTable->AddRow(DesiredRowName, EditedRow);
		SpeakerTable->RemoveRow(CurrentRowName);
		SelectedSpeakerRowName = DesiredRowName;
	}
	else
	{
		*MutableRow = EditedRow;
	}
	SpeakerTable->MarkPackageDirty();
	return true;
}

FGameplayTag SDialogueSpeakerEditorPanel::GetEditedSpeakerTag() const
{
	return EditedSpeakerTag;
}

void SDialogueSpeakerEditorPanel::OnEditedSpeakerTagChanged(FGameplayTag NewTag)
{
	if (EditedSpeakerTag.IsValid() && EditedSpeakerTag.MatchesTagExact(NewTag))
	{
		return;
	}

	EditedSpeakerTag = NewTag;
	EnsureSpeakerDefaultEmotionTag(NewTag);
	RebuildEmotionTagCombo();

	FName ConflictingRowName = NAME_None;
	if (TryFindConflictingSpeakerTagRow(NewTag, ConflictingRowName))
	{
		AppendLogLine(FString::Printf(
			TEXT("[WARN] Speaker Tag '%s' is already assigned to row '%s'. Save will be blocked until a unique tag is selected."),
			*NewTag.ToString(),
			*ConflictingRowName.ToString()));
	}

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

FString SDialogueSpeakerEditorPanel::GetEditedLineFontPath() const
{
	return EditedLineFontAsset.ToSoftObjectPath().ToString();
}

void SDialogueSpeakerEditorPanel::OnEditedLineFontChanged(const FAssetData& AssetData)
{
	EditedLineFontAsset = AssetData.IsValid()
		? TSoftObjectPtr<UFont>(AssetData.ToSoftObjectPath())
		: TSoftObjectPtr<UFont>();
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
		return TEXT("Parley.Speaker");
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
	FParleySpeakerRow SelectedRow;
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
		EditedLineFontAsset = TSoftObjectPtr<UFont>();
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
	OnEditedSpeakerTagChanged(SelectedRow.SpeakerTag);
	EditedFactionTag = SelectedRow.FactionTag;
	EditedLineFontAsset = SelectedRow.LineFont;
	EditedDefaultPortraitTexture = SelectedRow.DefaultPortrait.PortraitTexture;
	EditedPortraitTag = FGameplayTag();
	EditedPortraitTexture = TSoftObjectPtr<UTexture2D>();
	EditedRelationshipThresholds = SelectedRow.RelationshipThresholds;
	if (EditedRelationshipThresholds.IsEmpty())
	{
		EditedRelationshipThresholds = { 5.0f, 15.0f, 30.0f, 50.0f };
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
				SpeakerListView->RequestScrollIntoView(Entry);
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

TSharedPtr<SWidget> SDialogueSpeakerEditorPanel::BuildConversationListContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	const bool bHasSelectedConversation = ConversationListView.IsValid()
		&& ConversationListView->GetSelectedItems().ContainsByPredicate([](const TSharedPtr<FConversationEntry>& Entry)
		{
			return Entry.IsValid() && !Entry->bIsBandHeader && Entry->Asset.IsValid();
		});

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Open")),
		FText::FromString(TEXT("Open the selected conversation in the graph editor.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleOpenConversation(); }),
			FCanExecuteAction::CreateLambda([bHasSelectedConversation]() { return bHasSelectedConversation; })));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Rename")),
		FText::FromString(TEXT("Rename the selected conversation display title inline.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleRenameConversation(); }),
			FCanExecuteAction::CreateLambda([bHasSelectedConversation]() { return bHasSelectedConversation; })));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Duplicate")),
		FText::FromString(TEXT("Duplicate the selected conversation asset and register a new conversation tag/lookup row.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleDuplicateConversation(); }),
			FCanExecuteAction::CreateLambda([bHasSelectedConversation]() { return bHasSelectedConversation; })));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Remove From Lookup")),
		FText::FromString(TEXT("Remove the selected conversation from lookup and clean references to its conversation tag.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleDeleteConversation(); }),
			FCanExecuteAction::CreateLambda([bHasSelectedConversation]() { return bHasSelectedConversation; })));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Delete Asset + Remove From Lookup")),
		FText::FromString(TEXT("Delete the selected conversation asset from Content Browser and remove its lookup/tag references.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleDeleteConversationAndAsset(); }),
			FCanExecuteAction::CreateLambda([bHasSelectedConversation]() { return bHasSelectedConversation; })));

	return MenuBuilder.MakeWidget();
}

void SDialogueSpeakerEditorPanel::HandleCopySpeaker()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		return;
	}

	const FParleySpeakerRow* SourceRow = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	FParleySpeakerRow NewRow = SpeakerClipboardRow;
	NewRow.DisplayName = FText::FromString(NewRow.DisplayName.ToString() + TEXT(" Copy"));

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
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

	const bool bHasSelectedBand = EditedRelationshipThresholds.IsValidIndex(SelectedThresholdIndex);
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Edit Level Value")),
		FText::FromString(TEXT("Edit the selected relationship level value inline.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { BeginInlineThresholdEdit(SelectedThresholdIndex); }),
			FCanExecuteAction::CreateLambda([bHasSelectedBand]() { return bHasSelectedBand; })));
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Add Level")),
		FText::FromString(TEXT("Insert a new relationship level after the selected level.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleAddThreshold(); })));
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Remove Level")),
		FText::FromString(TEXT("Remove the selected relationship level.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleRemoveThreshold(); })));
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Move Up")),
		FText::FromString(TEXT("Move the selected relationship level earlier in the list.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleMoveThresholdUp(); })));
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Move Down")),
		FText::FromString(TEXT("Move the selected relationship level later in the list.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleMoveThresholdDown(); })));
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Reset Levels")),
		FText::FromString(TEXT("Reset relationship levels to the default values (5, 15, 30, 50).")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleResetThresholds(); })));
	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SDialogueSpeakerEditorPanel::BuildEmotionListContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Add New Emotion")),
		FText::FromString(TEXT("Create a new emotion entry from the current Emotion Tag + Texture fields.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleCreateEmotionFromFields(); }),
			FCanExecuteAction::CreateSP(this, &SDialogueSpeakerEditorPanel::CanCreateEmotionFromFields)));

	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Remove Emotion")),
		FText::FromString(TEXT("Remove the currently selected emotion entry.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { HandleRemovePortrait(); }),
			FCanExecuteAction::CreateLambda([this]()
			{
				return SelectedPortraitIndex != INDEX_NONE && PortraitEntries.IsValidIndex(SelectedPortraitIndex);
			})));

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
		AppendLogLine(FString::Printf(TEXT("Invalid level value '%s'."), *TextValue));
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

TArray<float> SDialogueSpeakerEditorPanel::GetActiveThresholdsForConversationMap() const
{
	if (!EditedRelationshipThresholds.IsEmpty())
	{
		return EditedRelationshipThresholds;
	}

	for (const TSharedPtr<FSpeakerEntry>& Entry : AllSpeakerEntries)
	{
		if (Entry.IsValid() && Entry->RowName == SelectedSpeakerRowName)
		{
			if (!Entry->Row.RelationshipThresholds.IsEmpty())
			{
				return Entry->Row.RelationshipThresholds;
			}
			break;
		}
	}

	return { 5.0f, 15.0f, 30.0f, 50.0f };
}

float SDialogueSpeakerEditorPanel::GetMinimumRelationshipForBand(const int32 BandIndex, const TArray<float>& Thresholds) const
{
	if (BandIndex <= 0 || Thresholds.IsEmpty())
	{
		return 0.0f;
	}

	const int32 ThresholdIndex = FMath::Clamp(BandIndex - 1, 0, Thresholds.Num() - 1);
	return Thresholds[ThresholdIndex];
}

FReply SDialogueSpeakerEditorPanel::HandleCycleConversationBand(TWeakObjectPtr<UParleyConversationAsset> ConversationAsset)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return FReply::Handled();
	}

	const TArray<float> Thresholds = GetActiveThresholdsForConversationMap();
	const int32 BandCount = FMath::Max(1, Thresholds.Num() + 1);
	const int32 CurrentBand = ComputeRelationshipBand(Conversation->Header.MinimumRelationshipPoints, Thresholds);
	const int32 NextBand = (CurrentBand + 1) % BandCount;

	Conversation->Modify();
	Conversation->Header.MinimumRelationshipPoints = GetMinimumRelationshipForBand(NextBand, Thresholds);
	Conversation->MarkPackageDirty();

	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleSetConversationBand(
	TWeakObjectPtr<UParleyConversationAsset> ConversationAsset,
	const int32 TargetBand)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return FReply::Handled();
	}

	const TArray<float> Thresholds = GetActiveThresholdsForConversationMap();
	const int32 BandCount = FMath::Max(1, Thresholds.Num() + 1);
	const int32 ClampedTargetBand = FMath::Clamp(TargetBand, 0, BandCount - 1);
	const float NewMinimumRelationshipPoints = GetMinimumRelationshipForBand(ClampedTargetBand, Thresholds);
	if (FMath::IsNearlyEqual(Conversation->Header.MinimumRelationshipPoints, NewMinimumRelationshipPoints))
	{
		return FReply::Handled();
	}

	Conversation->Modify();
	Conversation->Header.MinimumRelationshipPoints = NewMinimumRelationshipPoints;
	Conversation->MarkPackageDirty();

	RefreshConversationMap();
	return FReply::Handled();
}

void SDialogueSpeakerEditorPanel::CommitConversationPriority(
	TWeakObjectPtr<UParleyConversationAsset> ConversationAsset,
	const FText& NewText,
	const ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnCleared)
	{
		return;
	}

	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return;
	}

	const FString TextValue = NewText.ToString().TrimStartAndEnd();
	int32 ParsedPriority = 0;
	if (!LexTryParseString(ParsedPriority, *TextValue))
	{
		AppendLogLine(FString::Printf(TEXT("Invalid priority '%s'."), *TextValue));
		return;
	}

	Conversation->Modify();
	Conversation->Header.Priority = ParsedPriority;
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
}

void SDialogueSpeakerEditorPanel::CommitConversationDisplayName(
	TWeakObjectPtr<UParleyConversationAsset> ConversationAsset,
	const FText& NewText,
	const ETextCommit::Type CommitType)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return;
	}

	if (CommitType != ETextCommit::OnEnter)
	{
		if (CommitType == ETextCommit::OnUserMovedFocus || CommitType == ETextCommit::OnCleared || CommitType == ETextCommit::Default)
		{
			RenamingConversationAsset.Reset();
			RefreshConversationMap();
		}
		return;
	}

	const FString TrimmedTitle = NewText.ToString().TrimStartAndEnd();
	const FText OldTitle = Conversation->Header.DisplayTitle;
	const FText NewTitle = TrimmedTitle.IsEmpty()
		? FText::FromString(FPackageName::GetLongPackageAssetName(Conversation->GetPathName()))
		: FText::FromString(TrimmedTitle);

	if (OldTitle.EqualTo(NewTitle))
	{
		RenamingConversationAsset.Reset();
		RefreshConversationMap();
		return;
	}

	Conversation->Modify();
	Conversation->Header.DisplayTitle = NewTitle;
	Conversation->MarkPackageDirty();

	RenamingConversationAsset.Reset();
	RefreshConversationMap();
}

FReply SDialogueSpeakerEditorPanel::HandleToggleConversationRepeatable(TWeakObjectPtr<UParleyConversationAsset> ConversationAsset)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return FReply::Handled();
	}

	Conversation->Modify();
	Conversation->Header.bRepeatable = !Conversation->Header.bRepeatable;
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleToggleConversationImportant(TWeakObjectPtr<UParleyConversationAsset> ConversationAsset)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return FReply::Handled();
	}

	Conversation->Modify();
	Conversation->Header.bImportant = !Conversation->Header.bImportant;
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleAddConversationLockedByTag(TWeakObjectPtr<UParleyConversationAsset> ConversationAsset)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return FReply::Handled();
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FString FilterRoot = BuildSpeakerConversationTagFilter(Settings, Conversation->Header.PrimarySpeakerTag);
	if (FilterRoot.IsEmpty())
	{
		AppendLogLine(TEXT("Unable to add lock tag: conversation root filter could not be resolved."));
		return FReply::Handled();
	}

	TArray<FGameplayTag> LockedByTags;
	GatherLockedConversationTagsForRoot(Conversation->Header.LockedConditions, FilterRoot, LockedByTags);

	FGameplayTag NewLockTag;
	for (const TSharedPtr<FConversationEntry>& Entry : ConversationEntries)
	{
		if (!Entry.IsValid() || Entry->bIsBandHeader || !Entry->ConversationTag.IsValid())
		{
			continue;
		}

		if (!IsTagWithinFilterRoot(Entry->ConversationTag, FilterRoot))
		{
			continue;
		}

		NewLockTag = Entry->ConversationTag;
		break;
	}

	if (!NewLockTag.IsValid())
	{
		AppendLogLine(TEXT("No valid conversation tag exists under this speaker root to add as a lock."));
		return FReply::Handled();
	}

	LockedByTags.AddUnique(NewLockTag);
	Conversation->Modify();
	ReplaceLockedConversationTagsForRoot(Conversation->Header.LockedConditions, FilterRoot, LockedByTags);
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleRemoveConversationLockedByTag(
	TWeakObjectPtr<UParleyConversationAsset> ConversationAsset,
	const int32 TagIndex)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return FReply::Handled();
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FString FilterRoot = BuildSpeakerConversationTagFilter(Settings, Conversation->Header.PrimarySpeakerTag);
	if (FilterRoot.IsEmpty())
	{
		return FReply::Handled();
	}

	TArray<FGameplayTag> LockedByTags;
	GatherLockedConversationTagsForRoot(Conversation->Header.LockedConditions, FilterRoot, LockedByTags);
	if (!LockedByTags.IsValidIndex(TagIndex))
	{
		return FReply::Handled();
	}

	LockedByTags.RemoveAt(TagIndex);
	Conversation->Modify();
	ReplaceLockedConversationTagsForRoot(Conversation->Header.LockedConditions, FilterRoot, LockedByTags);
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
	return FReply::Handled();
}

void SDialogueSpeakerEditorPanel::HandleConversationLockedByTagChanged(
	TWeakObjectPtr<UParleyConversationAsset> ConversationAsset,
	const int32 TagIndex,
	const FGameplayTag NewTag)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return;
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FString FilterRoot = BuildSpeakerConversationTagFilter(Settings, Conversation->Header.PrimarySpeakerTag);
	if (FilterRoot.IsEmpty())
	{
		return;
	}

	if (NewTag.IsValid() && !IsTagWithinFilterRoot(NewTag, FilterRoot))
	{
		AppendLogLine(FString::Printf(
			TEXT("Ignored lock tag '%s': must be under '%s'."),
			*NewTag.ToString(),
			*FilterRoot));
		return;
	}

	TArray<FGameplayTag> LockedByTags;
	GatherLockedConversationTagsForRoot(Conversation->Header.LockedConditions, FilterRoot, LockedByTags);
	if (!LockedByTags.IsValidIndex(TagIndex))
	{
		return;
	}

	if (NewTag.IsValid())
	{
		LockedByTags[TagIndex] = NewTag;
	}
	else
	{
		LockedByTags.RemoveAt(TagIndex);
	}

	Conversation->Modify();
	ReplaceLockedConversationTagsForRoot(Conversation->Header.LockedConditions, FilterRoot, LockedByTags);
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
}

void SDialogueSpeakerEditorPanel::CommitConversationRequiredTags(
	TWeakObjectPtr<UParleyConversationAsset> ConversationAsset,
	const FGameplayTagContainer& NewTags)
{
	UParleyConversationAsset* Conversation = ConversationAsset.Get();
	if (!Conversation)
	{
		return;
	}

	TSet<FGameplayTag> UniqueTags;
	TArray<FGameplayTag> ParsedTags;
	NewTags.GetGameplayTagArray(ParsedTags);
	for (int32 TagIndex = ParsedTags.Num() - 1; TagIndex >= 0; --TagIndex)
	{
		const FGameplayTag ParsedTag = ParsedTags[TagIndex];
		if (!ParsedTag.IsValid())
		{
			ParsedTags.RemoveAt(TagIndex);
			continue;
		}

		if (UniqueTags.Contains(ParsedTag))
		{
			ParsedTags.RemoveAt(TagIndex);
		}
		else
		{
			UniqueTags.Add(ParsedTag);
		}
	}
	ParsedTags.Sort([](const FGameplayTag& Lhs, const FGameplayTag& Rhs)
	{
		return Lhs.ToString() < Rhs.ToString();
	});

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	const FString LockTagFilterRoot = BuildSpeakerConversationTagFilter(Settings, Conversation->Header.PrimarySpeakerTag);

	TArray<FDialogueCondition> ExistingNonRequiredConditions;
	ExistingNonRequiredConditions.Reserve(Conversation->Header.LockedConditions.Conditions.Num());
	for (const FDialogueCondition& Condition : Conversation->Header.LockedConditions.Conditions)
	{
		const bool bIsEditableRequired = IsEditableRequiredTagCondition(Condition);
		const bool bIsSpeakerLockTag = bIsEditableRequired
			&& !LockTagFilterRoot.IsEmpty()
			&& IsTagWithinFilterRoot(Condition.TagValue, LockTagFilterRoot);
		if (!bIsEditableRequired || bIsSpeakerLockTag)
		{
			ExistingNonRequiredConditions.Add(Condition);
		}
	}

	TArray<FDialogueCondition> UpdatedLockedConditions = ExistingNonRequiredConditions;
	UpdatedLockedConditions.Reserve(ExistingNonRequiredConditions.Num() + ParsedTags.Num());
	for (const FGameplayTag RequiredTag : ParsedTags)
	{
		FDialogueCondition& NewCondition = UpdatedLockedConditions.AddDefaulted_GetRef();
		NewCondition.Source = EDialogueConditionSource::CombinedTags;
		NewCondition.Operator = EDialogueComparisonOp::Present;
		NewCondition.TagValue = RequiredTag;
	}

	const bool bSameCount = Conversation->Header.LockedConditions.Conditions.Num() == UpdatedLockedConditions.Num();
	bool bAnyConditionDiffers = !bSameCount;
	if (!bAnyConditionDiffers)
	{
		for (int32 Index = 0; Index < UpdatedLockedConditions.Num(); ++Index)
		{
			const FDialogueCondition& Existing = Conversation->Header.LockedConditions.Conditions[Index];
			const FDialogueCondition& Updated = UpdatedLockedConditions[Index];
			if (Existing.Source != Updated.Source
				|| Existing.Operator != Updated.Operator
				|| !Existing.TagValue.MatchesTagExact(Updated.TagValue))
			{
				bAnyConditionDiffers = true;
				break;
			}
		}
	}

	const EDialogueConditionMatchMode PreviousMatchMode = Conversation->Header.LockedConditions.MatchMode;
	const EDialogueConditionMatchMode UpdatedMatchMode = ParsedTags.IsEmpty()
		? PreviousMatchMode
		: EDialogueConditionMatchMode::All;

	if (!bAnyConditionDiffers && PreviousMatchMode == UpdatedMatchMode)
	{
		return;
	}

	Conversation->Modify();
	Conversation->Header.LockedConditions.Conditions = MoveTemp(UpdatedLockedConditions);
	if (!ParsedTags.IsEmpty())
	{
		Conversation->Header.LockedConditions.MatchMode = UpdatedMatchMode;
	}
	Conversation->MarkPackageDirty();
	RefreshConversationMap();
}

FReply SDialogueSpeakerEditorPanel::HandleThresholdListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;
	if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::F2)
	{
		BeginInlineThresholdEdit(SelectedThresholdIndex);
		return FReply::Handled();
	}

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
	DefaultPortraitFieldBrush.ImageSize = FVector2D(72.0f, 72.0f);
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

	FName NewRowName(TEXT("NewSpeaker"));
	for (int32 Suffix = 1; SpeakerTable->GetRowMap().Contains(NewRowName); ++Suffix)
	{
		NewRowName = FName(*FString::Printf(TEXT("NewSpeaker_%d"), Suffix));
	}

	FParleySpeakerRow NewRow;
	NewRow.DisplayName = FText::FromString(TEXT("New Speaker"));
	NewRow.Description = FText::GetEmpty();
	NewRow.RelationshipThresholds = { 5.0f, 15.0f, 30.0f, 50.0f };

	SpeakerTable->Modify();
	SpeakerTable->AddRow(NewRowName, NewRow);
	SpeakerTable->MarkPackageDirty();

	AppendLogLine(FString::Printf(TEXT("Created speaker row '%s'. Pick an unused Speaker Tag, then Save."), *NewRowName.ToString()));
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

	const FParleySpeakerRow* SourceRow = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	FParleySpeakerRow NewRow = *SourceRow;
	NewRow.DisplayName = FText::FromString(NewRow.DisplayName.ToString() + TEXT(" Copy"));

	const UParleyDialogueSettings* DialogueSettings = GetDefault<UParleyDialogueSettings>();
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
	FParleySpeakerRow EditedRow;
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
	FParleySpeakerRow EditedRow;
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

	FParleySpeakerRow* SpeakerRow = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings || !Settings->ConversationDefinitionRootTag.IsValid())
	{
		AppendLogLine(TEXT("Dialogue settings/content lookup configuration are unavailable."));
		return FReply::Handled();
	}

	UDataTable* ConversationLookupTable = nullptr;
	FString LookupError;
	if (!FTagKeyEditorHelpers::TryResolveDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationLookupTable, LookupError))
	{
		AppendLogLine(LookupError);
		return FReply::Handled();
	}

	if (ConversationLookupTable->GetRowStruct() != FParleyConversationAssetRow::StaticStruct())
	{
		AppendLogLine(FString::Printf(TEXT("Conversation lookup table row struct mismatch. Expected '%s', got '%s'."),
			*FParleyConversationAssetRow::StaticStruct()->GetName(),
			*GetNameSafe(ConversationLookupTable->GetRowStruct())));
		return FReply::Handled();
	}

	const UParleyDialogueEditorSettings* DialogueEditorSettings = GetDefault<UParleyDialogueEditorSettings>();
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
			TEXT("Invalid ConversationAssetsFolder '%s'. Use a package path like '/Game/Data/Conversations' in Project Settings -> Parley -> Dialogue Tooling."),
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

	const FScopedTransaction Transaction(NSLOCTEXT("ARDialogueSpeakerEditor", "CreateConversation", "Create Dialogue Conversation"));

	UParleyConversationAsset* NewConversation = NewObject<UParleyConversationAsset>(
		Package,
		UParleyConversationAsset::StaticClass(),
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
	if (const FGameplayTag RequesterSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Speaker.Requester"), false); RequesterSpeakerTag.IsValid())
	{
		NewConversation->Header.ParticipatingSpeakerTags.AddUnique(RequesterSpeakerTag);
	}
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
		const FParleyConversationAssetRow* ExistingRow = ConversationLookupTable->FindRow<FParleyConversationAssetRow>(
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

		if (UParleyConversationAsset* ExistingConversation = ExistingRow->Conversation.LoadSynchronous())
		{
			if (ExistingConversation->Header.ConversationTag.IsValid())
			{
				UsedConversationTags.Add(ExistingConversation->Header.ConversationTag);
			}
		}
	}

	const FString ConversationTagPrefix = BuildSpeakerConversationTagFilter(Settings, SpeakerRow->SpeakerTag);
	if (ConversationTagPrefix.IsEmpty())
	{
		AppendLogLine(TEXT("Failed to resolve conversation tag root for selected speaker."));
		return FReply::Handled();
	}

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

	FParleyConversationAssetRow NewLookupRow;
	NewLookupRow.ConversationTag = NewConversation->Header.ConversationTag;
	NewLookupRow.Conversation = TSoftObjectPtr<UParleyConversationAsset>(NewConversation);

	ConversationLookupTable->Modify();
	ConversationLookupTable->AddRow(LookupRowName, NewLookupRow);
	ConversationLookupTable->MarkPackageDirty();

	AppendLogLine(FString::Printf(TEXT("Created conversation asset '%s' and lookup row '%s' for speaker '%s' (level %d, min rel %.1f)."),
		*PackageName,
		*LookupRowName.ToString(),
		*SpeakerRow->SpeakerTag.ToString(),
		TargetRelationshipBand,
		NewConversation->Header.MinimumRelationshipPoints));

	SDialogueConversationGraphEditorPanel::RequestOpenConversation(NewConversation);
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("Parley_DialogueConversationGraphEditor")));

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

	SDialogueConversationGraphEditorPanel::RequestOpenConversation(SelectedItems[0]->Asset.Get());
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("Parley_DialogueConversationGraphEditor")));
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleRenameConversation()
{
	if (!ConversationListView.IsValid())
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FConversationEntry>> SelectedItems = ConversationListView->GetSelectedItems();
	if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid() || SelectedItems[0]->bIsBandHeader || !SelectedItems[0]->Asset.IsValid())
	{
		AppendLogLine(TEXT("No conversation selected."));
		return FReply::Handled();
	}

	RenamingConversationAsset = SelectedItems[0]->Asset;
	RefreshConversationMap();
	if (ConversationListView.IsValid())
	{
		for (const TSharedPtr<FConversationEntry>& Entry : ConversationEntries)
		{
			if (Entry.IsValid() && !Entry->bIsBandHeader && Entry->Asset == RenamingConversationAsset)
			{
				ConversationListView->SetSelection(Entry);
				ConversationListView->RequestScrollIntoView(Entry);
				break;
			}
		}
	}
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleDuplicateConversation()
{
	if (!ConversationListView.IsValid())
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FConversationEntry>> SelectedItems = ConversationListView->GetSelectedItems();
	if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid() || SelectedItems[0]->bIsBandHeader || !SelectedItems[0]->Asset.IsValid())
	{
		AppendLogLine(TEXT("No conversation selected."));
		return FReply::Handled();
	}

	UParleyConversationAsset* SourceConversation = SelectedItems[0]->Asset.Get();
	if (!SourceConversation)
	{
		AppendLogLine(TEXT("Selected conversation asset is unavailable."));
		return FReply::Handled();
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings || !Settings->ConversationDefinitionRootTag.IsValid())
	{
		AppendLogLine(TEXT("Dialogue settings/content lookup configuration are unavailable."));
		return FReply::Handled();
	}

	UDataTable* ConversationLookupTable = nullptr;
	FString LookupError;
	if (!FTagKeyEditorHelpers::TryResolveDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationLookupTable, LookupError))
	{
		AppendLogLine(LookupError);
		return FReply::Handled();
	}

	if (!ConversationLookupTable || ConversationLookupTable->GetRowStruct() != FParleyConversationAssetRow::StaticStruct())
	{
		AppendLogLine(TEXT("Conversation lookup table is invalid."));
		return FReply::Handled();
	}

	const FString SourcePackagePath = SourceConversation->GetOutermost()
		? SourceConversation->GetOutermost()->GetName()
		: FString();
	FString PackageFolder = FPackageName::GetLongPackagePath(SourcePackagePath);
	if (PackageFolder.IsEmpty())
	{
		const UParleyDialogueEditorSettings* DialogueEditorSettings = GetDefault<UParleyDialogueEditorSettings>();
		PackageFolder = DialogueEditorSettings ? DialogueEditorSettings->ConversationAssetsFolder.Path : FString();
		if (PackageFolder.IsEmpty())
		{
			PackageFolder = TEXT("/Game/Data/Conversations");
		}
		if (!PackageFolder.StartsWith(TEXT("/")))
		{
			PackageFolder = FString(TEXT("/")) + PackageFolder;
		}
		PackageFolder.RemoveFromEnd(TEXT("/"));
	}

	if (!FPackageName::IsValidLongPackageName(PackageFolder))
	{
		AppendLogLine(FString::Printf(TEXT("Invalid conversation package folder '%s'."), *PackageFolder));
		return FReply::Handled();
	}

	const FString BaseAssetName = SourceConversation->GetName() + TEXT("_Copy");
	FString NewAssetName = BaseAssetName;
	FString NewPackageName = FString::Printf(TEXT("%s/%s"), *PackageFolder, *NewAssetName);
	for (int32 Suffix = 1; FPackageName::DoesPackageExist(NewPackageName); ++Suffix)
	{
		NewAssetName = FString::Printf(TEXT("%s_%d"), *BaseAssetName, Suffix);
		NewPackageName = FString::Printf(TEXT("%s/%s"), *PackageFolder, *NewAssetName);
	}

	UPackage* NewPackage = CreatePackage(*NewPackageName);
	if (!NewPackage)
	{
		AppendLogLine(FString::Printf(TEXT("Failed to create package '%s'."), *NewPackageName));
		return FReply::Handled();
	}

	TSet<FGameplayTag> UsedConversationTags;
	for (const FName ExistingRowName : ConversationLookupTable->GetRowNames())
	{
		const FParleyConversationAssetRow* ExistingRow = ConversationLookupTable->FindRow<FParleyConversationAssetRow>(
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

		if (UParleyConversationAsset* ExistingConversation = ExistingRow->Conversation.LoadSynchronous())
		{
			if (ExistingConversation->Header.ConversationTag.IsValid())
			{
				UsedConversationTags.Add(ExistingConversation->Header.ConversationTag);
			}
		}
	}

	const FGameplayTag PrimarySpeakerTag = SourceConversation->Header.PrimarySpeakerTag;
	const FString ConversationTagPrefix = BuildSpeakerConversationTagFilter(Settings, PrimarySpeakerTag);
	if (ConversationTagPrefix.IsEmpty())
	{
		AppendLogLine(TEXT("Failed to resolve conversation tag root for selected conversation speaker."));
		return FReply::Handled();
	}

	FString EnsureTagError;
	FGameplayTag NewConversationTag;
	for (int32 Index = 1; Index < 100000; ++Index)
	{
		const FString CandidateTagPath = FString::Printf(TEXT("%s.%d"), *ConversationTagPrefix, Index);
		const FGameplayTag CandidateTag = EnsureGameplayTagRegistered(
			CandidateTagPath,
			FString::Printf(TEXT("Auto-created conversation tag for speaker '%s'."), *PrimarySpeakerTag.ToString()),
			EnsureTagError);
		if (!CandidateTag.IsValid())
		{
			break;
		}

		if (!UsedConversationTags.Contains(CandidateTag))
		{
			NewConversationTag = CandidateTag;
			break;
		}
	}

	if (!NewConversationTag.IsValid())
	{
		AppendLogLine(FString::Printf(TEXT("Failed to allocate a duplicate conversation tag under '%s': %s"), *ConversationTagPrefix, *EnsureTagError));
		return FReply::Handled();
	}

	FName NewLookupRowName(*NewAssetName);
	for (int32 Suffix = 1; ConversationLookupTable->GetRowMap().Contains(NewLookupRowName); ++Suffix)
	{
		NewLookupRowName = FName(*FString::Printf(TEXT("%s_%d"), *NewAssetName, Suffix));
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ARDialogueSpeakerEditor", "DuplicateConversation", "Duplicate Dialogue Conversation"));
	ConversationLookupTable->Modify();

	UParleyConversationAsset* NewConversation = DuplicateObject<UParleyConversationAsset>(
		SourceConversation,
		NewPackage,
		*NewAssetName);
	if (!NewConversation)
	{
		AppendLogLine(TEXT("Failed to duplicate conversation asset."));
		return FReply::Handled();
	}

	NewConversation->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	NewConversation->Modify();
	NewConversation->Header.ConversationTag = NewConversationTag;
	if (!NewConversation->Header.DisplayTitle.IsEmpty())
	{
		NewConversation->Header.DisplayTitle = FText::FromString(NewConversation->Header.DisplayTitle.ToString() + TEXT(" Copy"));
	}
	NewConversation->Header.ParticipatingSpeakerTags.AddUnique(NewConversation->Header.PrimarySpeakerTag);
	if (const FGameplayTag RequesterSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Parley.Speaker.Requester"), false); RequesterSpeakerTag.IsValid())
	{
		NewConversation->Header.ParticipatingSpeakerTags.AddUnique(RequesterSpeakerTag);
	}

	FParleyConversationAssetRow NewLookupRow;
	NewLookupRow.ConversationTag = NewConversationTag;
	NewLookupRow.Conversation = TSoftObjectPtr<UParleyConversationAsset>(NewConversation);
	ConversationLookupTable->AddRow(NewLookupRowName, NewLookupRow);
	ConversationLookupTable->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(NewConversation);
	NewPackage->MarkPackageDirty();
	NewConversation->MarkPackageDirty();

	AppendLogLine(FString::Printf(
		TEXT("Duplicated conversation '%s' as '%s' (Tag=%s, LookupRow=%s)."),
		*GetNameSafe(SourceConversation),
		*NewPackageName,
		*NewConversationTag.ToString(),
		*NewLookupRowName.ToString()));

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	if (ConversationListView.IsValid())
	{
		for (const TSharedPtr<FConversationEntry>& Entry : ConversationEntries)
		{
			if (Entry.IsValid() && !Entry->bIsBandHeader && Entry->Asset.Get() == NewConversation)
			{
				ConversationListView->SetSelection(Entry);
				ConversationListView->RequestScrollIntoView(Entry);
				break;
			}
		}
	}

	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleDeleteConversation()
{
	return HandleDeleteConversationInternal(false);
}

FReply SDialogueSpeakerEditorPanel::HandleDeleteConversationAndAsset()
{
	return HandleDeleteConversationInternal(true);
}

FReply SDialogueSpeakerEditorPanel::HandleDeleteConversationInternal(const bool bDeleteAssetFromContentBrowser)
{
	if (!ConversationListView.IsValid())
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FConversationEntry>> SelectedItems = ConversationListView->GetSelectedItems();
	if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid() || SelectedItems[0]->bIsBandHeader || !SelectedItems[0]->Asset.IsValid())
	{
		AppendLogLine(TEXT("No conversation selected."));
		return FReply::Handled();
	}

	UParleyConversationAsset* ConversationToDelete = SelectedItems[0]->Asset.Get();
	if (!ConversationToDelete)
	{
		AppendLogLine(TEXT("Selected conversation asset is unavailable."));
		return FReply::Handled();
	}

	const FString ConversationLabel = ConversationToDelete->Header.DisplayTitle.IsEmpty()
		? GetNameSafe(ConversationToDelete)
		: ConversationToDelete->Header.DisplayTitle.ToString();

	const FText ConfirmMessage = bDeleteAssetFromContentBrowser
		? FText::FromString(FString::Printf(TEXT("Delete asset '%s' from Content Browser and remove it from lookup?"), *ConversationLabel))
		: FText::FromString(FString::Printf(TEXT("Remove conversation '%s' from lookup?"), *ConversationLabel));
	const EAppReturnType::Type Confirm = FMessageDialog::Open(
		EAppMsgType::YesNo,
		ConfirmMessage);
	if (Confirm != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings || !Settings->ConversationDefinitionRootTag.IsValid())
	{
		AppendLogLine(TEXT("Dialogue settings/content lookup configuration are unavailable."));
		return FReply::Handled();
	}

	UDataTable* ConversationLookupTable = nullptr;
	FString LookupError;
	if (!FTagKeyEditorHelpers::TryResolveDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationLookupTable, LookupError))
	{
		AppendLogLine(LookupError);
		return FReply::Handled();
	}

	if (!ConversationLookupTable || ConversationLookupTable->GetRowStruct() != FParleyConversationAssetRow::StaticStruct())
	{
		AppendLogLine(TEXT("Conversation lookup table is invalid."));
		return FReply::Handled();
	}

	TArray<FName> RowsToRemove;
	TSet<FGameplayTag> RemovedConversationTags;
	const FGameplayTag ConversationTag = ConversationToDelete->Header.ConversationTag;
	for (const FName RowName : ConversationLookupTable->GetRowNames())
	{
		const FParleyConversationAssetRow* Row = ConversationLookupTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorDeleteConversation"), false);
		if (!Row)
		{
			continue;
		}

		const UParleyConversationAsset* RowConversation = Row->Conversation.LoadSynchronous();
		const bool bMatchesAsset = RowConversation == ConversationToDelete;
		const bool bMatchesTag = ConversationTag.IsValid() && Row->ConversationTag.IsValid() && Row->ConversationTag.MatchesTagExact(ConversationTag);
		if (!bMatchesAsset && !bMatchesTag)
		{
			continue;
		}

		RowsToRemove.Add(RowName);
		if (Row->ConversationTag.IsValid())
		{
			RemovedConversationTags.Add(Row->ConversationTag);
		}
	}

	if (ConversationTag.IsValid())
	{
		RemovedConversationTags.Add(ConversationTag);
	}

	TArray<UParleyConversationAsset*> LoadedConversations;
	for (const FName RowName : ConversationLookupTable->GetRowNames())
	{
		const FParleyConversationAssetRow* Row = ConversationLookupTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorDeleteConversationScan"), false);
		if (!Row)
		{
			continue;
		}

		if (UParleyConversationAsset* LoadedConversation = Row->Conversation.LoadSynchronous())
		{
			LoadedConversations.AddUnique(LoadedConversation);
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("ARDialogueSpeakerEditor", "DeleteConversation", "Delete Dialogue Conversation"));
	ConversationLookupTable->Modify();
	for (const FName RowName : RowsToRemove)
	{
		ConversationLookupTable->RemoveRow(RowName);
	}
	ConversationLookupTable->MarkPackageDirty();

	ConversationToDelete->Modify();
	ConversationToDelete->Header.ConversationTag = FGameplayTag();
	ConversationToDelete->MarkPackageDirty();

	for (UParleyConversationAsset* LoadedConversation : LoadedConversations)
	{
		RemoveConversationTagReferencesFromConversation(LoadedConversation, RemovedConversationTags);
	}

	if (bDeleteAssetFromContentBrowser)
	{
		TArray<FAssetData> AssetsToDelete;
		AssetsToDelete.Add(FAssetData(ConversationToDelete));
		const int32 DeletedAssetCount = ObjectTools::DeleteAssets(AssetsToDelete, false);
		AppendLogLine(FString::Printf(
			TEXT("Asset delete requested for '%s' (Deleted=%d)."),
			*GetNameSafe(ConversationToDelete),
			DeletedAssetCount));
	}

	AppendLogLine(FString::Printf(
		TEXT("%s '%s' (Rows=%d, TagsRemoved=%d)."),
		bDeleteAssetFromContentBrowser ? TEXT("Deleted asset + removed lookup for") : TEXT("Removed lookup entry for"),
		*ConversationLabel,
		RowsToRemove.Num(),
		RemovedConversationTags.Num()));

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleCleanupGeneratedConversationTags()
{
	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings || !Settings->ConversationDefinitionRootTag.IsValid())
	{
		AppendLogLine(TEXT("Dialogue settings/content lookup configuration are unavailable."));
		return FReply::Handled();
	}

	UDataTable* ConversationLookupTable = nullptr;
	FString LookupError;
	if (!FTagKeyEditorHelpers::TryResolveDataTableForRootTag(Settings->ConversationDefinitionRootTag, ConversationLookupTable, LookupError))
	{
		AppendLogLine(LookupError);
		return FReply::Handled();
	}

	if (!ConversationLookupTable || ConversationLookupTable->GetRowStruct() != FParleyConversationAssetRow::StaticStruct())
	{
		AppendLogLine(TEXT("Conversation lookup table is invalid."));
		return FReply::Handled();
	}

	TSet<FString> ReferencedTagPaths;
	for (const FName RowName : ConversationLookupTable->GetRowNames())
	{
		const FParleyConversationAssetRow* Row = ConversationLookupTable->FindRow<FParleyConversationAssetRow>(RowName, TEXT("DialogueSpeakerEditorCleanupGeneratedTags"), false);
		if (!Row)
		{
			continue;
		}

		if (Row->ConversationTag.IsValid())
		{
			ReferencedTagPaths.Add(Row->ConversationTag.ToString());
		}

		if (UParleyConversationAsset* Conversation = Row->Conversation.LoadSynchronous())
		{
			if (Conversation->Header.ConversationTag.IsValid())
			{
				ReferencedTagPaths.Add(Conversation->Header.ConversationTag.ToString());
			}
		}
	}

	const FString GeneratedConfigPath = FPaths::ProjectConfigDir() / DialogueAutoTagConfigRelativePath;
	if (!FPaths::FileExists(GeneratedConfigPath))
	{
		AppendLogLine(TEXT("No generated dialogue conversation tag config file found; nothing to clean."));
		return FReply::Handled();
	}

	FString ConfigText;
	if (!FFileHelper::LoadFileToString(ConfigText, *GeneratedConfigPath))
	{
		AppendLogLine(FString::Printf(TEXT("Failed to load generated tag config '%s'."), *GeneratedConfigPath));
		return FReply::Handled();
	}

	const FString ConversationRootPath = Settings->ConversationDefinitionRootTag.ToString();
	const FString ConversationRootPrefix = ConversationRootPath + TEXT(".");

	TArray<FString> Lines;
	ConfigText.ParseIntoArrayLines(Lines);
	int32 RemovedLineCount = 0;
	for (int32 LineIndex = Lines.Num() - 1; LineIndex >= 0; --LineIndex)
	{
		const FString& Line = Lines[LineIndex];
		const int32 TagTokenStart = Line.Find(TEXT("Tag=\""), ESearchCase::CaseSensitive);
		if (TagTokenStart == INDEX_NONE)
		{
			continue;
		}

		const int32 TagValueStart = TagTokenStart + 5;
		const int32 TagValueEnd = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, TagValueStart);
		if (TagValueEnd == INDEX_NONE || TagValueEnd < TagValueStart)
		{
			continue;
		}

		const FString TagPath = Line.Mid(TagValueStart, TagValueEnd - TagValueStart);
		const bool bIsConversationTag = TagPath.Equals(ConversationRootPath, ESearchCase::CaseSensitive)
			|| TagPath.StartsWith(ConversationRootPrefix, ESearchCase::CaseSensitive);
		if (!bIsConversationTag)
		{
			continue;
		}

		if (!ReferencedTagPaths.Contains(TagPath))
		{
			Lines.RemoveAt(LineIndex);
			++RemovedLineCount;
		}
	}

	if (RemovedLineCount <= 0)
	{
		AppendLogLine(TEXT("Cleanup generated conversation tags: no stale generated tags found."));
		return FReply::Handled();
	}

	const FString UpdatedText = FString::Join(Lines, LINE_TERMINATOR) + LINE_TERMINATOR;
	if (!FFileHelper::SaveStringToFile(UpdatedText, *GeneratedConfigPath))
	{
		AppendLogLine(FString::Printf(TEXT("Failed to write generated tag config '%s'."), *GeneratedConfigPath));
		return FReply::Handled();
	}

	UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
	AppendLogLine(FString::Printf(
		TEXT("Cleanup generated conversation tags completed: removed %d stale tag line(s) from '%s'."),
		RemovedLineCount,
		*GeneratedConfigPath));
	return FReply::Handled();
}

FReply SDialogueSpeakerEditorPanel::HandleFindBrokenConversations()
{
	ValidationOutput.Empty();

	const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
	if (!Settings)
	{
		AppendLogLine(TEXT("Dialogue settings are unavailable."));
		return FReply::Handled();
	}

	int32 ErrorIssueCount = 0;
	int32 WarningIssueCount = 0;
	bool bFoundAnyIssues = false;
	TMap<FGameplayTag, UParleyConversationAsset*> ConversationsByTag;
	GatherConversationAssetsFromLookup(Settings, ConversationsByTag);
	TMap<FGameplayTag, TArray<UParleyConversationAsset*>> ConversationsBySpeakerTag;

	auto BuildConversationLabel = [](const UParleyConversationAsset* Conversation) -> FString
	{
		if (!Conversation)
		{
			return TEXT("<Invalid Conversation>");
		}

		FString DisplayTitle = Conversation->Header.DisplayTitle.ToString().TrimStartAndEnd();
		if (DisplayTitle.IsEmpty())
		{
			DisplayTitle = Conversation->Header.ConversationTag.IsValid()
				? Conversation->Header.ConversationTag.ToString()
				: Conversation->GetName();
		}

		const FString ConversationTagString = Conversation->Header.ConversationTag.IsValid()
			? Conversation->Header.ConversationTag.ToString()
			: FString(TEXT("<InvalidTag>"));
		return FString::Printf(TEXT("%s [%s]"), *DisplayTitle, *ConversationTagString);
	};

	for (const TPair<FGameplayTag, UParleyConversationAsset*>& Pair : ConversationsByTag)
	{
		UParleyConversationAsset* Conversation = Pair.Value;
		if (!Conversation)
		{
			AppendLogLine(FString::Printf(TEXT("ERROR: Failed to load conversation for tag '%s'."), *Pair.Key.ToString()));
			++ErrorIssueCount;
			bFoundAnyIssues = true;
			continue;
		}

		ConversationsBySpeakerTag.FindOrAdd(Conversation->Header.PrimarySpeakerTag).Add(Conversation);

		FDialogueValidationReport Report;
		const bool bValid = ValidateConversationWithBestAvailable(Conversation, Report);
		const bool bHasNonInfoIssue = Report.Issues.ContainsByPredicate(
			[](const FDialogueValidationIssue& Issue)
			{
				return Issue.Severity != EDialogueValidationSeverity::Info;
			});
		if (bValid && !bHasNonInfoIssue)
		{
			continue;
		}

		bFoundAnyIssues = true;
		AppendLogLine(FString::Printf(TEXT("%s: %s"), Report.HasErrors() ? TEXT("ERROR") : TEXT("WARN"), *BuildConversationLabel(Conversation)));
		for (const FDialogueValidationIssue& Issue : Report.Issues)
		{
			if (Issue.Severity == EDialogueValidationSeverity::Info)
			{
				continue;
			}

			if (Issue.Severity == EDialogueValidationSeverity::Error)
			{
				++ErrorIssueCount;
			}
			else
			{
				++WarningIssueCount;
			}

			const TCHAR* Severity = Issue.Severity == EDialogueValidationSeverity::Error ? TEXT("ERROR") : TEXT("WARN");
			AppendLogLine(FString::Printf(TEXT("  - [%s] %s"), Severity, *Issue.Message.ToString()));
		}
	}

	for (const TPair<FGameplayTag, TArray<UParleyConversationAsset*>>& SpeakerPair : ConversationsBySpeakerTag)
	{
		const FGameplayTag SpeakerTag = SpeakerPair.Key;
		const TArray<UParleyConversationAsset*>& SpeakerConversations = SpeakerPair.Value;
		if (SpeakerConversations.Num() < 2)
		{
			continue;
		}

		const FString SpeakerLabel = SpeakerTag.IsValid() ? SpeakerTag.ToString() : FString(TEXT("<InvalidSpeakerTag>"));
		TMap<FString, TArray<UParleyConversationAsset*>> ConversationsByDisplayTitleKey;
		TMap<int32, TArray<UParleyConversationAsset*>> ConversationsByPriority;

		for (UParleyConversationAsset* Conversation : SpeakerConversations)
		{
			if (!Conversation)
			{
				continue;
			}

			ConversationsByPriority.FindOrAdd(Conversation->Header.Priority).Add(Conversation);

			const FString DisplayTitle = Conversation->Header.DisplayTitle.ToString().TrimStartAndEnd();
			if (!DisplayTitle.IsEmpty())
			{
				ConversationsByDisplayTitleKey.FindOrAdd(DisplayTitle.ToLower()).Add(Conversation);
			}
		}

		for (const TPair<FString, TArray<UParleyConversationAsset*>>& DuplicateTitlePair : ConversationsByDisplayTitleKey)
		{
			const TArray<UParleyConversationAsset*>& DuplicateConversations = DuplicateTitlePair.Value;
			if (DuplicateConversations.Num() < 2)
			{
				continue;
			}

			bFoundAnyIssues = true;
			++WarningIssueCount;

			TArray<FString> ConversationLabels;
			FString SharedDisplayTitle;
			for (UParleyConversationAsset* Conversation : DuplicateConversations)
			{
				if (!Conversation)
				{
					continue;
				}

				if (SharedDisplayTitle.IsEmpty())
				{
					SharedDisplayTitle = Conversation->Header.DisplayTitle.ToString().TrimStartAndEnd();
				}

				ConversationLabels.Add(BuildConversationLabel(Conversation));
			}
			ConversationLabels.Sort();

			AppendLogLine(FString::Printf(
				TEXT("WARN: Speaker '%s' has duplicate display title '%s' (%d conversations): %s"),
				*SpeakerLabel,
				*SharedDisplayTitle,
				ConversationLabels.Num(),
				*FString::Join(ConversationLabels, TEXT(", "))));
		}

		for (const TPair<int32, TArray<UParleyConversationAsset*>>& PriorityPair : ConversationsByPriority)
		{
			const TArray<UParleyConversationAsset*>& PriorityConversations = PriorityPair.Value;
			if (PriorityConversations.Num() < 2)
			{
				continue;
			}

			bFoundAnyIssues = true;
			++WarningIssueCount;

			TArray<FString> PriorityLabels;
			for (UParleyConversationAsset* Conversation : PriorityConversations)
			{
				if (!Conversation)
				{
					continue;
				}
				PriorityLabels.Add(FString::Printf(
					TEXT("%s (Weight %d)"),
					*BuildConversationLabel(Conversation),
					FMath::Max(1, Conversation->Header.OfferWeight)));
			}
			PriorityLabels.Sort();

			AppendLogLine(FString::Printf(
				TEXT("WARN: Speaker '%s' has %d conversation(s) at priority %d. Selection between same-priority candidates is weighted-random by Offer Weight: %s"),
				*SpeakerLabel,
				PriorityLabels.Num(),
				PriorityPair.Key,
				*FString::Join(PriorityLabels, TEXT(", "))));
		}
	}

	if (!bFoundAnyIssues)
	{
		AppendLogLine(TEXT("No broken conversations found."));
	}
	else
	{
		AppendLogLine(FString::Printf(TEXT("Find Broken complete: %d error(s), %d warning(s)."), ErrorIssueCount, WarningIssueCount));
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
	EditedRelationshipThresholds = { 5.0f, 15.0f, 30.0f, 50.0f };
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

bool SDialogueSpeakerEditorPanel::CanCreateEmotionFromFields() const
{
	return SpeakerDataTable.IsValid()
		&& !SelectedSpeakerRowName.IsNone()
		&& !EditedDefaultPortraitTexture.IsNull()
		&& EditedPortraitTag.IsValid();
}

FReply SDialogueSpeakerEditorPanel::HandleCreateEmotionFromFields()
{
	SelectedPortraitIndex = INDEX_NONE;
	return HandleAddPortrait();
}

FReply SDialogueSpeakerEditorPanel::HandleAddEmotionSlot()
{
	UDataTable* SpeakerTable = SpeakerDataTable.Get();
	if (!SpeakerTable || SelectedSpeakerRowName.IsNone())
	{
		AppendLogLine(TEXT("No speaker selected."));
		return FReply::Handled();
	}

	FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

	int32 TargetPortraitIndex = SelectedPortraitIndex;
	SpeakerTable->Modify();
	if (SelectedPortraitIndex != INDEX_NONE && Row->Portraits.IsValidIndex(SelectedPortraitIndex))
	{
		Row->Portraits[SelectedPortraitIndex] = NewPortrait;
		AppendLogLine(FString::Printf(TEXT("Updated emotion index %d."), SelectedPortraitIndex));
	}
	else
	{
		Row->Portraits.Add(NewPortrait);
		TargetPortraitIndex = Row->Portraits.Num() - 1;
		AppendLogLine(TEXT("Added new emotion entry."));
	}
	SpeakerTable->MarkPackageDirty();

	RefreshData();
	SetSelectedSpeakerRow(SelectedSpeakerRowName);
	SelectedPortraitIndex = TargetPortraitIndex;
	if (PortraitEntries.IsValidIndex(SelectedPortraitIndex) && PortraitListView.IsValid())
	{
		PortraitListView->SetSelection(PortraitEntries[SelectedPortraitIndex], ESelectInfo::OnMouseClick);
	}
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

	FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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
		const FLinearColor BandColor = FLinearColor(0.05f, 0.43f, 0.86f, 1.0f);

		return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
		.Style(&GetConversationBandHeaderRowStyle())
		.ShowSelection(false)
		.OnCanAcceptDrop_Lambda([Item](const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FConversationEntry>)
		{
			if (!Item.IsValid() || !Item->bIsBandHeader)
			{
				return TOptional<EItemDropZone>();
			}

			const TSharedPtr<FConversationBandDragDropOp> DragOp = DragDropEvent.GetOperationAs<FConversationBandDragDropOp>();
			if (!DragOp.IsValid() || !DragOp->ConversationAsset.IsValid())
			{
				return TOptional<EItemDropZone>();
			}

			if (DragOp->SourceBand == Item->RelationshipBand)
			{
				return TOptional<EItemDropZone>();
			}

			return TOptional<EItemDropZone>(DropZone);
		})
		.OnAcceptDrop_Lambda([this, Item](const FDragDropEvent& DragDropEvent, EItemDropZone, TSharedPtr<FConversationEntry>)
		{
			const TSharedPtr<FConversationBandDragDropOp> DragOp = DragDropEvent.GetOperationAs<FConversationBandDragDropOp>();
			if (!Item.IsValid() || !DragOp.IsValid() || !DragOp->ConversationAsset.IsValid())
			{
				return FReply::Unhandled();
			}

			return const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleSetConversationBand(DragOp->ConversationAsset, Item->RelationshipBand);
		})
		[
			SNew(SBorder)
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&)
			{
				return FReply::Handled();
			})
			.BorderBackgroundColor(BandColor)
			.Padding(FMargin(8.0f, 6.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayTitle))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			]
		];
	}

	if (Item->bIsLevelZeroDropTarget)
	{
		return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
		.ShowSelection(false)
		.OnCanAcceptDrop_Lambda([](const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FConversationEntry>)
		{
			const TSharedPtr<FConversationBandDragDropOp> DragOp = DragDropEvent.GetOperationAs<FConversationBandDragDropOp>();
			if (!DragOp.IsValid() || !DragOp->ConversationAsset.IsValid() || DragOp->SourceBand == 0)
			{
				return TOptional<EItemDropZone>();
			}

			return TOptional<EItemDropZone>(DropZone);
		})
		.OnAcceptDrop_Lambda([this](const FDragDropEvent& DragDropEvent, EItemDropZone, TSharedPtr<FConversationEntry>)
		{
			const TSharedPtr<FConversationBandDragDropOp> DragOp = DragDropEvent.GetOperationAs<FConversationBandDragDropOp>();
			if (!DragOp.IsValid() || !DragOp->ConversationAsset.IsValid())
			{
				return FReply::Unhandled();
			}

			return const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleSetConversationBand(DragOp->ConversationAsset, 0);
		})
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.10f, 0.20f, 0.30f, 0.45f))
			.Padding(FMargin(7.0f, 5.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayTitle))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.85f, 1.0f, 1.0f)))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
		];
	}

	TSharedRef<SWrapBox> LockedByTagsWrap = SNew(SWrapBox).UseAllottedSize(true);
	if (Item->LockedByConversationTags.IsEmpty())
	{
		LockedByTagsWrap->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("-")))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.70f, 0.70f, 1.0f)))
		];
	}
	else
	{
		for (int32 TagIndex = 0; TagIndex < Item->LockedByConversationTags.Num(); ++TagIndex)
		{
			const FGameplayTag LockedTag = Item->LockedByConversationTags[TagIndex];
			LockedByTagsWrap->AddSlot()
			.Padding(0.0f, 0.0f, 6.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(220.0f)
					[
						SNew(SGameplayTagCombo)
						.Filter(Item->ConversationTagFilter)
						.Tag(LockedTag)
						.OnTagChanged_Lambda([this, WeakConversation = Item->Asset, TagIndex](const FGameplayTag NewTag)
						{
							const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleConversationLockedByTagChanged(WeakConversation, TagIndex, NewTag);
						})
					]
				]
			];
		}
	}

	LockedByTagsWrap->AddSlot()
	.Padding(0.0f, 0.0f, 6.0f, 4.0f)
	[
		SNew(SButton)
		.Text(FText::FromString(TEXT("Add Lock Tag")))
		.ToolTipText(FText::FromString(TEXT("Add another required conversation tag lock for this conversation.")))
		.OnClicked_Lambda([this, WeakConversation = Item->Asset]()
		{
			return const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleAddConversationLockedByTag(WeakConversation);
		})
	];

	const bool bIsRenamingTitle = Item->Asset.IsValid() && RenamingConversationAsset.IsValid() && Item->Asset == RenamingConversationAsset;
	TSharedRef<SWidget> ConversationTitleWidget =
		bIsRenamingTitle
		? StaticCastSharedRef<SWidget>(
			SNew(SEditableTextBox)
			.Text(FText::FromString(Item->DisplayTitle))
			.SelectAllTextWhenFocused(true)
			.OnTextCommitted_Lambda([this, WeakConversation = Item->Asset](const FText& NewText, ETextCommit::Type CommitType)
			{
				const_cast<SDialogueSpeakerEditorPanel*>(this)->CommitConversationDisplayName(WeakConversation, NewText, CommitType);
			}))
		: StaticCastSharedRef<SWidget>(
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Transparent)
			.Padding(FMargin(0.0f))
			.ToolTipText(FText::FromString(TEXT("Drag this conversation to a Level header to change its relationship level. Right-click for actions.")))
			.OnMouseButtonDown_Lambda([this, Item](const FGeometry&, const FPointerEvent& MouseEvent)
			{
				if (!Item.IsValid() || !Item->Asset.IsValid() || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
				{
					return FReply::Unhandled();
				}

				if (ConversationListView.IsValid())
				{
					ConversationListView->SetSelection(Item, ESelectInfo::OnMouseClick);
				}

				return FReply::Handled().BeginDragDrop(
					FConversationBandDragDropOp::New(
						Item->Asset,
						Item->RelationshipBand,
						FString::Printf(TEXT("Move: %s"), *Item->DisplayTitle)));
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("::")))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.62f, 0.62f, 1.0f)))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->DisplayTitle))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.AutoWrapText(true)
				]
			]);

	return SNew(STableRow<TSharedPtr<FConversationEntry>>, OwnerTable)
		.OnDragDetected_Lambda([this, Item](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (!Item.IsValid() || !Item->Asset.IsValid() || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}

			if (ConversationListView.IsValid())
			{
				ConversationListView->SetSelection(Item, ESelectInfo::OnMouseClick);
			}

			return FReply::Handled().BeginDragDrop(
				FConversationBandDragDropOp::New(
					Item->Asset,
					Item->RelationshipBand,
					FString::Printf(TEXT("Move: %s"), *Item->DisplayTitle)));
		})
		.OnCanAcceptDrop_Lambda([Item](const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FConversationEntry>)
		{
			if (!Item.IsValid() || !Item->Asset.IsValid())
			{
				return TOptional<EItemDropZone>();
			}

			const TSharedPtr<FConversationBandDragDropOp> DragOp = DragDropEvent.GetOperationAs<FConversationBandDragDropOp>();
			if (!DragOp.IsValid() || !DragOp->ConversationAsset.IsValid() || DragOp->SourceBand == Item->RelationshipBand)
			{
				return TOptional<EItemDropZone>();
			}

			return TOptional<EItemDropZone>(DropZone);
		})
		.OnAcceptDrop_Lambda([this, Item](const FDragDropEvent& DragDropEvent, EItemDropZone, TSharedPtr<FConversationEntry>)
		{
			const TSharedPtr<FConversationBandDragDropOp> DragOp = DragDropEvent.GetOperationAs<FConversationBandDragDropOp>();
			if (!Item.IsValid() || !DragOp.IsValid() || !DragOp->ConversationAsset.IsValid())
			{
				return FReply::Unhandled();
			}

			return const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleSetConversationBand(DragOp->ConversationAsset, Item->RelationshipBand);
		})
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.075f, 0.075f, 0.075f, 1.0f))
		.Padding(FMargin(7.0f, 6.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				ConversationTitleWidget
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(0.20f, 0.24f, 0.32f, 1.0f))
					.Padding(FMargin(5.0f, 2.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Pri")))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(44.0f)
							[
								SNew(SEditableTextBox)
								.Text(FText::AsNumber(Item->Priority))
								.MinDesiredWidth(40.0f)
								.OnTextCommitted_Lambda([this, WeakConversation = Item->Asset](const FText& NewText, ETextCommit::Type CommitType)
								{
									const_cast<SDialogueSpeakerEditorPanel*>(this)->CommitConversationPriority(WeakConversation, NewText, CommitType);
								})
							]
						]
					]
				]
				+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(116.0f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(6.0f, 2.0f))
						.OnClicked_Lambda([this, WeakConversation = Item->Asset]()
						{
							return const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleToggleConversationRepeatable(WeakConversation);
						})
						[
							SNew(STextBlock).Text(FText::FromString(Item->bRepeatable ? TEXT("Repeatable") : TEXT("One-shot")))
						]
					]
				]
				+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(98.0f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(6.0f, 2.0f))
						.OnClicked_Lambda([this, WeakConversation = Item->Asset]()
						{
							return const_cast<SDialogueSpeakerEditorPanel*>(this)->HandleToggleConversationImportant(WeakConversation);
						})
						[
							SNew(STextBlock).Text(FText::FromString(Item->bImportant ? TEXT("Important") : TEXT("Normal")))
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 1.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Locked by")))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					LockedByTagsWrap
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Required Tags")))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SGameplayTagContainerCombo)
					.TagContainer_Lambda([WeakConversation = Item->Asset, FilterRoot = Item->ConversationTagFilter]()
					{
						FGameplayTagContainer Tags;
						if (const UParleyConversationAsset* Conversation = WeakConversation.Get())
						{
							TArray<FGameplayTag> RequiredTags;
							GatherHeaderRequiredTags(Conversation, FilterRoot, RequiredTags);
							for (const FGameplayTag RequiredTag : RequiredTags)
							{
								if (RequiredTag.IsValid())
								{
									Tags.AddTag(RequiredTag);
								}
							}
						}
						return Tags;
					})
					.OnTagContainerChanged_Lambda([this, WeakConversation = Item->Asset](const FGameplayTagContainer& NewTags)
					{
						const_cast<SDialogueSpeakerEditorPanel*>(this)->CommitConversationRequiredTags(WeakConversation, NewTags);
					})
					.OnTagContainerComboClosed_Lambda([this, WeakConversation = Item->Asset](const FGameplayTagContainer& NewTags)
					{
						const_cast<SDialogueSpeakerEditorPanel*>(this)->CommitConversationRequiredTags(WeakConversation, NewTags);
					})
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
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f))
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.09f, 0.09f, 0.09f, 1.0f))
		.Padding(FMargin(6.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f).VAlign(VAlign_Center)
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
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Item->Label))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TextureLabel))
				]
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

	const FString BandLabel = FString::Printf(TEXT("Level %d"), Item->ThresholdIndex + 1);
	const FString ValueLabel = FString::Printf(TEXT("%.1f"), Item->Value);
	const FLinearColor BandTint = (Item->ThresholdIndex % 2 == 0)
		? FLinearColor(0.18f, 0.32f, 0.50f, 1.0f)
		: FLinearColor(0.22f, 0.25f, 0.40f, 1.0f);

	return SNew(STableRow<TSharedPtr<FThresholdEntry>>, OwnerTable)
	[
		SNew(SBorder)
		.OnMouseButtonDown_Lambda([this, ThresholdIndex = Item->ThresholdIndex](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (!ThresholdListView.IsValid() || !ThresholdEntries.IsValidIndex(ThresholdIndex))
			{
				return FReply::Unhandled();
			}

			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
				const_cast<SDialogueSpeakerEditorPanel*>(this)->SelectedThresholdIndex = ThresholdIndex;
				ThresholdListView->SetSelection(ThresholdEntries[ThresholdIndex], ESelectInfo::OnMouseClick);
			}

			return FReply::Unhandled();
		})
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
				SNew(SEditableTextBox)
				.Text(FText::FromString(ValueLabel))
				.SelectAllTextWhenFocused(true)
				.OnTextCommitted_Lambda([this, ThresholdIndex = Item->ThresholdIndex](const FText& NewText, ETextCommit::Type CommitType)
				{
					const_cast<SDialogueSpeakerEditorPanel*>(this)->CommitInlineThresholdEdit(ThresholdIndex, NewText, CommitType);
				})
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
		RenamingConversationAsset.Reset();
		SelectedSpeakerRowName = NAME_None;
		SelectedPortraitIndex = INDEX_NONE;
		SyncSpeakerFieldsFromSelection();
		return;
	}

	if (!SelectedSpeakerRowName.IsNone() && Item->RowName != SelectedSpeakerRowName)
	{
		HandleSaveSpeaker();
		RenamingConversationAsset.Reset();
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

	SDialogueConversationGraphEditorPanel::RequestOpenConversation(Item->Asset.Get());
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("Parley_DialogueConversationGraphEditor")));
}

void SDialogueSpeakerEditorPanel::OnConversationSelectionChanged(TSharedPtr<FConversationEntry> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (!ConversationListView.IsValid() || !Item.IsValid() || (!Item->bIsBandHeader && !Item->bIsLevelZeroDropTarget))
	{
		return;
	}

	ConversationListView->ClearSelection();
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
	const FParleySpeakerRow* Row = SpeakerTable->FindRow<FParleySpeakerRow>(SelectedSpeakerRowName, TEXT("DialogueSpeakerEditor"), false);
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

