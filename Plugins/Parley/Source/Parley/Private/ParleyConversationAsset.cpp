#include "ParleyConversationAsset.h"

#include "GameplayTagsManager.h"

namespace
{
	static FGameplayTag RequestOptionalGameplayTag(const TCHAR* TagPath)
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(TagPath), false);
	}

	static FGameplayTag ResolveLegacyCharacterRestrictionTag(const EDialogueActiveCharacterRestriction Restriction)
	{
		switch (Restriction)
		{
		case EDialogueActiveCharacterRestriction::BrotherOnly:
		{
			const FGameplayTag BrotherCharacterTag = RequestOptionalGameplayTag(TEXT("Shop.Character.Brother"));
			return BrotherCharacterTag.IsValid()
				? BrotherCharacterTag
				: RequestOptionalGameplayTag(TEXT("Parley.Speaker.Brother"));
		}
		case EDialogueActiveCharacterRestriction::SisterOnly:
		{
			const FGameplayTag SisterCharacterTag = RequestOptionalGameplayTag(TEXT("Shop.Character.Sister"));
			return SisterCharacterTag.IsValid()
				? SisterCharacterTag
				: RequestOptionalGameplayTag(TEXT("Parley.Speaker.Sister"));
		}
		case EDialogueActiveCharacterRestriction::Any:
		default:
			return FGameplayTag();
		}
	}

	static void MigrateCharacterRestrictionTag(FDialogueConversationHeader& Header)
	{
		if (!Header.CharacterRestrictionTag.IsValid())
		{
			Header.CharacterRestrictionTag = ResolveLegacyCharacterRestrictionTag(Header.CharacterRestriction);
		}
	}

	static void MigrateCharacterRestrictionTag(FDialogueLineNodeData& LineData)
	{
		if (!LineData.CharacterRestrictionTag.IsValid())
		{
			LineData.CharacterRestrictionTag = ResolveLegacyCharacterRestrictionTag(LineData.CharacterRestriction);
		}
	}

	static void MigrateCharacterRestrictionTag(FDialogueMultiLineNodeData& LineData)
	{
		for (FDialogueMultiLineEntry& Entry : LineData.Lines)
		{
			MigrateCharacterRestrictionTag(Entry.LineData);
		}
	}

	static void MigrateCompiledNodeCharacterRestrictions(FDialogueCompiledNode& Node)
	{
		if (FDialogueLineNodeData* LineData = Node.NodeData.GetMutablePtr<FDialogueLineNodeData>())
		{
			MigrateCharacterRestrictionTag(*LineData);
			return;
		}

		if (FDialogueMultiLineNodeData* MultiLineData = Node.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>())
		{
			MigrateCharacterRestrictionTag(*MultiLineData);
		}
	}
}

void UParleyConversationAsset::PostLoad()
{
	Super::PostLoad();

	MigrateCharacterRestrictionTag(Header);
	for (FDialogueCompiledNode& Node : CompiledData.Nodes)
	{
		MigrateCompiledNodeCharacterRestrictions(Node);
	}
}

bool UParleyConversationAsset::IsCompiledGraphValid() const
{
	return CompiledData.EnterNodeId.IsValid() && CompiledData.Nodes.Num() > 0 && !LastCompileValidation.HasErrors();
}

void UParleyConversationAsset::ClearCompiledData()
{
	CompiledData = FDialogueCompiledConversationData();
	LastCompileValidation = FDialogueValidationReport();
	bLastCompileSucceeded = false;
	CompileVersion = 0;
}

const FDialogueCompiledNode* UParleyConversationAsset::FindCompiledNode(const FGuid& NodeId) const
{
	for (const FDialogueCompiledNode& Node : CompiledData.Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}
	return nullptr;
}

FDialogueCompiledNode* UParleyConversationAsset::FindCompiledNodeMutable(const FGuid& NodeId)
{
	for (FDialogueCompiledNode& Node : CompiledData.Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}
	return nullptr;
}
