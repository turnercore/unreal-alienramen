#include "ARDialogueEdGraphNode.h"

#include "EdGraph/EdGraphPin.h"
#include "Logging/MessageLog.h"

namespace
{
	static const FName PinCategoryExec(TEXT("DialogueExec"));
	static const FString ChoicePinPrefix(TEXT("Choice_"));
	static const FString SwitchPinPrefix(TEXT("Switch_"));
	static const FString RandomPinPrefix(TEXT("Random_"));

	static FEdGraphPinType MakeExecPinType()
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategoryExec;
		return PinType;
	}

	static FText BuildNodeTypeText(const EDialogueNodeType NodeType)
	{
		switch (NodeType)
		{
		case EDialogueNodeType::Enter:
			return FText::FromString(TEXT("Enter"));
		case EDialogueNodeType::Completed:
			return FText::FromString(TEXT("Completed"));
		case EDialogueNodeType::Line:
			return FText::FromString(TEXT("Line"));
		case EDialogueNodeType::Choice:
			return FText::FromString(TEXT("Choice"));
		case EDialogueNodeType::Bool:
			return FText::FromString(TEXT("Bool"));
		case EDialogueNodeType::SwitchOnTagsByPriority:
			return FText::FromString(TEXT("Switch Tags"));
		case EDialogueNodeType::TagMutation:
			return FText::FromString(TEXT("Tag Mutation"));
		case EDialogueNodeType::RelationshipMutation:
			return FText::FromString(TEXT("Relationship"));
		case EDialogueNodeType::FactionMutation:
			return FText::FromString(TEXT("Faction"));
		case EDialogueNodeType::Random:
			return FText::FromString(TEXT("Random"));
		default:
			return FText::FromString(TEXT("Unknown"));
		}
	}
}

FName UARDialogueEdGraphNode::GetPinNameIn()
{
	return TEXT("In");
}

FName UARDialogueEdGraphNode::GetPinNameNext()
{
	return TEXT("Next");
}

FName UARDialogueEdGraphNode::GetPinNameTrue()
{
	return TEXT("True");
}

FName UARDialogueEdGraphNode::GetPinNameFalse()
{
	return TEXT("False");
}

FName UARDialogueEdGraphNode::GetPinNameFallback()
{
	return TEXT("Fallback");
}

FName UARDialogueEdGraphNode::GetPinNameSwitchDefault()
{
	return TEXT("SwitchDefault");
}

FName UARDialogueEdGraphNode::MakeChoicePinName(const FGuid& ChoiceBranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *ChoicePinPrefix, *ChoiceBranchId.ToString(EGuidFormats::Digits)));
}

FName UARDialogueEdGraphNode::MakeSwitchPinName(const FGuid& BranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *SwitchPinPrefix, *BranchId.ToString(EGuidFormats::Digits)));
}

FName UARDialogueEdGraphNode::MakeRandomPinName(const FGuid& BranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *RandomPinPrefix, *BranchId.ToString(EGuidFormats::Digits)));
}

void UARDialogueEdGraphNode::InitializeForNodeType(const EDialogueNodeType NodeType)
{
	RuntimeNode = FDialogueCompiledNode();
	RuntimeNode.NodeType = NodeType;
	RuntimeNode.NodeId = FGuid::NewGuid();
	RuntimeNode.FallbackChoiceText = FText::FromString(TEXT("..."));
	RuntimeNode.CompletedChoicePolicy = EDialogueCompletedChoicePolicy::LockedToRecordedChoice;

	EnsureNodeDataMatchesNodeType();
	EnsureBranchAndLineIds(false, false);
}

void UARDialogueEdGraphNode::EnsureStableIds(const bool bRegenerateNodeId, const bool bRegenerateBranchIds)
{
	if (bRegenerateNodeId || !RuntimeNode.NodeId.IsValid())
	{
		RuntimeNode.NodeId = FGuid::NewGuid();
	}

	EnsureNodeDataMatchesNodeType();
	EnsureBranchAndLineIds(bRegenerateBranchIds, bRegenerateNodeId || bRegenerateBranchIds);
}

void UARDialogueEdGraphNode::ClearRuntimeLinks()
{
	RuntimeNode.NextNodeId.Invalidate();
	RuntimeNode.TrueNodeId.Invalidate();
	RuntimeNode.FalseNodeId.Invalidate();
	RuntimeNode.FallbackNodeId.Invalidate();
	RuntimeNode.SwitchDefaultNodeId.Invalidate();

	for (FDialogueCompiledChoiceBranch& Branch : RuntimeNode.ChoiceBranches)
	{
		Branch.NextNodeId.Invalidate();
	}
	for (FDialogueCompiledSwitchBranch& Branch : RuntimeNode.SwitchBranches)
	{
		Branch.NextNodeId.Invalidate();
	}
	for (FDialogueCompiledRandomBranch& Branch : RuntimeNode.RandomBranches)
	{
		Branch.NextNodeId.Invalidate();
	}
}

