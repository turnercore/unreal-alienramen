#include "ParleyDialogueNodeDetailsCustomization.h"

#include "ParleyDialogueEdGraphNode.h"
#include "ParleyDialogueSettings.h"
#include "ParleyDialogueTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

namespace
{
	static bool ShouldDisplayLineAudioField(const FName FieldName)
	{
		const UParleyDialogueSettings* Settings = GetDefault<UParleyDialogueSettings>();
		if (!Settings)
		{
			return true;
		}

		const bool bSignalsMode = Settings->DialogueAudioMode == EParleyDialogueAudioMode::AudioSignals;
		const FName SoundField = GET_MEMBER_NAME_CHECKED(FDialogueConversationLine, Sound);
		const FName CueField = GET_MEMBER_NAME_CHECKED(FDialogueConversationLine, AudioCueTag);

		if (FieldName == SoundField)
		{
			return !bSignalsMode;
		}
		if (FieldName == CueField)
		{
			return bSignalsMode;
		}
		return true;
	}
}

TSharedRef<IDetailCustomization> FParleyDialogueEdGraphNodeDetails::MakeInstance()
{
	return MakeShared<FParleyDialogueEdGraphNodeDetails>();
}

void FParleyDialogueEdGraphNodeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	EDialogueEditorNodeType NodeType = EDialogueEditorNodeType::Line;
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
		for (const TWeakObjectPtr<UObject>& ObjectPtr : ObjectsBeingCustomized)
		{
			const UParleyDialogueEdGraphNode* NodeObject = Cast<UParleyDialogueEdGraphNode>(ObjectPtr.Get());
			if (NodeObject)
			{
				NodeType = NodeObject->EditorNodeType;
				break;
			}
		}
	}

	TSharedRef<IPropertyHandle> RuntimeNodeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UParleyDialogueEdGraphNode, RuntimeNode));
	DetailBuilder.HideProperty(RuntimeNodeHandle);
	DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UParleyDialogueEdGraphNode, EditorNodeType)));

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT(""), FText::GetEmpty(), ECategoryPriority::Important);

	auto AddRuntimeNodeField = [&Category, &RuntimeNodeHandle](const FName FieldName, const bool bAdvanced)
	{
		const TSharedPtr<IPropertyHandle> FieldHandle = RuntimeNodeHandle->GetChildHandle(FieldName);
		if (!FieldHandle.IsValid() || !FieldHandle->IsValidHandle())
		{
			return;
		}

		Category.AddProperty(FieldHandle.ToSharedRef(), bAdvanced ? EPropertyLocation::Advanced : EPropertyLocation::Default);
	};

	const TSharedPtr<IPropertyHandle> NodeDataHandle = RuntimeNodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NodeData));
	if (NodeDataHandle.IsValid() && NodeDataHandle->IsValidHandle())
	{
		TSharedPtr<IPropertyHandle> PayloadStructHandle;
		uint32 NumNodeDataChildren = 0;
		NodeDataHandle->GetNumChildren(NumNodeDataChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumNodeDataChildren; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> CandidateChild = NodeDataHandle->GetChildHandle(ChildIndex);
			if (!CandidateChild.IsValid() || !CandidateChild->IsValidHandle())
			{
				continue;
			}

			uint32 NumCandidateChildren = 0;
			CandidateChild->GetNumChildren(NumCandidateChildren);
			if (NumCandidateChildren > 0)
			{
				PayloadStructHandle = CandidateChild;
				break;
			}
		}

		if (!PayloadStructHandle.IsValid())
		{
			PayloadStructHandle = NodeDataHandle;
		}

		uint32 NumPayloadChildren = 0;
		PayloadStructHandle->GetNumChildren(NumPayloadChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumPayloadChildren; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = PayloadStructHandle->GetChildHandle(ChildIndex);
			if (!ChildHandle.IsValid() || !ChildHandle->IsValidHandle())
			{
				continue;
			}

			const FName ChildName = ChildHandle->GetProperty() ? ChildHandle->GetProperty()->GetFName() : NAME_None;
			const bool bFlattenInnerStruct = ChildName == GET_MEMBER_NAME_CHECKED(FDialogueLineNodeData, Line);
			const bool bSkipBranchInputs = NodeType == EDialogueEditorNodeType::Branch
				&& ChildName == GET_MEMBER_NAME_CHECKED(FDialogueEditorBranchNodeData, Inputs);

			if (bSkipBranchInputs)
			{
				continue;
			}

			if (bFlattenInnerStruct)
			{
				uint32 NumInnerChildren = 0;
				ChildHandle->GetNumChildren(NumInnerChildren);
				for (uint32 InnerIndex = 0; InnerIndex < NumInnerChildren; ++InnerIndex)
				{
					const TSharedPtr<IPropertyHandle> InnerHandle = ChildHandle->GetChildHandle(InnerIndex);
					if (InnerHandle.IsValid() && InnerHandle->IsValidHandle())
					{
						const FName InnerName = InnerHandle->GetProperty() ? InnerHandle->GetProperty()->GetFName() : NAME_None;
						if (!ShouldDisplayLineAudioField(InnerName))
						{
							continue;
						}

						IDetailPropertyRow& Row = Category.AddProperty(InnerHandle.ToSharedRef(), EPropertyLocation::Default);
						Row.ShouldAutoExpand(true);
					}
				}
				continue;
			}

			IDetailPropertyRow& Row = Category.AddProperty(ChildHandle.ToSharedRef(), EPropertyLocation::Default);
			Row.ShouldAutoExpand(true);
		}
	}

	AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, RandomBranches), false);

	const bool bHideCompileManagedBranchFields = NodeType == EDialogueEditorNodeType::Branch
		|| NodeType == EDialogueEditorNodeType::CheckTags
		|| NodeType == EDialogueEditorNodeType::CheckRelationship
		|| NodeType == EDialogueEditorNodeType::CheckProgress
		|| NodeType == EDialogueEditorNodeType::CheckLoadout
		|| NodeType == EDialogueEditorNodeType::CheckCharacter
		|| NodeType == EDialogueEditorNodeType::CheckVariable;

	if (NodeType != EDialogueEditorNodeType::Enter && NodeType != EDialogueEditorNodeType::Completed)
	{
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NodeType), true);
		if (!bHideCompileManagedBranchFields)
		{
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NextNodeId), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, TrueNodeId), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, FalseNodeId), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, ChoiceBranches), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, FallbackNodeId), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, FallbackChoiceText), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, bChoiceNodeImportant), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, CompletedChoicePolicy), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, SwitchBranches), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, SequenceBranches), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, CharacterRouteBranches), true);
		}

		if (NodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
		{
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, bSwitchHasDefaultOutput), true);
			AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, SwitchDefaultNodeId), true);
		}
	}
}

