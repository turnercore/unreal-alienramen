#include "ARDialogueNodeDetailsCustomization.h"

#include "ARDialogueEdGraphNode.h"
#include "ARDialogueTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

TSharedRef<IDetailCustomization> FARDialogueEdGraphNodeDetails::MakeInstance()
{
	return MakeShared<FARDialogueEdGraphNodeDetails>();
}

void FARDialogueEdGraphNodeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	EDialogueNodeType NodeType = EDialogueNodeType::Line;
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
		for (const TWeakObjectPtr<UObject>& ObjectPtr : ObjectsBeingCustomized)
		{
			const UARDialogueEdGraphNode* NodeObject = Cast<UARDialogueEdGraphNode>(ObjectPtr.Get());
			if (NodeObject)
			{
				NodeType = NodeObject->RuntimeNode.NodeType;
				break;
			}
		}
	}

	TSharedRef<IPropertyHandle> RuntimeNodeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UARDialogueEdGraphNode, RuntimeNode));
	DetailBuilder.HideProperty(RuntimeNodeHandle);

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
			const bool bFlattenInnerStruct = ChildName == GET_MEMBER_NAME_CHECKED(FDialogueLineNodeData, Line)
				|| ChildName == GET_MEMBER_NAME_CHECKED(FDialogueBoolNodeData, Condition);

			if (bFlattenInnerStruct)
			{
				uint32 NumInnerChildren = 0;
				ChildHandle->GetNumChildren(NumInnerChildren);
				for (uint32 InnerIndex = 0; InnerIndex < NumInnerChildren; ++InnerIndex)
				{
					const TSharedPtr<IPropertyHandle> InnerHandle = ChildHandle->GetChildHandle(InnerIndex);
					if (InnerHandle.IsValid() && InnerHandle->IsValidHandle())
					{
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

	if (NodeType != EDialogueNodeType::Enter && NodeType != EDialogueNodeType::Completed)
	{
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NodeType), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, NextNodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, TrueNodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, FalseNodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, ChoiceBranches), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, FallbackNodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, FallbackChoiceText), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, bChoiceNodeImportant), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, CompletedChoicePolicy), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, SwitchBranches), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, bSwitchHasDefaultOutput), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, SwitchDefaultNodeId), true);
		AddRuntimeNodeField(GET_MEMBER_NAME_CHECKED(FDialogueCompiledNode, SequenceBranches), true);
	}
}

TSharedRef<IPropertyTypeCustomization> FARDialogueBoolNodeDataCustomization::MakeInstance()
{
	return MakeShared<FARDialogueBoolNodeDataCustomization>();
}

void FARDialogueBoolNodeDataCustomization::CustomizeHeader(
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

void FARDialogueBoolNodeDataCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	(void)StructCustomizationUtils;

	const TSharedPtr<IPropertyHandle> ConditionHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDialogueBoolNodeData, Condition));
	if (ConditionHandle.IsValid() && ConditionHandle->IsValidHandle())
	{
		uint32 NumConditionChildren = 0;
		ConditionHandle->GetNumChildren(NumConditionChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumConditionChildren; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = ConditionHandle->GetChildHandle(ChildIndex);
			if (ChildHandle.IsValid() && ChildHandle->IsValidHandle())
			{
				ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			}
		}
		return;
	}

	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex);
		if (ChildHandle.IsValid() && ChildHandle->IsValidHandle())
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

TSharedRef<IPropertyTypeCustomization> FARDialogueLineNodeDataCustomization::MakeInstance()
{
	return MakeShared<FARDialogueLineNodeDataCustomization>();
}

void FARDialogueLineNodeDataCustomization::CustomizeHeader(
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

void FARDialogueLineNodeDataCustomization::CustomizeChildren(
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
}