UEdGraphPin* UARDialogueEdGraphNode::GetExecInputPin() const
{
	return FindPin(GetPinNameIn(), EGPD_Input);
}

UEdGraphPin* UARDialogueEdGraphNode::GetOutputPinByName(const FName PinName) const
{
	return FindPin(PinName, EGPD_Output);
}

UEdGraphPin* UARDialogueEdGraphNode::GetChoiceOutputPin(const FGuid& ChoiceBranchId) const
{
	return FindPin(MakeChoicePinName(ChoiceBranchId), EGPD_Output);
}

UEdGraphPin* UARDialogueEdGraphNode::GetSwitchOutputPin(const FGuid& BranchId) const
{
	return FindPin(MakeSwitchPinName(BranchId), EGPD_Output);
}

UEdGraphPin* UARDialogueEdGraphNode::GetRandomOutputPin(const FGuid& BranchId) const
{
	return FindPin(MakeRandomPinName(BranchId), EGPD_Output);
}

void UARDialogueEdGraphNode::AddInputPinIfNeeded()
{
	CreatePin(EGPD_Input, MakeExecPinType(), GetPinNameIn());
}

void UARDialogueEdGraphNode::AddNextOutputPinIfNeeded()
{
	CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameNext());
}

void UARDialogueEdGraphNode::AddChoicePins()
{
	for (int32 Index = 0; Index < RuntimeNode.ChoiceBranches.Num(); ++Index)
	{
		FDialogueCompiledChoiceBranch& Branch = RuntimeNode.ChoiceBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeChoicePinName(Branch.ChoiceBranchId));
		if (Pin)
		{
			Pin->PinFriendlyName = Branch.ChoiceText.IsEmpty()
				? FText::FromString(FString::Printf(TEXT("Choice %d"), Index + 1))
				: Branch.ChoiceText;
		}
	}

	UEdGraphPin* FallbackPin = CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameFallback());
	if (FallbackPin)
	{
		FallbackPin->PinFriendlyName = RuntimeNode.FallbackChoiceText.IsEmpty()
			? FText::FromString(TEXT("..."))
			: RuntimeNode.FallbackChoiceText;
	}
}

void UARDialogueEdGraphNode::AddSwitchPins()
{
	for (int32 Index = 0; Index < RuntimeNode.SwitchBranches.Num(); ++Index)
	{
		const FDialogueCompiledSwitchBranch& Branch = RuntimeNode.SwitchBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeSwitchPinName(Branch.BranchId));
		if (Pin)
		{
			Pin->PinFriendlyName = Branch.Label.IsEmpty()
				? FText::FromString(FString::Printf(TEXT("Branch %d"), Index + 1))
				: Branch.Label;
		}
	}

	if (RuntimeNode.bSwitchHasDefaultOutput)
	{
		UEdGraphPin* DefaultPin = CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameSwitchDefault());
		if (DefaultPin)
		{
			DefaultPin->PinFriendlyName = FText::FromString(TEXT("Default"));
		}
	}
}

void UARDialogueEdGraphNode::AddRandomPins()
{
	for (int32 Index = 0; Index < RuntimeNode.RandomBranches.Num(); ++Index)
	{
		const FDialogueCompiledRandomBranch& Branch = RuntimeNode.RandomBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeRandomPinName(Branch.BranchId));
		if (Pin)
		{
			Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("Branch %d (w=%.2f)"), Index + 1, Branch.Weight));
		}
	}
}

void UARDialogueEdGraphNode::AllocateDefaultPins()
{
	EnsureStableIds(false, false);

	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Enter:
		AddNextOutputPinIfNeeded();
		break;
	case EDialogueNodeType::Completed:
		AddInputPinIfNeeded();
		break;
	case EDialogueNodeType::Line:
	case EDialogueNodeType::TagMutation:
	case EDialogueNodeType::RelationshipMutation:
	case EDialogueNodeType::FactionMutation:
		AddInputPinIfNeeded();
		AddNextOutputPinIfNeeded();
		break;
	case EDialogueNodeType::Choice:
		AddInputPinIfNeeded();
		AddChoicePins();
		break;
	case EDialogueNodeType::Bool:
		AddInputPinIfNeeded();
		CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameTrue());
		CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameFalse());
		break;
	case EDialogueNodeType::SwitchOnTagsByPriority:
		AddInputPinIfNeeded();
		AddSwitchPins();
		break;
	case EDialogueNodeType::Random:
		AddInputPinIfNeeded();
		AddRandomPins();
		break;
	default:
		AddInputPinIfNeeded();
		AddNextOutputPinIfNeeded();
		break;
	}
}