TSharedRef<IPropertyTypeCustomization> FParleyDialogueLineNodeDataCustomization::MakeInstance()
{
	return MakeShared<FParleyDialogueLineNodeDataCustomization>();
}

void FParleyDialogueLineNodeDataCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	(void)StructCustomizationUtils;

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FParleyDialogueLineNodeDataCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	(void)StructCustomizationUtils;

	const TSharedPtr<IPropertyHandle> LineHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDialogueLineNodeData, Line));
	if (LineHandle.IsValid() && LineHandle->IsValidHandle())
	{
		uint32 NumLineChildren = 0;
		LineHandle->GetNumChildren(NumLineChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumLineChildren; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = LineHandle->GetChildHandle(ChildIndex);
			if (ChildHandle.IsValid() && ChildHandle->IsValidHandle())
			{
				const FName ChildName = ChildHandle->GetProperty() ? ChildHandle->GetProperty()->GetFName() : NAME_None;
				if (!ShouldDisplayLineAudioField(ChildName))
				{
					continue;
				}

				ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			}
		}
	}

	const TSharedPtr<IPropertyHandle> SkipLockedHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDialogueLineNodeData, SkipLockedConditions));
	if (SkipLockedHandle.IsValid() && SkipLockedHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(SkipLockedHandle.ToSharedRef());
	}

	const TSharedPtr<IPropertyHandle> SkipBlockedHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDialogueLineNodeData, SkipBlockedConditions));
	if (SkipBlockedHandle.IsValid() && SkipBlockedHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(SkipBlockedHandle.ToSharedRef());
	}

	const TSharedPtr<IPropertyHandle> CharacterRestrictionHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDialogueLineNodeData, CharacterRestriction));
	if (CharacterRestrictionHandle.IsValid() && CharacterRestrictionHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(CharacterRestrictionHandle.ToSharedRef());
	}
}