FText UARDialogueEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	(void)TitleType;
	const FText TypeTitle = BuildNodeTypeText(RuntimeNode.NodeType);
	const FString InlineSummary = BuildInlineSummary();
	if (InlineSummary.IsEmpty())
	{
		return TypeTitle;
	}

	return FText::FromString(FString::Printf(TEXT("%s\n%s"), *TypeTitle.ToString(), *InlineSummary));
}

FText UARDialogueEdGraphNode::GetTooltipText() const
{
	if (!ValidationMessage.IsEmpty())
	{
		return FText::FromString(ValidationMessage);
	}

	return FText::FromString(BuildInlineSummary());
}

FLinearColor UARDialogueEdGraphNode::GetNodeTitleColor() const
{
	if (ValidationSeverity == EDialogueValidationSeverity::Error)
	{
		return FLinearColor(0.8f, 0.1f, 0.1f, 1.0f);
	}
	if (ValidationSeverity == EDialogueValidationSeverity::Warning)
	{
		return FLinearColor(0.85f, 0.55f, 0.1f, 1.0f);
	}

	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Enter:
		return FLinearColor(0.1f, 0.45f, 0.8f, 1.0f);
	case EDialogueNodeType::Completed:
		return FLinearColor(0.15f, 0.55f, 0.2f, 1.0f);
	case EDialogueNodeType::Choice:
	case EDialogueNodeType::Bool:
	case EDialogueNodeType::SwitchOnTagsByPriority:
	case EDialogueNodeType::Random:
		return FLinearColor(0.18f, 0.18f, 0.5f, 1.0f);
	default:
		return FLinearColor(0.28f, 0.28f, 0.28f, 1.0f);
	}
}

void UARDialogueEdGraphNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	SetFlags(RF_Transactional);
	CreateNewGuid();
	EnsureStableIds(false, false);
}

void UARDialogueEdGraphNode::PostPasteNode()
{
	Super::PostPasteNode();
	CreateNewGuid();
	EnsureStableIds(true, true);
	ReconstructNode();
}

void UARDialogueEdGraphNode::PrepareForCopying()
{
	Super::PrepareForCopying();
}

#if WITH_EDITOR
void UARDialogueEdGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EnsureStableIds(false, false);
	ReconstructNode();
	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
}
#endif

void UARDialogueEdGraphNode::ApplyValidation(const EDialogueValidationSeverity Severity, const FString& Message)
{
	ValidationSeverity = Severity;
	ValidationMessage = Message;

	if (Severity == EDialogueValidationSeverity::Error || Severity == EDialogueValidationSeverity::Warning)
	{
		ErrorMsg = Message;
		ErrorType = (Severity == EDialogueValidationSeverity::Error)
			? static_cast<int32>(EMessageSeverity::Error)
			: static_cast<int32>(EMessageSeverity::Warning);
	}
	else
	{
		ErrorMsg.Empty();
		ErrorType = static_cast<int32>(EMessageSeverity::Info);
	}
}

void UARDialogueEdGraphNode::ClearValidation()
{
	ValidationSeverity = EDialogueValidationSeverity::Info;
	ValidationMessage.Empty();
	ErrorMsg.Empty();
	ErrorType = static_cast<int32>(EMessageSeverity::Info);
}

void UARDialogueEdGraphNode::EnsureNodeDataMatchesNodeType()
{
	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Line:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueLineNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueLineNodeData>();
		}
		break;
	case EDialogueNodeType::Bool:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueBoolNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueBoolNodeData>();
		}
		break;
	case EDialogueNodeType::TagMutation:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueTagMutationNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueTagMutationNodeData>();
		}
		break;
	case EDialogueNodeType::RelationshipMutation:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueRelationshipMutationNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueRelationshipMutationNodeData>();
		}
		break;
	case EDialogueNodeType::FactionMutation:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueFactionMutationNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueFactionMutationNodeData>();
		}
		break;
	default:
		RuntimeNode.NodeData.Reset();
		break;
	}
}

void UARDialogueEdGraphNode::EnsureBranchAndLineIds(const bool bRegenerateBranches, const bool bRegenerateLineGuid)
{
	if (RuntimeNode.NodeType == EDialogueNodeType::Line)
	{
		if (FDialogueLineNodeData* LineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>())
		{
			if (bRegenerateLineGuid || !LineData->Line.LocalLineGuid.IsValid())
			{
				LineData->Line.LocalLineGuid = FGuid::NewGuid();
			}
		}
	}

	if (RuntimeNode.NodeType == EDialogueNodeType::Choice)
	{
		if (RuntimeNode.FallbackChoiceText.IsEmpty())
		{
			RuntimeNode.FallbackChoiceText = FText::FromString(TEXT("..."));
		}

		TSet<FGuid> SeenBranchIds;
		for (int32 Index = 0; Index < RuntimeNode.ChoiceBranches.Num(); ++Index)
		{
			FDialogueCompiledChoiceBranch& Branch = RuntimeNode.ChoiceBranches[Index];
			if (bRegenerateBranches || !Branch.ChoiceBranchId.IsValid() || SeenBranchIds.Contains(Branch.ChoiceBranchId))
			{
				Branch.ChoiceBranchId = FGuid::NewGuid();
			}
			SeenBranchIds.Add(Branch.ChoiceBranchId);
		}
	}

	if (RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority)
	{
		TSet<FGuid> SeenBranchIds;
		for (FDialogueCompiledSwitchBranch& Branch : RuntimeNode.SwitchBranches)
		{
			if (bRegenerateBranches || !Branch.BranchId.IsValid() || SeenBranchIds.Contains(Branch.BranchId))
			{
				Branch.BranchId = FGuid::NewGuid();
			}
			SeenBranchIds.Add(Branch.BranchId);
		}
	}

	if (RuntimeNode.NodeType == EDialogueNodeType::Random)
	{
		TSet<FGuid> SeenBranchIds;
		for (FDialogueCompiledRandomBranch& Branch : RuntimeNode.RandomBranches)
		{
			if (bRegenerateBranches || !Branch.BranchId.IsValid() || SeenBranchIds.Contains(Branch.BranchId))
			{
				Branch.BranchId = FGuid::NewGuid();
			}
			SeenBranchIds.Add(Branch.BranchId);
		}
	}
}

FString UARDialogueEdGraphNode::BuildInlineSummary() const
{
	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Line:
	{
		const FDialogueLineNodeData* LineData = RuntimeNode.NodeData.GetPtr<FDialogueLineNodeData>();
		if (!LineData)
		{
			return TEXT("Invalid line payload");
		}

		const FString Text = LineData->Line.Text.ToString().Left(64);
		return FString::Printf(TEXT("[%s] \"%s\" SkipLocked:%d SkipBlocked:%d"),
			*LineData->Line.SpeakerTag.ToString(),
			*Text,
			LineData->SkipLockedConditions.Conditions.Num(),
			LineData->SkipBlockedConditions.Conditions.Num());
	}
	case EDialogueNodeType::Choice:
		return FString::Printf(TEXT("Choices:%d Fallback:\"%s\" Important:%s"),
			RuntimeNode.ChoiceBranches.Num(),
			*RuntimeNode.FallbackChoiceText.ToString(),
			RuntimeNode.bChoiceNodeImportant ? TEXT("Y") : TEXT("N"));
	case EDialogueNodeType::Bool:
	{
		const FDialogueBoolNodeData* BoolData = RuntimeNode.NodeData.GetPtr<FDialogueBoolNodeData>();
		if (!BoolData)
		{
			return TEXT("Invalid bool payload");
		}
		return FString::Printf(TEXT("Cond Source:%d Op:%d"),
			static_cast<int32>(BoolData->Condition.Source),
			static_cast<int32>(BoolData->Condition.Operator));
	}
	case EDialogueNodeType::SwitchOnTagsByPriority:
		return FString::Printf(TEXT("Branches:%d Default:%s"),
			RuntimeNode.SwitchBranches.Num(),
			RuntimeNode.bSwitchHasDefaultOutput ? TEXT("Y") : TEXT("N"));
	case EDialogueNodeType::TagMutation:
	{
		const FDialogueTagMutationNodeData* MutationData = RuntimeNode.NodeData.GetPtr<FDialogueTagMutationNodeData>();
		return MutationData ? FString::Printf(TEXT("Mutations:%d"), MutationData->Mutations.Num()) : TEXT("Invalid tag mutation payload");
	}
	case EDialogueNodeType::RelationshipMutation:
	{
		const FDialogueRelationshipMutationNodeData* MutationData = RuntimeNode.NodeData.GetPtr<FDialogueRelationshipMutationNodeData>();
		return MutationData
			? FString::Printf(TEXT("Target:%s Delta:%+.2f"), *MutationData->TargetSpeakerTag.ToString(), MutationData->DeltaPoints)
			: TEXT("Invalid relationship payload");
	}
	case EDialogueNodeType::FactionMutation:
	{
		const FDialogueFactionMutationNodeData* MutationData = RuntimeNode.NodeData.GetPtr<FDialogueFactionMutationNodeData>();
		return MutationData
			? FString::Printf(TEXT("Faction:%s Delta:%+.2f"), *MutationData->FactionTag.ToString(), MutationData->DeltaPopularity)
			: TEXT("Invalid faction payload");
	}
	case EDialogueNodeType::Random:
		return FString::Printf(TEXT("Branches:%d"), RuntimeNode.RandomBranches.Num());
	default:
		return FString();
	}
}
