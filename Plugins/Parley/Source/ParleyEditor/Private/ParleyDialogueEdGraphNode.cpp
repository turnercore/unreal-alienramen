#include "ParleyDialogueEdGraphNode.h"

#include "EdGraphUtilities.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Logging/MessageLog.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ParleyDialogueEdGraphNode"

namespace
{
	static float SanitizeInlineLineLengthSeconds(const float InLengthSeconds)
	{
		return FMath::Max(0.0f, InLengthSeconds);
	}

	static const FName PinCategoryExec(TEXT("DialogueExec"));
	static const FName PinCategoryConditionBool(TEXT("DialogueConditionBool"));
	static const FString ConditionPinPrefix(TEXT("Condition_"));
	static const FString ChoicePinPrefix(TEXT("Choice_"));
	static const FString SwitchPinPrefix(TEXT("Switch_"));
	static const FString RandomPinPrefix(TEXT("Random_"));
	static const FString SequencePinPrefix(TEXT("Sequence_"));
	static const FString CharacterRoutePinPrefix(TEXT("CharacterRoute_"));

	static FEdGraphPinType MakeExecPinType()
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategoryExec;
		return PinType;
	}

	static FEdGraphPinType MakeConditionBoolPinType()
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategoryConditionBool;
		return PinType;
	}

	static FText BuildNodeTypeText(const EDialogueEditorNodeType NodeType)
	{
		switch (NodeType)
		{
		case EDialogueEditorNodeType::Enter:
			return FText::FromString(TEXT("Enter"));
		case EDialogueEditorNodeType::Completed:
			return FText::FromString(TEXT("Completed"));
		case EDialogueEditorNodeType::Line:
			return FText::FromString(TEXT("Line"));
		case EDialogueEditorNodeType::Choice:
			return FText::FromString(TEXT("Choice"));
		case EDialogueEditorNodeType::Branch:
			return FText::FromString(TEXT("Branch"));
		case EDialogueEditorNodeType::SwitchOnTagsByPriority:
			return FText::FromString(TEXT("Switch Tags"));
		case EDialogueEditorNodeType::TagMutation:
			return FText::FromString(TEXT("Tag Mutation"));
		case EDialogueEditorNodeType::RelationshipMutation:
			return FText::FromString(TEXT("Relationship"));
		case EDialogueEditorNodeType::FactionMutation:
			return FText::FromString(TEXT("Faction"));
		case EDialogueEditorNodeType::Signal:
			return FText::FromString(TEXT("Signal"));
		case EDialogueEditorNodeType::Random:
			return FText::FromString(TEXT("Random"));
		case EDialogueEditorNodeType::Route:
			return FText::FromString(TEXT("Route"));
		case EDialogueEditorNodeType::Sequence:
			return FText::FromString(TEXT("Sequence"));
		case EDialogueEditorNodeType::MultiLine:
			return FText::FromString(TEXT("Multi-Line"));
		case EDialogueEditorNodeType::SplitLine:
			return FText::FromString(TEXT("Split Line"));
		case EDialogueEditorNodeType::RouteByCharacter:
			return FText::FromString(TEXT("Route Character"));
		case EDialogueEditorNodeType::CheckTags:
			return FText::FromString(TEXT("Check Tags"));
		case EDialogueEditorNodeType::CheckRelationship:
			return FText::FromString(TEXT("Check Relationship"));
		case EDialogueEditorNodeType::CheckProgress:
			return FText::FromString(TEXT("Check Progress"));
		case EDialogueEditorNodeType::CheckStats:
			return FText::FromString(TEXT("Check Stats"));
		case EDialogueEditorNodeType::CheckLoadout:
			return FText::FromString(TEXT("Check Loadout"));
		case EDialogueEditorNodeType::CheckCharacter:
			return FText::FromString(TEXT("Check Character"));
		case EDialogueEditorNodeType::CheckVariable:
			return FText::FromString(TEXT("Check Variable"));
		default:
			return FText::FromString(TEXT("Unknown"));
		}
	}

	static FString BuildNodeTypeTooltip(const EDialogueEditorNodeType NodeType)
	{
		switch (NodeType)
		{
		case EDialogueEditorNodeType::Enter:
			return TEXT("Entry point for this conversation graph.");
		case EDialogueEditorNodeType::Completed:
			return TEXT("Conversation completion node.");
		case EDialogueEditorNodeType::Line:
			return TEXT("Single spoken line.");
		case EDialogueEditorNodeType::Choice:
			return TEXT("Player choice branch node.");
		case EDialogueEditorNodeType::Branch:
			return TEXT("Conditional branch node driven by connected bool source nodes.");
		case EDialogueEditorNodeType::SwitchOnTagsByPriority:
			return TEXT("Priority switch branch node.");
		case EDialogueEditorNodeType::TagMutation:
			return TEXT("Gameplay tag mutation node.");
		case EDialogueEditorNodeType::RelationshipMutation:
			return TEXT("Relationship mutation node.");
		case EDialogueEditorNodeType::FactionMutation:
			return TEXT("Faction mutation node.");
		case EDialogueEditorNodeType::Signal:
			return TEXT("Fires a gameplay tag signal for game systems to react to.");
		case EDialogueEditorNodeType::Random:
			return TEXT("Weighted random branch node.");
		case EDialogueEditorNodeType::Route:
			return TEXT("Flow routing helper node.");
		case EDialogueEditorNodeType::Sequence:
			return TEXT("Sequential branch node.");
		case EDialogueEditorNodeType::MultiLine:
			return TEXT("Multi-line sequence node.");
		case EDialogueEditorNodeType::SplitLine:
			return TEXT("Split-line variant node.");
		case EDialogueEditorNodeType::RouteByCharacter:
			return TEXT("Route by active player character.");
		case EDialogueEditorNodeType::CheckTags:
			return TEXT("Evaluates a tag-based dialogue condition and outputs a bool.");
		case EDialogueEditorNodeType::CheckRelationship:
			return TEXT("Evaluates relationship points or level and outputs a bool.");
		case EDialogueEditorNodeType::CheckProgress:
			return TEXT("Evaluates dialogue seen/completed progress and outputs a bool.");
		case EDialogueEditorNodeType::CheckStats:
			return TEXT("Evaluates runtime player stats and outputs a bool.");
		case EDialogueEditorNodeType::CheckLoadout:
			return TEXT("Evaluates active loadout tags and outputs a bool.");
		case EDialogueEditorNodeType::CheckCharacter:
			return TEXT("Evaluates the active player character and outputs a bool.");
		case EDialogueEditorNodeType::CheckVariable:
			return TEXT("Evaluates an injected runtime variable and outputs a bool.");
		default:
			return TEXT("Dialogue node.");
		}
	}
}

FName UParleyDialogueEdGraphNode::GetPinNameIn()
{
	return TEXT("In");
}

FName UParleyDialogueEdGraphNode::GetPinNameNext()
{
	return TEXT("Next");
}

FName UParleyDialogueEdGraphNode::GetPinNameTrue()
{
	return TEXT("True");
}

FName UParleyDialogueEdGraphNode::GetPinNameFalse()
{
	return TEXT("False");
}

FName UParleyDialogueEdGraphNode::GetPinNameFallback()
{
	return TEXT("Fallback");
}

FName UParleyDialogueEdGraphNode::GetPinNameSwitchDefault()
{
	return TEXT("SwitchDefault");
}

FName UParleyDialogueEdGraphNode::MakeConditionPinName(const FGuid& InputId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *ConditionPinPrefix, *InputId.ToString(EGuidFormats::Digits)));
}

FName UParleyDialogueEdGraphNode::MakeChoicePinName(const FGuid& ChoiceBranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *ChoicePinPrefix, *ChoiceBranchId.ToString(EGuidFormats::Digits)));
}

FName UParleyDialogueEdGraphNode::MakeSwitchPinName(const FGuid& BranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *SwitchPinPrefix, *BranchId.ToString(EGuidFormats::Digits)));
}

FName UParleyDialogueEdGraphNode::MakeRandomPinName(const FGuid& BranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *RandomPinPrefix, *BranchId.ToString(EGuidFormats::Digits)));
}

FName UParleyDialogueEdGraphNode::MakeSequencePinName(const FGuid& BranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *SequencePinPrefix, *BranchId.ToString(EGuidFormats::Digits)));
}

FName UParleyDialogueEdGraphNode::MakeCharacterRoutePinName(const FGuid& BranchId)
{
	return FName(*FString::Printf(TEXT("%s%s"), *CharacterRoutePinPrefix, *BranchId.ToString(EGuidFormats::Digits)));
}

EDialogueEditorNodeType UParleyDialogueEdGraphNode::MakeEditorNodeTypeFromRuntime(const EDialogueNodeType NodeType)
{
	switch (NodeType)
	{
	case EDialogueNodeType::Enter:
		return EDialogueEditorNodeType::Enter;
	case EDialogueNodeType::Completed:
		return EDialogueEditorNodeType::Completed;
	case EDialogueNodeType::Line:
		return EDialogueEditorNodeType::Line;
	case EDialogueNodeType::Choice:
		return EDialogueEditorNodeType::Choice;
	case EDialogueNodeType::SwitchOnTagsByPriority:
		return EDialogueEditorNodeType::SwitchOnTagsByPriority;
	case EDialogueNodeType::TagMutation:
		return EDialogueEditorNodeType::TagMutation;
	case EDialogueNodeType::RelationshipMutation:
		return EDialogueEditorNodeType::RelationshipMutation;
	case EDialogueNodeType::FactionMutation:
		return EDialogueEditorNodeType::FactionMutation;
	case EDialogueNodeType::Random:
		return EDialogueEditorNodeType::Random;
	case EDialogueNodeType::Route:
		return EDialogueEditorNodeType::Route;
	case EDialogueNodeType::Sequence:
		return EDialogueEditorNodeType::Sequence;
	case EDialogueNodeType::MultiLine:
		return EDialogueEditorNodeType::MultiLine;
	case EDialogueNodeType::SplitLine:
		return EDialogueEditorNodeType::SplitLine;
	case EDialogueNodeType::RouteByCharacter:
		return EDialogueEditorNodeType::RouteByCharacter;
	case EDialogueNodeType::Signal:
		return EDialogueEditorNodeType::Signal;
	default:
		return EDialogueEditorNodeType::Route;
	}
}

EDialogueNodeType UParleyDialogueEdGraphNode::MakeRuntimeNodeTypeFromEditor(const EDialogueEditorNodeType NodeType)
{
	switch (NodeType)
	{
	case EDialogueEditorNodeType::Enter:
		return EDialogueNodeType::Enter;
	case EDialogueEditorNodeType::Completed:
		return EDialogueNodeType::Completed;
	case EDialogueEditorNodeType::Line:
		return EDialogueNodeType::Line;
	case EDialogueEditorNodeType::Choice:
		return EDialogueNodeType::Choice;
	case EDialogueEditorNodeType::Branch:
		return EDialogueNodeType::SwitchOnTagsByPriority;
	case EDialogueEditorNodeType::SwitchOnTagsByPriority:
		return EDialogueNodeType::SwitchOnTagsByPriority;
	case EDialogueEditorNodeType::TagMutation:
		return EDialogueNodeType::TagMutation;
	case EDialogueEditorNodeType::RelationshipMutation:
		return EDialogueNodeType::RelationshipMutation;
	case EDialogueEditorNodeType::FactionMutation:
		return EDialogueNodeType::FactionMutation;
	case EDialogueEditorNodeType::Random:
		return EDialogueNodeType::Random;
	case EDialogueEditorNodeType::Route:
		return EDialogueNodeType::Route;
	case EDialogueEditorNodeType::Sequence:
		return EDialogueNodeType::Sequence;
	case EDialogueEditorNodeType::MultiLine:
		return EDialogueNodeType::MultiLine;
	case EDialogueEditorNodeType::SplitLine:
		return EDialogueNodeType::SplitLine;
	case EDialogueEditorNodeType::RouteByCharacter:
		return EDialogueNodeType::RouteByCharacter;
	case EDialogueEditorNodeType::Signal:
		return EDialogueNodeType::Signal;
	case EDialogueEditorNodeType::CheckTags:
	case EDialogueEditorNodeType::CheckRelationship:
	case EDialogueEditorNodeType::CheckProgress:
	case EDialogueEditorNodeType::CheckStats:
	case EDialogueEditorNodeType::CheckLoadout:
	case EDialogueEditorNodeType::CheckCharacter:
	case EDialogueEditorNodeType::CheckVariable:
	default:
		return EDialogueNodeType::Route;
	}
}

void UParleyDialogueEdGraphNode::InitializeForNodeType(const EDialogueEditorNodeType NodeType)
{
	EditorNodeType = NodeType;
	RuntimeNode = FDialogueCompiledNode();
	RuntimeNode.NodeType = MakeRuntimeNodeTypeFromEditor(NodeType);
	RuntimeNode.NodeId = FGuid::NewGuid();
	RuntimeNode.FallbackChoiceText = FText::FromString(TEXT("..."));
	RuntimeNode.CompletedChoicePolicy = EDialogueCompletedChoicePolicy::LockedToRecordedChoice;

	EnsureNodeDataMatchesNodeType();
	EnsureBranchAndLineIds(false, false);
}

void UParleyDialogueEdGraphNode::EnsureStableIds(const bool bRegenerateNodeId, const bool bRegenerateBranchIds)
{
	if (bRegenerateNodeId || !RuntimeNode.NodeId.IsValid())
	{
		RuntimeNode.NodeId = FGuid::NewGuid();
	}

	if (EditorNodeType == EDialogueEditorNodeType::Line && RuntimeNode.NodeType != EDialogueNodeType::Line)
	{
		EditorNodeType = MakeEditorNodeTypeFromRuntime(RuntimeNode.NodeType);
	}

	RuntimeNode.NodeType = MakeRuntimeNodeTypeFromEditor(EditorNodeType);
	EnsureNodeDataMatchesNodeType();
	EnsureBranchAndLineIds(bRegenerateBranchIds, bRegenerateNodeId || bRegenerateBranchIds);
}

void UParleyDialogueEdGraphNode::ClearRuntimeLinks()
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
	for (FDialogueCompiledSequenceBranch& Branch : RuntimeNode.SequenceBranches)
	{
		Branch.NextNodeId.Invalidate();
	}
	for (FDialogueCompiledCharacterRouteBranch& Branch : RuntimeNode.CharacterRouteBranches)
	{
		Branch.NextNodeId.Invalidate();
	}
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetExecInputPin() const
{
	return FindPin(GetPinNameIn(), EGPD_Input);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetOutputPinByName(const FName PinName) const
{
	return FindPin(PinName, EGPD_Output);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetConditionInputPin(const FGuid& InputId) const
{
	return FindPin(MakeConditionPinName(InputId), EGPD_Input);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetChoiceOutputPin(const FGuid& ChoiceBranchId) const
{
	return FindPin(MakeChoicePinName(ChoiceBranchId), EGPD_Output);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetSwitchOutputPin(const FGuid& BranchId) const
{
	return FindPin(MakeSwitchPinName(BranchId), EGPD_Output);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetRandomOutputPin(const FGuid& BranchId) const
{
	return FindPin(MakeRandomPinName(BranchId), EGPD_Output);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetSequenceOutputPin(const FGuid& BranchId) const
{
	return FindPin(MakeSequencePinName(BranchId), EGPD_Output);
}

UEdGraphPin* UParleyDialogueEdGraphNode::GetCharacterRouteOutputPin(const FGuid& BranchId) const
{
	return FindPin(MakeCharacterRoutePinName(BranchId), EGPD_Output);
}

bool UParleyDialogueEdGraphNode::SupportsDynamicBranchPins() const
{
	return EditorNodeType == EDialogueEditorNodeType::Choice
		|| EditorNodeType == EDialogueEditorNodeType::Branch
		|| EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority
		|| EditorNodeType == EDialogueEditorNodeType::Random
		|| EditorNodeType == EDialogueEditorNodeType::Sequence
		|| EditorNodeType == EDialogueEditorNodeType::RouteByCharacter;
}

void UParleyDialogueEdGraphNode::AddDynamicBranchPin()
{
	if (!SupportsDynamicBranchPins())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddDialogueBranchPin", "Add Dialogue Branch Pin"));
	Modify();

	UEdGraphPin* NewPin = nullptr;
	switch (EditorNodeType)
	{
	case EDialogueEditorNodeType::Choice:
	{
		FDialogueCompiledChoiceBranch NewBranch;
		NewBranch.ChoiceBranchId = FGuid::NewGuid();
		NewBranch.ChoiceText = FText::FromString(FString::Printf(TEXT("Choice %d"), RuntimeNode.ChoiceBranches.Num() + 1));
		const FDialogueCompiledChoiceBranch& AddedBranch = RuntimeNode.ChoiceBranches.Add_GetRef(MoveTemp(NewBranch));
		NewPin = CreatePin(EGPD_Output, MakeExecPinType(), MakeChoicePinName(AddedBranch.ChoiceBranchId));
		if (NewPin)
		{
			NewPin->PinFriendlyName = AddedBranch.ChoiceText;
		}
		break;
	}
	case EDialogueEditorNodeType::Branch:
	{
		FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorBranchNodeData>();
		if (!BranchData)
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorBranchNodeData>();
			BranchData = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorBranchNodeData>();
		}

		FDialogueEditorConditionInput NewInput;
		NewInput.InputId = FGuid::NewGuid();
		const FDialogueEditorConditionInput& AddedInput = BranchData->Inputs.Add_GetRef(NewInput);
		NewPin = CreatePin(EGPD_Input, MakeConditionBoolPinType(), MakeConditionPinName(AddedInput.InputId));
		if (NewPin)
		{
			NewPin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("Condition %d"), BranchData->Inputs.Num()));
		}
		break;
	}
	case EDialogueEditorNodeType::SwitchOnTagsByPriority:
	{
		FDialogueCompiledSwitchBranch NewBranch;
		NewBranch.BranchId = FGuid::NewGuid();
		NewBranch.Label = FText::FromString(FString::Printf(TEXT("Branch %d"), RuntimeNode.SwitchBranches.Num() + 1));
		const FDialogueCompiledSwitchBranch& AddedBranch = RuntimeNode.SwitchBranches.Add_GetRef(MoveTemp(NewBranch));
		NewPin = CreatePin(EGPD_Output, MakeExecPinType(), MakeSwitchPinName(AddedBranch.BranchId));
		if (NewPin)
		{
			NewPin->PinFriendlyName = AddedBranch.Label;
		}
		break;
	}
	case EDialogueEditorNodeType::Random:
	{
		FDialogueCompiledRandomBranch NewBranch;
		NewBranch.BranchId = FGuid::NewGuid();
		NewBranch.Weight = 1.0f;
		const FDialogueCompiledRandomBranch& AddedBranch = RuntimeNode.RandomBranches.Add_GetRef(MoveTemp(NewBranch));
		NewPin = CreatePin(EGPD_Output, MakeExecPinType(), MakeRandomPinName(AddedBranch.BranchId));
		if (NewPin)
		{
			NewPin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("%d"), RuntimeNode.RandomBranches.Num()));
		}
		break;
	}
	case EDialogueEditorNodeType::Sequence:
	{
		FDialogueCompiledSequenceBranch NewBranch;
		NewBranch.BranchId = FGuid::NewGuid();
		const FDialogueCompiledSequenceBranch& AddedBranch = RuntimeNode.SequenceBranches.Add_GetRef(MoveTemp(NewBranch));
		NewPin = CreatePin(EGPD_Output, MakeExecPinType(), MakeSequencePinName(AddedBranch.BranchId));
		if (NewPin)
		{
			NewPin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("Then %d"), RuntimeNode.SequenceBranches.Num()));
		}
		break;
	}
	case EDialogueEditorNodeType::RouteByCharacter:
	{
		FDialogueCompiledCharacterRouteBranch NewBranch;
		NewBranch.BranchId = FGuid::NewGuid();
		const FDialogueCompiledCharacterRouteBranch& AddedBranch = RuntimeNode.CharacterRouteBranches.Add_GetRef(MoveTemp(NewBranch));
		NewPin = CreatePin(EGPD_Output, MakeExecPinType(), MakeCharacterRoutePinName(AddedBranch.BranchId));
		if (NewPin)
		{
			NewPin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("Character %d"), RuntimeNode.CharacterRouteBranches.Num()));
		}
		break;
	}
	default:
		return;
	}

	EnsureStableIds(false, false);

	if (NewPin)
	{
		NewPin->bHidden = false;
		NewPin->bAdvancedView = false;
	}

	if (UEdGraph* Graph = GetGraph())
	{
		Graph->Modify();
		Graph->NotifyNodeChanged(this);
		Graph->NotifyGraphChanged();
		if (UObject* GraphOuter = Graph->GetOuter())
		{
			GraphOuter->MarkPackageDirty();
		}
	}

	MarkPackageDirty();
}

bool UParleyDialogueEdGraphNode::RemoveLastDynamicBranchPin()
{
	if (!SupportsDynamicBranchPins())
	{
		return false;
	}

	switch (EditorNodeType)
	{
	case EDialogueEditorNodeType::Choice:
		if (RuntimeNode.ChoiceBranches.IsEmpty())
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeChoicePinName(RuntimeNode.ChoiceBranches.Last().ChoiceBranchId));
	case EDialogueEditorNodeType::Branch:
	{
		const FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetPtr<FDialogueEditorBranchNodeData>();
		if (!BranchData || BranchData->Inputs.Num() <= 1)
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeConditionPinName(BranchData->Inputs.Last().InputId));
	}
	case EDialogueEditorNodeType::SwitchOnTagsByPriority:
		if (RuntimeNode.SwitchBranches.IsEmpty())
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeSwitchPinName(RuntimeNode.SwitchBranches.Last().BranchId));
	case EDialogueEditorNodeType::Random:
		if (RuntimeNode.RandomBranches.Num() <= 1)
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeRandomPinName(RuntimeNode.RandomBranches.Last().BranchId));
	case EDialogueEditorNodeType::Sequence:
		if (RuntimeNode.SequenceBranches.Num() <= 1)
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeSequencePinName(RuntimeNode.SequenceBranches.Last().BranchId));
	case EDialogueEditorNodeType::RouteByCharacter:
		if (RuntimeNode.CharacterRouteBranches.IsEmpty())
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeCharacterRoutePinName(RuntimeNode.CharacterRouteBranches.Last().BranchId));
	default:
		return false;
	}
}

bool UParleyDialogueEdGraphNode::RemoveDynamicBranchPinByName(const FName PinName)
{
	if (!SupportsDynamicBranchPins())
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("RemoveDialogueBranchPin", "Remove Dialogue Branch Pin"),
		[this, PinName]() -> bool
		{
			FGuid BranchId;
			if (EditorNodeType == EDialogueEditorNodeType::Choice
				&& TryParseBranchGuidFromPinName(PinName, ChoicePinPrefix, BranchId))
			{
				const int32 RemovedCount = RuntimeNode.ChoiceBranches.RemoveAll([BranchId](const FDialogueCompiledChoiceBranch& Branch)
				{
					return Branch.ChoiceBranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (EditorNodeType == EDialogueEditorNodeType::Branch
				&& TryParseBranchGuidFromPinName(PinName, ConditionPinPrefix, BranchId))
			{
				FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorBranchNodeData>();
				if (!BranchData || BranchData->Inputs.Num() <= 1)
				{
					return false;
				}

				const int32 RemovedCount = BranchData->Inputs.RemoveAll([BranchId](const FDialogueEditorConditionInput& Input)
				{
					return Input.InputId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority
				&& TryParseBranchGuidFromPinName(PinName, SwitchPinPrefix, BranchId))
			{
				const int32 RemovedCount = RuntimeNode.SwitchBranches.RemoveAll([BranchId](const FDialogueCompiledSwitchBranch& Branch)
				{
					return Branch.BranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (EditorNodeType == EDialogueEditorNodeType::Random
				&& TryParseBranchGuidFromPinName(PinName, RandomPinPrefix, BranchId))
			{
				if (RuntimeNode.RandomBranches.Num() <= 1)
				{
					return false;
				}

				const int32 RemovedCount = RuntimeNode.RandomBranches.RemoveAll([BranchId](const FDialogueCompiledRandomBranch& Branch)
				{
					return Branch.BranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (EditorNodeType == EDialogueEditorNodeType::Sequence
				&& TryParseBranchGuidFromPinName(PinName, SequencePinPrefix, BranchId))
			{
				if (RuntimeNode.SequenceBranches.Num() <= 1)
				{
					return false;
				}

				const int32 RemovedCount = RuntimeNode.SequenceBranches.RemoveAll([BranchId](const FDialogueCompiledSequenceBranch& Branch)
				{
					return Branch.BranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (EditorNodeType == EDialogueEditorNodeType::RouteByCharacter
				&& TryParseBranchGuidFromPinName(PinName, CharacterRoutePinPrefix, BranchId))
			{
				const int32 RemovedCount = RuntimeNode.CharacterRouteBranches.RemoveAll([BranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
				{
					return Branch.BranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			return false;
		},
		true);
}

bool UParleyDialogueEdGraphNode::SetBranchMatchMode(const EDialogueConditionMatchMode NewMatchMode)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetBranchMatchMode", "Set Branch Match Mode"),
		[this, NewMatchMode]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::Branch)
			{
				return false;
			}

			FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorBranchNodeData>();
			if (!BranchData || BranchData->MatchMode == NewMatchMode)
			{
				return false;
			}

			BranchData->MatchMode = NewMatchMode;
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::SetChoiceBranchText(const FGuid& ChoiceBranchId, const FText& NewText)
{
	const bool bChanged = CommitRuntimeNodeMutation(
		LOCTEXT("SetChoiceBranchText", "Set Choice Branch Text"),
		[this, ChoiceBranchId, NewText]() -> bool
		{
			for (FDialogueCompiledChoiceBranch& Branch : RuntimeNode.ChoiceBranches)
			{
				if (Branch.ChoiceBranchId != ChoiceBranchId)
				{
					continue;
				}

				if (Branch.ChoiceText.EqualTo(NewText))
				{
					return false;
				}

				Branch.ChoiceText = NewText;
				return true;
			}

			return false;
		},
		true);

	if (bChanged)
	{
		if (UEdGraphPin* Pin = GetChoiceOutputPin(ChoiceBranchId))
		{
			Pin->PinFriendlyName = NewText;
		}
	}
	return bChanged;
}

bool UParleyDialogueEdGraphNode::SetSwitchBranchLabel(const FGuid& BranchId, const FText& NewLabel)
{
	const bool bChanged = CommitRuntimeNodeMutation(
		LOCTEXT("SetSwitchBranchLabel", "Set Switch Branch Label"),
		[this, BranchId, NewLabel]() -> bool
		{
			for (FDialogueCompiledSwitchBranch& Branch : RuntimeNode.SwitchBranches)
			{
				if (Branch.BranchId != BranchId)
				{
					continue;
				}

				if (Branch.Label.EqualTo(NewLabel))
				{
					return false;
				}

				Branch.Label = NewLabel;
				return true;
			}

			return false;
		},
		true);

	if (bChanged)
	{
		if (UEdGraphPin* Pin = GetSwitchOutputPin(BranchId))
		{
			Pin->PinFriendlyName = NewLabel;
		}
	}
	return bChanged;
}

bool UParleyDialogueEdGraphNode::SetCharacterRouteBranchSpeakerTag(const FGuid& BranchId, const FGameplayTag& NewSpeakerTag)
{
	const bool bChanged = CommitRuntimeNodeMutation(
		LOCTEXT("SetCharacterRouteBranchSpeakerTag", "Set Character Route Branch Speaker Tag"),
		[this, BranchId, NewSpeakerTag]() -> bool
		{
			for (FDialogueCompiledCharacterRouteBranch& Branch : RuntimeNode.CharacterRouteBranches)
			{
				if (Branch.BranchId != BranchId)
				{
					continue;
				}

				if (Branch.SpeakerTag.MatchesTagExact(NewSpeakerTag))
				{
					return false;
				}

				Branch.SpeakerTag = NewSpeakerTag;
				return true;
			}

			return false;
		},
		true);

	if (bChanged)
	{
		if (UEdGraphPin* Pin = GetCharacterRouteOutputPin(BranchId))
		{
			const int32 BranchIndex = RuntimeNode.CharacterRouteBranches.IndexOfByPredicate([BranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
			{
				return Branch.BranchId == BranchId;
			});
			const FDialogueCompiledCharacterRouteBranch* Branch = RuntimeNode.CharacterRouteBranches.FindByPredicate([BranchId](const FDialogueCompiledCharacterRouteBranch& Candidate)
			{
				return Candidate.BranchId == BranchId;
			});
			Pin->PinFriendlyName = Branch && Branch->SpeakerTag.IsValid()
				? FText::FromString(Branch->SpeakerTag.ToString())
				: FText::FromString(FString::Printf(TEXT("Character %d"), BranchIndex + 1));
		}
	}
	return bChanged;
}

bool UParleyDialogueEdGraphNode::SetRandomBranchWeight(const FGuid& BranchId, const float NewWeight)
{
	const bool bChanged = CommitRuntimeNodeMutation(
		LOCTEXT("SetRandomBranchWeight", "Set Random Branch Weight"),
		[this, BranchId, NewWeight]() -> bool
		{
			for (FDialogueCompiledRandomBranch& Branch : RuntimeNode.RandomBranches)
			{
				if (Branch.BranchId != BranchId)
				{
					continue;
				}

				if (FMath::IsNearlyEqual(Branch.Weight, NewWeight))
				{
					return false;
				}

				Branch.Weight = NewWeight;
				return true;
			}

			return false;
		},
		true);

	if (bChanged)
	{
		const int32 BranchIndex = RuntimeNode.RandomBranches.IndexOfByPredicate([BranchId](const FDialogueCompiledRandomBranch& Branch)
		{
			return Branch.BranchId == BranchId;
		});
		if (BranchIndex != INDEX_NONE)
		{
			if (UEdGraphPin* Pin = GetRandomOutputPin(BranchId))
			{
				Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("%d"), BranchIndex + 1));
			}
		}
	}
	return bChanged;
}

bool UParleyDialogueEdGraphNode::MoveChoiceBranch(const FGuid& ChoiceBranchId, const bool bMoveUp)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("MoveChoiceBranch", "Reorder Choice Branch"),
		[this, ChoiceBranchId, bMoveUp]() -> bool
		{
			const int32 CurrentIndex = RuntimeNode.ChoiceBranches.IndexOfByPredicate([ChoiceBranchId](const FDialogueCompiledChoiceBranch& Branch)
			{
				return Branch.ChoiceBranchId == ChoiceBranchId;
			});
			if (CurrentIndex == INDEX_NONE)
			{
				return false;
			}

			const int32 TargetIndex = bMoveUp ? CurrentIndex - 1 : CurrentIndex + 1;
			if (!RuntimeNode.ChoiceBranches.IsValidIndex(TargetIndex))
			{
				return false;
			}

			RuntimeNode.ChoiceBranches.Swap(CurrentIndex, TargetIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::MoveSwitchBranch(const FGuid& BranchId, const bool bMoveUp)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("MoveSwitchBranch", "Reorder Switch Branch"),
		[this, BranchId, bMoveUp]() -> bool
		{
			const int32 CurrentIndex = RuntimeNode.SwitchBranches.IndexOfByPredicate([BranchId](const FDialogueCompiledSwitchBranch& Branch)
			{
				return Branch.BranchId == BranchId;
			});
			if (CurrentIndex == INDEX_NONE)
			{
				return false;
			}

			const int32 TargetIndex = bMoveUp ? CurrentIndex - 1 : CurrentIndex + 1;
			if (!RuntimeNode.SwitchBranches.IsValidIndex(TargetIndex))
			{
				return false;
			}

			RuntimeNode.SwitchBranches.Swap(CurrentIndex, TargetIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::MoveCharacterRouteBranch(const FGuid& BranchId, const bool bMoveUp)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("MoveCharacterRouteBranch", "Reorder Character Route Branch"),
		[this, BranchId, bMoveUp]() -> bool
		{
			const int32 CurrentIndex = RuntimeNode.CharacterRouteBranches.IndexOfByPredicate([BranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
			{
				return Branch.BranchId == BranchId;
			});
			if (CurrentIndex == INDEX_NONE)
			{
				return false;
			}

			const int32 TargetIndex = bMoveUp ? CurrentIndex - 1 : CurrentIndex + 1;
			if (!RuntimeNode.CharacterRouteBranches.IsValidIndex(TargetIndex))
			{
				return false;
			}

			RuntimeNode.CharacterRouteBranches.Swap(CurrentIndex, TargetIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::ReorderChoiceBranch(const FGuid& MovingChoiceBranchId, const FGuid& TargetChoiceBranchId)
{
	if (!MovingChoiceBranchId.IsValid() || !TargetChoiceBranchId.IsValid() || MovingChoiceBranchId == TargetChoiceBranchId)
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("ReorderChoiceBranch", "Reorder Choice Branch"),
		[this, MovingChoiceBranchId, TargetChoiceBranchId]() -> bool
		{
			const int32 SourceIndex = RuntimeNode.ChoiceBranches.IndexOfByPredicate([MovingChoiceBranchId](const FDialogueCompiledChoiceBranch& Branch)
			{
				return Branch.ChoiceBranchId == MovingChoiceBranchId;
			});
			const int32 TargetIndex = RuntimeNode.ChoiceBranches.IndexOfByPredicate([TargetChoiceBranchId](const FDialogueCompiledChoiceBranch& Branch)
			{
				return Branch.ChoiceBranchId == TargetChoiceBranchId;
			});
			if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)
			{
				return false;
			}

			FDialogueCompiledChoiceBranch MovingBranch = MoveTemp(RuntimeNode.ChoiceBranches[SourceIndex]);
			RuntimeNode.ChoiceBranches.RemoveAt(SourceIndex);
			const int32 InsertIndex = SourceIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
			RuntimeNode.ChoiceBranches.Insert(MoveTemp(MovingBranch), InsertIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::ReorderSwitchBranch(const FGuid& MovingBranchId, const FGuid& TargetBranchId)
{
	if (!MovingBranchId.IsValid() || !TargetBranchId.IsValid() || MovingBranchId == TargetBranchId)
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("ReorderSwitchBranch", "Reorder Switch Branch"),
		[this, MovingBranchId, TargetBranchId]() -> bool
		{
			const int32 SourceIndex = RuntimeNode.SwitchBranches.IndexOfByPredicate([MovingBranchId](const FDialogueCompiledSwitchBranch& Branch)
			{
				return Branch.BranchId == MovingBranchId;
			});
			const int32 TargetIndex = RuntimeNode.SwitchBranches.IndexOfByPredicate([TargetBranchId](const FDialogueCompiledSwitchBranch& Branch)
			{
				return Branch.BranchId == TargetBranchId;
			});
			if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)
			{
				return false;
			}

			FDialogueCompiledSwitchBranch MovingBranch = MoveTemp(RuntimeNode.SwitchBranches[SourceIndex]);
			RuntimeNode.SwitchBranches.RemoveAt(SourceIndex);
			const int32 InsertIndex = SourceIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
			RuntimeNode.SwitchBranches.Insert(MoveTemp(MovingBranch), InsertIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::ReorderRandomBranch(const FGuid& MovingBranchId, const FGuid& TargetBranchId)
{
	if (!MovingBranchId.IsValid() || !TargetBranchId.IsValid() || MovingBranchId == TargetBranchId)
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("ReorderRandomBranch", "Reorder Random Branch"),
		[this, MovingBranchId, TargetBranchId]() -> bool
		{
			const int32 SourceIndex = RuntimeNode.RandomBranches.IndexOfByPredicate([MovingBranchId](const FDialogueCompiledRandomBranch& Branch)
			{
				return Branch.BranchId == MovingBranchId;
			});
			const int32 TargetIndex = RuntimeNode.RandomBranches.IndexOfByPredicate([TargetBranchId](const FDialogueCompiledRandomBranch& Branch)
			{
				return Branch.BranchId == TargetBranchId;
			});
			if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)
			{
				return false;
			}

			FDialogueCompiledRandomBranch MovingBranch = MoveTemp(RuntimeNode.RandomBranches[SourceIndex]);
			RuntimeNode.RandomBranches.RemoveAt(SourceIndex);
			const int32 InsertIndex = SourceIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
			RuntimeNode.RandomBranches.Insert(MoveTemp(MovingBranch), InsertIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::ReorderCharacterRouteBranch(const FGuid& MovingBranchId, const FGuid& TargetBranchId)
{
	if (!MovingBranchId.IsValid() || !TargetBranchId.IsValid() || MovingBranchId == TargetBranchId)
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("ReorderCharacterRouteBranch", "Reorder Character Route Branch"),
		[this, MovingBranchId, TargetBranchId]() -> bool
		{
			const int32 SourceIndex = RuntimeNode.CharacterRouteBranches.IndexOfByPredicate([MovingBranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
			{
				return Branch.BranchId == MovingBranchId;
			});
			const int32 TargetIndex = RuntimeNode.CharacterRouteBranches.IndexOfByPredicate([TargetBranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
			{
				return Branch.BranchId == TargetBranchId;
			});
			if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)
			{
				return false;
			}

			FDialogueCompiledCharacterRouteBranch MovingBranch = MoveTemp(RuntimeNode.CharacterRouteBranches[SourceIndex]);
			RuntimeNode.CharacterRouteBranches.RemoveAt(SourceIndex);
			const int32 InsertIndex = SourceIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
			RuntimeNode.CharacterRouteBranches.Insert(MoveTemp(MovingBranch), InsertIndex);
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::SetChoiceFallbackText(const FText& NewFallbackText)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetChoiceFallbackText", "Set Choice Fallback Text"),
		[this, NewFallbackText]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::Choice || RuntimeNode.FallbackChoiceText.EqualTo(NewFallbackText))
			{
				return false;
			}

			RuntimeNode.FallbackChoiceText = NewFallbackText;
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::SetRelationshipSourceSpeakerTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetRelationshipSourceSpeakerTag", "Set Relationship Source Speaker Tag"),
		[this, NewTag]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::RelationshipMutation)
			{
				return false;
			}

			FDialogueRelationshipMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueRelationshipMutationNodeData>();
			if (!MutationData || MutationData->SourceSpeakerTag.MatchesTagExact(NewTag))
			{
				return false;
			}

			MutationData->SourceSpeakerTag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetRelationshipTargetSpeakerTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetRelationshipSpeakerTag", "Set Relationship Speaker Tag"),
		[this, NewTag]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::RelationshipMutation)
			{
				return false;
			}

			FDialogueRelationshipMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueRelationshipMutationNodeData>();
			if (!MutationData || MutationData->TargetSpeakerTag.MatchesTagExact(NewTag))
			{
				return false;
			}

			MutationData->TargetSpeakerTag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetRelationshipDeltaPoints(const float NewDeltaPoints)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetRelationshipDelta", "Set Relationship Delta"),
		[this, NewDeltaPoints]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::RelationshipMutation)
			{
				return false;
			}

			FDialogueRelationshipMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueRelationshipMutationNodeData>();
			if (!MutationData || FMath::IsNearlyEqual(MutationData->DeltaPoints, NewDeltaPoints))
			{
				return false;
			}

			MutationData->DeltaPoints = NewDeltaPoints;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetFactionTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetFactionTag", "Set Faction Tag"),
		[this, NewTag]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::FactionMutation)
			{
				return false;
			}

			FDialogueFactionMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueFactionMutationNodeData>();
			if (!MutationData || MutationData->FactionTag.MatchesTagExact(NewTag))
			{
				return false;
			}

			MutationData->FactionTag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetFactionDeltaPopularity(const float NewDeltaPopularity)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetFactionDeltaPopularity", "Set Faction Popularity Delta"),
		[this, NewDeltaPopularity]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::FactionMutation)
			{
				return false;
			}

			FDialogueFactionMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueFactionMutationNodeData>();
			if (!MutationData || FMath::IsNearlyEqual(MutationData->DeltaPopularity, NewDeltaPopularity))
			{
				return false;
			}

			MutationData->DeltaPopularity = NewDeltaPopularity;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetFactionTargetSpeakerTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetFactionTargetSpeakerTag", "Set Faction Target Speaker Tag"),
		[this, NewTag]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::FactionMutation)
			{
				return false;
			}

			FDialogueFactionMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueFactionMutationNodeData>();
			if (!MutationData || MutationData->TargetSpeakerTag.MatchesTagExact(NewTag))
			{
				return false;
			}

			MutationData->TargetSpeakerTag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetFactionDeltaSpeakerReputation(const float NewDeltaSpeakerReputation)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetFactionDeltaSpeakerReputation", "Set Faction Speaker Reputation Delta"),
		[this, NewDeltaSpeakerReputation]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::FactionMutation)
			{
				return false;
			}

			FDialogueFactionMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueFactionMutationNodeData>();
			if (!MutationData || FMath::IsNearlyEqual(MutationData->DeltaSpeakerReputation, NewDeltaSpeakerReputation))
			{
				return false;
			}

			MutationData->DeltaSpeakerReputation = NewDeltaSpeakerReputation;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetPrimaryTagMutationTarget(const EDialogueTagMutationTarget NewTarget)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetPrimaryTagMutationTarget", "Set Tag Mutation Target"),
		[this, NewTarget]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::TagMutation)
			{
				return false;
			}

			FDialogueTagMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueTagMutationNodeData>();
			if (!MutationData)
			{
				return false;
			}

			if (MutationData->Mutations.IsEmpty())
			{
				MutationData->Mutations.AddDefaulted();
			}

			FDialogueTagMutation& Mutation = MutationData->Mutations[0];
			if (Mutation.Target == NewTarget)
			{
				return false;
			}

			Mutation.Target = NewTarget;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetPrimaryTagMutationOperation(const EDialogueTagMutationOp NewOperation)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetPrimaryTagMutationOperation", "Set Tag Mutation Operation"),
		[this, NewOperation]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::TagMutation)
			{
				return false;
			}

			FDialogueTagMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueTagMutationNodeData>();
			if (!MutationData)
			{
				return false;
			}

			if (MutationData->Mutations.IsEmpty())
			{
				MutationData->Mutations.AddDefaulted();
			}

			FDialogueTagMutation& Mutation = MutationData->Mutations[0];
			if (Mutation.Operation == NewOperation)
			{
				return false;
			}

			Mutation.Operation = NewOperation;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetPrimaryTagMutationTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetPrimaryTagMutationTag", "Set Tag Mutation Tag"),
		[this, NewTag]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::TagMutation)
			{
				return false;
			}

			FDialogueTagMutationNodeData* MutationData = RuntimeNode.NodeData.GetMutablePtr<FDialogueTagMutationNodeData>();
			if (!MutationData)
			{
				return false;
			}

			if (MutationData->Mutations.IsEmpty())
			{
				MutationData->Mutations.AddDefaulted();
			}

			FDialogueTagMutation& Mutation = MutationData->Mutations[0];
			if (Mutation.Tag.MatchesTagExact(NewTag))
			{
				return false;
			}

			Mutation.Tag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckTagsSource(const EDialogueEditorTagConditionSource NewSource)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckTagsSource", "Set Check Tags Source"),
		[this, NewSource]() -> bool
		{
			FDialogueEditorCheckTagsNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckTagsNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckTags || !Data || Data->Source == NewSource)
			{
				return false;
			}

			Data->Source = NewSource;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckTagsOperator(const EDialogueComparisonOp NewOperator)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckTagsOperator", "Set Check Tags Operator"),
		[this, NewOperator]() -> bool
		{
			FDialogueEditorCheckTagsNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckTagsNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckTags || !Data || Data->Operator == NewOperator)
			{
				return false;
			}

			Data->Operator = NewOperator;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckTagsTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckTagsTag", "Set Check Tags Tag"),
		[this, NewTag]() -> bool
		{
			FDialogueEditorCheckTagsNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckTagsNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckTags || !Data || Data->TagValue.MatchesTagExact(NewTag))
			{
				return false;
			}

			Data->TagValue = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckRelationshipSource(const EDialogueEditorRelationshipConditionSource NewSource)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckRelationshipSource", "Set Check Relationship Source"),
		[this, NewSource]() -> bool
		{
			FDialogueEditorCheckRelationshipNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckRelationship || !Data || Data->Source == NewSource)
			{
				return false;
			}

			Data->Source = NewSource;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckRelationshipOperator(const EDialogueComparisonOp NewOperator)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckRelationshipOperator", "Set Check Relationship Operator"),
		[this, NewOperator]() -> bool
		{
			FDialogueEditorCheckRelationshipNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckRelationship || !Data || Data->Operator == NewOperator)
			{
				return false;
			}

			Data->Operator = NewOperator;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckRelationshipTargetSpeakerTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckRelationshipTargetSpeakerTag", "Set Check Relationship Speaker"),
		[this, NewTag]() -> bool
		{
			FDialogueEditorCheckRelationshipNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckRelationship || !Data || Data->TargetSpeakerTag.MatchesTagExact(NewTag))
			{
				return false;
			}

			Data->TargetSpeakerTag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckRelationshipFactionTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckRelationshipFactionTag", "Set Check Relationship Faction"),
		[this, NewTag]() -> bool
		{
			FDialogueEditorCheckRelationshipNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckRelationship || !Data || Data->FactionTag.MatchesTagExact(NewTag))
			{
				return false;
			}

			Data->FactionTag = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckRelationshipNumericValue(const float NewValue)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckRelationshipNumericValue", "Set Check Relationship Value"),
		[this, NewValue]() -> bool
		{
			FDialogueEditorCheckRelationshipNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckRelationship || !Data || FMath::IsNearlyEqual(Data->NumericValue, NewValue))
			{
				return false;
			}

			Data->NumericValue = NewValue;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckProgressSource(const EDialogueEditorProgressConditionSource NewSource)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckProgressSource", "Set Check Progress Source"),
		[this, NewSource]() -> bool
		{
			FDialogueEditorCheckProgressNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckProgressNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckProgress || !Data || Data->Source == NewSource)
			{
				return false;
			}

			Data->Source = NewSource;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckProgressOperator(const EDialogueComparisonOp NewOperator)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckProgressOperator", "Set Check Progress Operator"),
		[this, NewOperator]() -> bool
		{
			FDialogueEditorCheckProgressNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckProgressNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckProgress || !Data || Data->Operator == NewOperator)
			{
				return false;
			}

			Data->Operator = NewOperator;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckProgressExpectedValue(const bool bExpectedValue)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckProgressExpectedValue", "Set Check Progress Expected Value"),
		[this, bExpectedValue]() -> bool
		{
			FDialogueEditorCheckProgressNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckProgressNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckProgress || !Data || Data->bExpectedValue == bExpectedValue)
			{
				return false;
			}

			Data->bExpectedValue = bExpectedValue;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckLoadoutOperator(const EDialogueComparisonOp NewOperator)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckLoadoutOperator", "Set Check Loadout Operator"),
		[this, NewOperator]() -> bool
		{
			FDialogueEditorCheckLoadoutNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckLoadoutNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckLoadout || !Data || Data->Operator == NewOperator)
			{
				return false;
			}

			Data->Operator = NewOperator;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckLoadoutTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckLoadoutTag", "Set Check Loadout Tag"),
		[this, NewTag]() -> bool
		{
			FDialogueEditorCheckLoadoutNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckLoadoutNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckLoadout || !Data || Data->TagValue.MatchesTagExact(NewTag))
			{
				return false;
			}

			Data->TagValue = NewTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckCharacterTag(const FGameplayTag& NewCharacterTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckCharacterTag", "Set Check Character Tag"),
		[this, NewCharacterTag]() -> bool
		{
			FDialogueEditorCheckCharacterNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckCharacterNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckCharacter || !Data || Data->CharacterTag.MatchesTagExact(NewCharacterTag))
			{
				return false;
			}

			Data->CharacterTag = NewCharacterTag;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckVariableName(const FName NewVariableName)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckVariableName", "Set Check Variable Name"),
		[this, NewVariableName]() -> bool
		{
			FDialogueEditorCheckVariableNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckVariableNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckVariable || !Data || Data->VariableName == NewVariableName)
			{
				return false;
			}

			Data->VariableName = NewVariableName;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetCheckVariableOperator(const EDialogueComparisonOp NewOperator)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetCheckVariableOperator", "Set Check Variable Operator"),
		[this, NewOperator]() -> bool
		{
			FDialogueEditorCheckVariableNodeData* Data = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckVariableNodeData>();
			if (EditorNodeType != EDialogueEditorNodeType::CheckVariable || !Data || Data->Operator == NewOperator)
			{
				return false;
			}

			Data->Operator = NewOperator;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::AddMultiLineEntry()
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("AddMultiLineEntry", "Add Multi-Line Entry"),
		[this]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::MultiLine
				&& EditorNodeType != EDialogueEditorNodeType::SplitLine)
			{
				return false;
			}

			FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				return false;
			}

			FDialogueMultiLineEntry& NewEntry = MultiLineData->Lines.AddDefaulted_GetRef();
			NewEntry.EntryId = FGuid::NewGuid();
			NewEntry.LineData.SkipBlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
			NewEntry.LineData.Line.LocalLineGuid = FGuid::NewGuid();
			NewEntry.LineData.Line.LengthSeconds = 1.0f;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::RemoveMultiLineEntry(const FGuid& EntryId)
{
	if (!EntryId.IsValid())
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("RemoveMultiLineEntry", "Remove Multi-Line Entry"),
		[this, EntryId]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::MultiLine
				&& EditorNodeType != EDialogueEditorNodeType::SplitLine)
			{
				return false;
			}

			FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData || MultiLineData->Lines.Num() <= 1)
			{
				return false;
			}

			const int32 RemovedCount = MultiLineData->Lines.RemoveAll([EntryId](const FDialogueMultiLineEntry& Entry)
			{
				return Entry.EntryId == EntryId;
			});
			return RemovedCount > 0;
		},
		false);
}

bool UParleyDialogueEdGraphNode::ReorderMultiLineEntry(const FGuid& MovingEntryId, const FGuid& TargetEntryId)
{
	if (!MovingEntryId.IsValid() || !TargetEntryId.IsValid() || MovingEntryId == TargetEntryId)
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("ReorderMultiLineEntry", "Reorder Multi-Line Entry"),
		[this, MovingEntryId, TargetEntryId]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::MultiLine
				&& EditorNodeType != EDialogueEditorNodeType::SplitLine)
			{
				return false;
			}

			FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				return false;
			}

			const int32 SourceIndex = MultiLineData->Lines.IndexOfByPredicate([MovingEntryId](const FDialogueMultiLineEntry& Entry)
			{
				return Entry.EntryId == MovingEntryId;
			});
			const int32 TargetIndex = MultiLineData->Lines.IndexOfByPredicate([TargetEntryId](const FDialogueMultiLineEntry& Entry)
			{
				return Entry.EntryId == TargetEntryId;
			});
			if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)
			{
				return false;
			}

			FDialogueMultiLineEntry Moving = MoveTemp(MultiLineData->Lines[SourceIndex]);
			MultiLineData->Lines.RemoveAt(SourceIndex);
			const int32 InsertIndex = SourceIndex < TargetIndex ? TargetIndex - 1 : TargetIndex;
			MultiLineData->Lines.Insert(MoveTemp(Moving), InsertIndex);
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetMultiLineEntrySpeakerTag(const FGuid& EntryId, const FGameplayTag& NewSpeakerTag)
{
	if (!EntryId.IsValid())
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("SetMultiLineEntrySpeakerTag", "Set Multi-Line Speaker Tag"),
		[this, EntryId, NewSpeakerTag]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::MultiLine
				&& EditorNodeType != EDialogueEditorNodeType::SplitLine)
			{
				return false;
			}

			FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				return false;
			}

			for (FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
			{
				if (Entry.EntryId != EntryId)
				{
					continue;
				}
				if (Entry.LineData.Line.SpeakerTag.MatchesTagExact(NewSpeakerTag))
				{
					return false;
				}
				Entry.LineData.Line.SpeakerTag = NewSpeakerTag;
				return true;
			}
			return false;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetMultiLineEntryText(const FGuid& EntryId, const FText& NewText)
{
	if (!EntryId.IsValid())
	{
		return false;
	}

	return CommitRuntimeNodeMutation(
		LOCTEXT("SetMultiLineEntryText", "Set Multi-Line Text"),
		[this, EntryId, NewText]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::MultiLine
				&& EditorNodeType != EDialogueEditorNodeType::SplitLine)
			{
				return false;
			}

			FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				return false;
			}

			for (FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
			{
				if (Entry.EntryId != EntryId)
				{
					continue;
				}
				if (Entry.LineData.Line.Text.EqualTo(NewText))
				{
					return false;
				}
				Entry.LineData.Line.Text = NewText;
				return true;
			}
			return false;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetLineLengthSeconds(const float NewLengthSeconds)
{
	const float SanitizedLengthSeconds = SanitizeInlineLineLengthSeconds(NewLengthSeconds);
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetLineLengthSeconds", "Set Line Length Seconds"),
		[this, SanitizedLengthSeconds]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::Line)
			{
				return false;
			}

			FDialogueLineNodeData* LineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>();
			if (!LineData || FMath::IsNearlyEqual(LineData->Line.LengthSeconds, SanitizedLengthSeconds))
			{
				return false;
			}

			LineData->Line.LengthSeconds = SanitizedLengthSeconds;
			return true;
		},
		false);
}

bool UParleyDialogueEdGraphNode::SetMultiLineEntryLengthSeconds(const FGuid& EntryId, const float NewLengthSeconds)
{
	if (!EntryId.IsValid())
	{
		return false;
	}

	const float SanitizedLengthSeconds = SanitizeInlineLineLengthSeconds(NewLengthSeconds);
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetMultiLineEntryLengthSeconds", "Set Multi-Line Length Seconds"),
		[this, EntryId, SanitizedLengthSeconds]() -> bool
		{
			if (EditorNodeType != EDialogueEditorNodeType::MultiLine
				&& EditorNodeType != EDialogueEditorNodeType::SplitLine)
			{
				return false;
			}

			FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
			if (!MultiLineData)
			{
				return false;
			}

			for (FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
			{
				if (Entry.EntryId != EntryId)
				{
					continue;
				}

				if (FMath::IsNearlyEqual(Entry.LineData.Line.LengthSeconds, SanitizedLengthSeconds))
				{
					return false;
				}

				Entry.LineData.Line.LengthSeconds = SanitizedLengthSeconds;
				return true;
			}

			return false;
		},
		false);
}

void UParleyDialogueEdGraphNode::AddInputPinIfNeeded()
{
	CreatePin(EGPD_Input, MakeExecPinType(), GetPinNameIn());
}

void UParleyDialogueEdGraphNode::AddNextOutputPinIfNeeded()
{
	CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameNext());
}

void UParleyDialogueEdGraphNode::AddChoicePins()
{
	for (int32 Index = 0; Index < RuntimeNode.ChoiceBranches.Num(); ++Index)
	{
		FDialogueCompiledChoiceBranch& Branch = RuntimeNode.ChoiceBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeChoicePinName(Branch.ChoiceBranchId));
		if (Pin)
		{
			Pin->bHidden = false;
			Pin->bAdvancedView = false;
			Pin->PinFriendlyName = Branch.ChoiceText.IsEmpty()
				? FText::FromString(FString::Printf(TEXT("Choice %d"), Index + 1))
				: Branch.ChoiceText;
		}
	}

	UEdGraphPin* FallbackPin = CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameFallback());
	if (FallbackPin)
	{
		FallbackPin->bHidden = false;
		FallbackPin->bAdvancedView = false;
		FallbackPin->PinFriendlyName = RuntimeNode.FallbackChoiceText.IsEmpty()
			? FText::FromString(TEXT("..."))
			: RuntimeNode.FallbackChoiceText;
	}
}

void UParleyDialogueEdGraphNode::AddSwitchPins()
{
	for (int32 Index = 0; Index < RuntimeNode.SwitchBranches.Num(); ++Index)
	{
		const FDialogueCompiledSwitchBranch& Branch = RuntimeNode.SwitchBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeSwitchPinName(Branch.BranchId));
		if (Pin)
		{
			Pin->bHidden = false;
			Pin->bAdvancedView = false;
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
			DefaultPin->bHidden = false;
			DefaultPin->bAdvancedView = false;
			DefaultPin->PinFriendlyName = FText::FromString(TEXT("Default"));
		}
	}
}

void UParleyDialogueEdGraphNode::AddRandomPins()
{
	for (int32 Index = 0; Index < RuntimeNode.RandomBranches.Num(); ++Index)
	{
		const FDialogueCompiledRandomBranch& Branch = RuntimeNode.RandomBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeRandomPinName(Branch.BranchId));
		if (Pin)
		{
			Pin->bHidden = false;
			Pin->bAdvancedView = false;
			Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("%d"), Index + 1));
		}
	}
}

void UParleyDialogueEdGraphNode::AddSequencePins()
{
	for (int32 Index = 0; Index < RuntimeNode.SequenceBranches.Num(); ++Index)
	{
		const FDialogueCompiledSequenceBranch& Branch = RuntimeNode.SequenceBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeSequencePinName(Branch.BranchId));
		if (Pin)
		{
			Pin->bHidden = false;
			Pin->bAdvancedView = false;
			Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("Then %d"), Index + 1));
		}
	}
}

void UParleyDialogueEdGraphNode::AddCharacterRoutePins()
{
	for (int32 Index = 0; Index < RuntimeNode.CharacterRouteBranches.Num(); ++Index)
	{
		const FDialogueCompiledCharacterRouteBranch& Branch = RuntimeNode.CharacterRouteBranches[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Output, MakeExecPinType(), MakeCharacterRoutePinName(Branch.BranchId));
		if (Pin)
		{
			Pin->bHidden = false;
			Pin->bAdvancedView = false;
			Pin->PinFriendlyName = Branch.SpeakerTag.IsValid()
				? FText::FromString(Branch.SpeakerTag.ToString())
				: FText::FromString(FString::Printf(TEXT("Character %d"), Index + 1));
		}
	}
}

void UParleyDialogueEdGraphNode::AddConditionInputPins()
{
	const FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetPtr<FDialogueEditorBranchNodeData>();
	if (!BranchData)
	{
		return;
	}

	for (int32 Index = 0; Index < BranchData->Inputs.Num(); ++Index)
	{
		const FDialogueEditorConditionInput& Input = BranchData->Inputs[Index];
		UEdGraphPin* Pin = CreatePin(EGPD_Input, MakeConditionBoolPinType(), MakeConditionPinName(Input.InputId));
		if (Pin)
		{
			Pin->bHidden = false;
			Pin->bAdvancedView = false;
			Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("Condition %d"), Index + 1));
		}
	}
}

void UParleyDialogueEdGraphNode::AllocateDefaultPins()
{
	EnsureStableIds(false, false);

	switch (EditorNodeType)
	{
	case EDialogueEditorNodeType::Enter:
		AddNextOutputPinIfNeeded();
		break;
	case EDialogueEditorNodeType::Completed:
		AddInputPinIfNeeded();
		break;
	case EDialogueEditorNodeType::Line:
	case EDialogueEditorNodeType::MultiLine:
	case EDialogueEditorNodeType::SplitLine:
	case EDialogueEditorNodeType::TagMutation:
	case EDialogueEditorNodeType::RelationshipMutation:
	case EDialogueEditorNodeType::FactionMutation:
	case EDialogueEditorNodeType::Signal:
	case EDialogueEditorNodeType::Route:
		AddInputPinIfNeeded();
		AddNextOutputPinIfNeeded();
		break;
	case EDialogueEditorNodeType::Choice:
		AddInputPinIfNeeded();
		AddChoicePins();
		break;
	case EDialogueEditorNodeType::Branch:
		AddInputPinIfNeeded();
		AddConditionInputPins();
		CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameTrue());
		CreatePin(EGPD_Output, MakeExecPinType(), GetPinNameFalse());
		break;
	case EDialogueEditorNodeType::SwitchOnTagsByPriority:
		AddInputPinIfNeeded();
		AddSwitchPins();
		break;
	case EDialogueEditorNodeType::Random:
		AddInputPinIfNeeded();
		AddRandomPins();
		break;
	case EDialogueEditorNodeType::Sequence:
		AddInputPinIfNeeded();
		AddSequencePins();
		break;
	case EDialogueEditorNodeType::RouteByCharacter:
		AddInputPinIfNeeded();
		AddCharacterRoutePins();
		break;
	case EDialogueEditorNodeType::CheckTags:
	case EDialogueEditorNodeType::CheckRelationship:
	case EDialogueEditorNodeType::CheckProgress:
	case EDialogueEditorNodeType::CheckStats:
	case EDialogueEditorNodeType::CheckLoadout:
	case EDialogueEditorNodeType::CheckCharacter:
	case EDialogueEditorNodeType::CheckVariable:
		if (UEdGraphPin* OutputPin = CreatePin(EGPD_Output, MakeConditionBoolPinType(), GetPinNameTrue()))
		{
			OutputPin->PinFriendlyName = FText::FromString(TEXT("Bool"));
		}
		break;
	default:
		AddInputPinIfNeeded();
		AddNextOutputPinIfNeeded();
		break;
	}
}

bool UParleyDialogueEdGraphNode::CanUserDeleteNode() const
{
	return EditorNodeType != EDialogueEditorNodeType::Enter;
}

FText UParleyDialogueEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	(void)TitleType;
	return BuildNodeTypeText(EditorNodeType);
}

FText UParleyDialogueEdGraphNode::GetTooltipText() const
{
	if (!ValidationMessage.IsEmpty())
	{
		return FText::FromString(ValidationMessage);
	}

	const FString InlineSummary = BuildInlineSummary();
	if (InlineSummary.IsEmpty())
	{
		return FText::FromString(BuildNodeTypeTooltip(EditorNodeType));
	}

	return FText::FromString(FString::Printf(TEXT("%s\n%s"), *BuildNodeTypeTooltip(EditorNodeType), *InlineSummary));
}

FLinearColor UParleyDialogueEdGraphNode::GetNodeTitleColor() const
{
	if (ValidationSeverity == EDialogueValidationSeverity::Error)
	{
		return FLinearColor(0.8f, 0.1f, 0.1f, 1.0f);
	}
	if (ValidationSeverity == EDialogueValidationSeverity::Warning)
	{
		return FLinearColor(0.85f, 0.55f, 0.1f, 1.0f);
	}

	switch (EditorNodeType)
	{
	case EDialogueEditorNodeType::Enter:
		return FLinearColor(0.16f, 0.22f, 0.30f, 1.0f);
	case EDialogueEditorNodeType::Completed:
		return FLinearColor(0.16f, 0.27f, 0.18f, 1.0f);
	case EDialogueEditorNodeType::Line:
		return FLinearColor(0.14f, 0.21f, 0.23f, 1.0f);
	case EDialogueEditorNodeType::MultiLine:
		return FLinearColor(0.14f, 0.24f, 0.28f, 1.0f);
	case EDialogueEditorNodeType::SplitLine:
		return FLinearColor(0.13f, 0.25f, 0.31f, 1.0f);
	case EDialogueEditorNodeType::Choice:
		return FLinearColor(0.22f, 0.20f, 0.30f, 1.0f);
	case EDialogueEditorNodeType::Branch:
		return FLinearColor(0.19f, 0.20f, 0.27f, 1.0f);
	case EDialogueEditorNodeType::SwitchOnTagsByPriority:
		return FLinearColor(0.20f, 0.19f, 0.28f, 1.0f);
	case EDialogueEditorNodeType::Random:
		return FLinearColor(0.21f, 0.19f, 0.29f, 1.0f);
	case EDialogueEditorNodeType::Sequence:
		return FLinearColor(0.20f, 0.22f, 0.28f, 1.0f);
	case EDialogueEditorNodeType::TagMutation:
		return FLinearColor(0.16f, 0.24f, 0.17f, 1.0f);
	case EDialogueEditorNodeType::RelationshipMutation:
		return FLinearColor(0.27f, 0.21f, 0.16f, 1.0f);
	case EDialogueEditorNodeType::FactionMutation:
		return FLinearColor(0.26f, 0.24f, 0.16f, 1.0f);
	case EDialogueEditorNodeType::Signal:
		return FLinearColor(0.24f, 0.16f, 0.27f, 1.0f);
	case EDialogueEditorNodeType::Route:
		return FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);
	case EDialogueEditorNodeType::RouteByCharacter:
		return FLinearColor(0.17f, 0.20f, 0.30f, 1.0f);
	case EDialogueEditorNodeType::CheckTags:
	case EDialogueEditorNodeType::CheckRelationship:
	case EDialogueEditorNodeType::CheckProgress:
	case EDialogueEditorNodeType::CheckStats:
	case EDialogueEditorNodeType::CheckLoadout:
	case EDialogueEditorNodeType::CheckCharacter:
	case EDialogueEditorNodeType::CheckVariable:
		return FLinearColor(0.16f, 0.26f, 0.30f, 1.0f);
	default:
		return FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);
	}
}

void UParleyDialogueEdGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);
	if (!Menu || !Context)
	{
		return;
	}

	UParleyDialogueEdGraphNode* MutableNode = const_cast<UParleyDialogueEdGraphNode*>(this);
	FToolMenuSection& NodeSection = Menu->AddSection(
		TEXT("ARDialogueNodeActions"),
		LOCTEXT("ARDialogueNodeActionsSection", "Dialogue Node"));
	NodeSection.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueRefreshNode"),
		LOCTEXT("ARDialogueRefreshNodeLabel", "Refresh Node"),
		LOCTEXT("ARDialogueRefreshNodeTooltip", "Rebuild this node's pins and refresh its visual state."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateWeakLambda(
			MutableNode,
			[MutableNode]()
			{
				if (!IsValid(MutableNode))
				{
					return;
				}

				MutableNode->EnsureStableIds(false, false);
				MutableNode->ReconstructNode();
				if (UEdGraph* Graph = MutableNode->GetGraph())
				{
					Graph->NotifyNodeChanged(MutableNode);
					Graph->NotifyGraphChanged();
				}
			}))));

	FToolMenuSection& EditSection = Menu->AddSection(
		TEXT("ARDialogueClipboardActions"),
		LOCTEXT("ARDialogueClipboardActionsSection", "Edit"));
	EditSection.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueCopyNode"),
		LOCTEXT("ARDialogueCopyNodeLabel", "Copy"),
		LOCTEXT("ARDialogueCopyNodeTooltip", "Copy this node to clipboard."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					if (!IsValid(MutableNode) || !MutableNode->CanDuplicateNode() || MutableNode->EditorNodeType == EDialogueEditorNodeType::Enter)
					{
						return;
					}

					TSet<UObject*> NodesToCopy;
					MutableNode->PrepareForCopying();
					NodesToCopy.Add(MutableNode);

					FString ExportedText;
					FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
					FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
				}),
			FCanExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					return IsValid(MutableNode) && MutableNode->CanDuplicateNode() && MutableNode->EditorNodeType != EDialogueEditorNodeType::Enter;
				}))));
	EditSection.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueCutNode"),
		LOCTEXT("ARDialogueCutNodeLabel", "Cut"),
		LOCTEXT("ARDialogueCutNodeTooltip", "Copy this node and delete it."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					if (!IsValid(MutableNode) || MutableNode->EditorNodeType == EDialogueEditorNodeType::Enter || !MutableNode->CanUserDeleteNode())
					{
						return;
					}

					TSet<UObject*> NodesToCopy;
					MutableNode->PrepareForCopying();
					NodesToCopy.Add(MutableNode);

					FString ExportedText;
					FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
					FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

					if (UEdGraph* Graph = MutableNode->GetGraph())
					{
						const FScopedTransaction Transaction(LOCTEXT("CutDialogueNode", "Cut Dialogue Node"));
						Graph->Modify();
						MutableNode->Modify();
						MutableNode->DestroyNode();
						Graph->NotifyGraphChanged();
						if (UObject* GraphOuter = Graph->GetOuter())
						{
							GraphOuter->MarkPackageDirty();
						}
					}
				}),
			FCanExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					return IsValid(MutableNode)
						&& MutableNode->EditorNodeType != EDialogueEditorNodeType::Enter
						&& MutableNode->CanDuplicateNode()
						&& MutableNode->CanUserDeleteNode();
				}))));
	EditSection.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueDuplicateNode"),
		LOCTEXT("ARDialogueDuplicateNodeLabel", "Duplicate"),
		LOCTEXT("ARDialogueDuplicateNodeTooltip", "Duplicate this node."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					if (!IsValid(MutableNode) || !MutableNode->CanDuplicateNode() || MutableNode->EditorNodeType == EDialogueEditorNodeType::Enter)
					{
						return;
					}

					UEdGraph* Graph = MutableNode->GetGraph();
					if (!Graph)
					{
						return;
					}

					TSet<UObject*> NodesToCopy;
					MutableNode->PrepareForCopying();
					NodesToCopy.Add(MutableNode);

					FString ExportedText;
					FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);

					const FScopedTransaction Transaction(LOCTEXT("DuplicateDialogueNode", "Duplicate Dialogue Node"));
					Graph->Modify();

					TSet<UEdGraphNode*> PastedNodes;
					FEdGraphUtilities::ImportNodesFromText(Graph, ExportedText, PastedNodes);

					for (UEdGraphNode* PastedNode : PastedNodes)
					{
						if (!PastedNode)
						{
							continue;
						}

						if (UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(PastedNode))
						{
							if (DialogueNode->EditorNodeType == EDialogueEditorNodeType::Enter)
							{
								DialogueNode->Modify();
								DialogueNode->DestroyNode();
								continue;
							}
							DialogueNode->EnsureStableIds(true, true);
							DialogueNode->ReconstructNode();
						}

						PastedNode->Modify();
						PastedNode->NodePosX += 80;
						PastedNode->NodePosY += 40;
						PastedNode->SnapToGrid(16);
					}

					Graph->NotifyGraphChanged();
					if (UObject* GraphOuter = Graph->GetOuter())
					{
						GraphOuter->MarkPackageDirty();
					}
				}),
			FCanExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					return IsValid(MutableNode) && MutableNode->CanDuplicateNode() && MutableNode->EditorNodeType != EDialogueEditorNodeType::Enter;
				}))));
	EditSection.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueDeleteNode"),
		LOCTEXT("ARDialogueDeleteNodeLabel", "Delete"),
		LOCTEXT("ARDialogueDeleteNodeTooltip", "Delete this node."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					if (!IsValid(MutableNode) || MutableNode->EditorNodeType == EDialogueEditorNodeType::Enter || !MutableNode->CanUserDeleteNode())
					{
						return;
					}

					if (UEdGraph* Graph = MutableNode->GetGraph())
					{
						const FScopedTransaction Transaction(LOCTEXT("DeleteDialogueNode", "Delete Dialogue Node"));
						Graph->Modify();
						MutableNode->Modify();
						MutableNode->DestroyNode();
						Graph->NotifyGraphChanged();
						if (UObject* GraphOuter = Graph->GetOuter())
						{
							GraphOuter->MarkPackageDirty();
						}
					}
				}),
			FCanExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					return IsValid(MutableNode) && MutableNode->EditorNodeType != EDialogueEditorNodeType::Enter && MutableNode->CanUserDeleteNode();
				}))));

	if (EditorNodeType == EDialogueEditorNodeType::MultiLine || EditorNodeType == EDialogueEditorNodeType::SplitLine)
	{
		FToolMenuSection& MultiLineSection = Menu->AddSection(
			TEXT("ARDialogueMultiLine"),
			LOCTEXT("ARDialogueMultiLineSection", "Multi-Line"));

		MultiLineSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("ARDialogueAddMultiLineEntry"),
			LOCTEXT("ARDialogueAddMultiLineEntryLabel", "Add Line"),
			LOCTEXT("ARDialogueAddMultiLineEntryTooltip", "Add another line entry to this multi-line node."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					if (IsValid(MutableNode))
					{
						MutableNode->AddMultiLineEntry();
					}
				}))));

		MultiLineSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("ARDialogueRemoveLastMultiLineEntry"),
			LOCTEXT("ARDialogueRemoveLastMultiLineEntryLabel", "Remove Line"),
			LOCTEXT("ARDialogueRemoveLastMultiLineEntryTooltip", "Remove the last line entry from this multi-line node."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode]()
				{
					if (!IsValid(MutableNode))
					{
						return;
					}

					const FDialogueMultiLineNodeData* Data = MutableNode->RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
					if (!Data || Data->Lines.Num() <= 1)
					{
						return;
					}

					MutableNode->RemoveMultiLineEntry(Data->Lines.Last().EntryId);
				}))));

		const FDialogueMultiLineNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
		if (Data)
		{
			for (int32 Index = 0; Index < Data->Lines.Num(); ++Index)
			{
				const FDialogueMultiLineEntry& Entry = Data->Lines[Index];
				MultiLineSection.AddEntry(FToolMenuEntry::InitMenuEntry(
					*FString::Printf(TEXT("ARDialogueDeleteMultiLineEntry_%d"), Index),
					FText::FromString(FString::Printf(TEXT("Delete Line %d"), Index + 1)),
					LOCTEXT("ARDialogueDeleteMultiLineEntryTooltip", "Delete this line entry."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MutableNode,
						[MutableNode, EntryId = Entry.EntryId]()
						{
							if (IsValid(MutableNode))
							{
								MutableNode->RemoveMultiLineEntry(EntryId);
							}
						}))));
			}
		}
	}

	if (!SupportsDynamicBranchPins())
	{
		return;
	}

	FToolMenuSection& Section = Menu->AddSection(
		TEXT("ARDialogueDynamicPins"),
		LOCTEXT("ARDialogueDynamicPinsSection", "Dialogue Pins"));

	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueAddPin"),
		LOCTEXT("ARDialogueAddPinLabel", "Add Pin"),
		LOCTEXT("ARDialogueAddPinTooltip", "Add another output branch pin to this node."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateUObject(MutableNode, &UParleyDialogueEdGraphNode::AddDynamicBranchPin))));

	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueRemoveLastPin"),
		LOCTEXT("ARDialogueRemoveLastPinLabel", "Remove Pin"),
		LOCTEXT("ARDialogueRemoveLastPinTooltip", "Remove the bottom-most branch pin from this node."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateWeakLambda(
			MutableNode,
			[MutableNode]()
			{
				if (IsValid(MutableNode))
				{
					MutableNode->RemoveLastDynamicBranchPin();
				}
			}))));

	if (!Context->Pin)
	{
		if (EditorNodeType == EDialogueEditorNodeType::Choice)
		{
			for (int32 Index = 0; Index < RuntimeNode.ChoiceBranches.Num(); ++Index)
			{
				const FDialogueCompiledChoiceBranch& Branch = RuntimeNode.ChoiceBranches[Index];
				const FName PinName = MakeChoicePinName(Branch.ChoiceBranchId);
				const FText Label = Branch.ChoiceText.IsEmpty()
					? FText::FromString(FString::Printf(TEXT("Delete Choice %d"), Index + 1))
					: FText::FromString(FString::Printf(TEXT("Delete Choice: %s"), *Branch.ChoiceText.ToString()));
				Section.AddEntry(FToolMenuEntry::InitMenuEntry(
					*FString::Printf(TEXT("ARDialogueDeleteChoice_%d"), Index),
					Label,
					LOCTEXT("ARDialogueDeleteChoiceTooltip", "Delete this choice branch pin."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MutableNode,
						[MutableNode, PinName]()
						{
							if (IsValid(MutableNode))
							{
								MutableNode->RemoveDynamicBranchPinByName(PinName);
							}
						}))));
			}
		}
		else if (EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
		{
			for (int32 Index = 0; Index < RuntimeNode.SwitchBranches.Num(); ++Index)
			{
				const FDialogueCompiledSwitchBranch& Branch = RuntimeNode.SwitchBranches[Index];
				const FName PinName = MakeSwitchPinName(Branch.BranchId);
				const FText Label = Branch.Label.IsEmpty()
					? FText::FromString(FString::Printf(TEXT("Delete Branch %d"), Index + 1))
					: FText::FromString(FString::Printf(TEXT("Delete Branch: %s"), *Branch.Label.ToString()));
				Section.AddEntry(FToolMenuEntry::InitMenuEntry(
					*FString::Printf(TEXT("ARDialogueDeleteSwitch_%d"), Index),
					Label,
					LOCTEXT("ARDialogueDeleteSwitchTooltip", "Delete this switch branch pin."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MutableNode,
						[MutableNode, PinName]()
						{
							if (IsValid(MutableNode))
							{
								MutableNode->RemoveDynamicBranchPinByName(PinName);
							}
						}))));
			}
		}
		else if (EditorNodeType == EDialogueEditorNodeType::Random)
		{
			for (int32 Index = 0; Index < RuntimeNode.RandomBranches.Num(); ++Index)
			{
				const FDialogueCompiledRandomBranch& Branch = RuntimeNode.RandomBranches[Index];
				const FName PinName = MakeRandomPinName(Branch.BranchId);
				const FText Label = FText::FromString(FString::Printf(TEXT("Delete Random Branch %d"), Index + 1));
				Section.AddEntry(FToolMenuEntry::InitMenuEntry(
					*FString::Printf(TEXT("ARDialogueDeleteRandom_%d"), Index),
					Label,
					LOCTEXT("ARDialogueDeleteRandomTooltip", "Delete this random branch pin."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MutableNode,
						[MutableNode, PinName]()
						{
							if (IsValid(MutableNode))
							{
								MutableNode->RemoveDynamicBranchPinByName(PinName);
							}
						}))));
			}
		}
		else if (EditorNodeType == EDialogueEditorNodeType::Sequence)
		{
			for (int32 Index = 0; Index < RuntimeNode.SequenceBranches.Num(); ++Index)
			{
				const FDialogueCompiledSequenceBranch& Branch = RuntimeNode.SequenceBranches[Index];
				const FName PinName = MakeSequencePinName(Branch.BranchId);
				const FText Label = FText::FromString(FString::Printf(TEXT("Delete Then %d"), Index + 1));
				Section.AddEntry(FToolMenuEntry::InitMenuEntry(
					*FString::Printf(TEXT("ARDialogueDeleteSequence_%d"), Index),
					Label,
					LOCTEXT("ARDialogueDeleteSequenceTooltip", "Delete this sequence branch pin."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MutableNode,
						[MutableNode, PinName]()
						{
							if (IsValid(MutableNode))
							{
								MutableNode->RemoveDynamicBranchPinByName(PinName);
							}
					}))));
			}
		}
		else if (EditorNodeType == EDialogueEditorNodeType::RouteByCharacter)
		{
			for (int32 Index = 0; Index < RuntimeNode.CharacterRouteBranches.Num(); ++Index)
			{
				const FDialogueCompiledCharacterRouteBranch& Branch = RuntimeNode.CharacterRouteBranches[Index];
				const FName PinName = MakeCharacterRoutePinName(Branch.BranchId);
				const FText Label = Branch.SpeakerTag.IsValid()
					? FText::FromString(FString::Printf(TEXT("Delete %s"), *Branch.SpeakerTag.ToString()))
					: FText::FromString(FString::Printf(TEXT("Delete Character Branch %d"), Index + 1));
				Section.AddEntry(FToolMenuEntry::InitMenuEntry(
					*FString::Printf(TEXT("ARDialogueDeleteCharacterRoute_%d"), Index),
					Label,
					LOCTEXT("ARDialogueDeleteCharacterRouteTooltip", "Delete this character route branch pin."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MutableNode,
						[MutableNode, PinName]()
						{
							if (IsValid(MutableNode))
							{
								MutableNode->RemoveDynamicBranchPinByName(PinName);
							}
						}))));
			}
		}

		return;
	}

	if (EditorNodeType == EDialogueEditorNodeType::Branch && Context->Pin->Direction == EGPD_Input)
	{
		FGuid ParsedInputId;
		if (!TryParseBranchGuidFromPinName(Context->Pin->PinName, ConditionPinPrefix, ParsedInputId))
		{
			return;
		}

		const FName PinName = Context->Pin->PinName;
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("ARDialogueDeleteConditionPin"),
			LOCTEXT("ARDialogueDeleteConditionPinLabel", "Delete Condition Pin"),
			LOCTEXT("ARDialogueDeleteConditionPinTooltip", "Delete this condition input pin and clear its connection."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode, PinName]()
				{
					if (IsValid(MutableNode))
					{
						MutableNode->RemoveDynamicBranchPinByName(PinName);
					}
				}))));
		return;
	}

	if (Context->Pin->Direction != EGPD_Output)
	{
		return;
	}

	FGuid ParsedBranchId;
	bool bCanDeletePin = false;
	if (EditorNodeType == EDialogueEditorNodeType::Choice)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, ChoicePinPrefix, ParsedBranchId);
	}
	else if (EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, SwitchPinPrefix, ParsedBranchId);
	}
	else if (EditorNodeType == EDialogueEditorNodeType::Random)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, RandomPinPrefix, ParsedBranchId);
	}
	else if (EditorNodeType == EDialogueEditorNodeType::Sequence)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, SequencePinPrefix, ParsedBranchId);
	}
	else if (EditorNodeType == EDialogueEditorNodeType::RouteByCharacter)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, CharacterRoutePinPrefix, ParsedBranchId);
	}

	if (!bCanDeletePin)
	{
		return;
	}

	int32 BranchIndex = INDEX_NONE;
	int32 BranchCount = 0;
	if (EditorNodeType == EDialogueEditorNodeType::Choice)
	{
		BranchIndex = RuntimeNode.ChoiceBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledChoiceBranch& Branch)
		{
			return Branch.ChoiceBranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.ChoiceBranches.Num();
	}
	else if (EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
	{
		BranchIndex = RuntimeNode.SwitchBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledSwitchBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.SwitchBranches.Num();
	}
	else if (EditorNodeType == EDialogueEditorNodeType::Random)
	{
		BranchIndex = RuntimeNode.RandomBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledRandomBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.RandomBranches.Num();
	}
	else if (EditorNodeType == EDialogueEditorNodeType::Sequence)
	{
		BranchIndex = RuntimeNode.SequenceBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledSequenceBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.SequenceBranches.Num();
	}
	else if (EditorNodeType == EDialogueEditorNodeType::RouteByCharacter)
	{
		BranchIndex = RuntimeNode.CharacterRouteBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.CharacterRouteBranches.Num();
	}

	if (EditorNodeType != EDialogueEditorNodeType::Sequence && BranchIndex > 0)
	{
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("ARDialogueMovePinUp"),
			LOCTEXT("ARDialogueMovePinUpLabel", "Move Pin Up"),
			LOCTEXT("ARDialogueMovePinUpTooltip", "Move this branch one slot up."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode, ParsedBranchId, BranchIndex]()
				{
					if (!IsValid(MutableNode))
					{
						return;
					}

					if (MutableNode->EditorNodeType == EDialogueEditorNodeType::Choice)
					{
						MutableNode->MoveChoiceBranch(ParsedBranchId, true);
					}
					else if (MutableNode->EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
					{
						MutableNode->MoveSwitchBranch(ParsedBranchId, true);
					}
					else if (MutableNode->EditorNodeType == EDialogueEditorNodeType::Random
						&& MutableNode->RuntimeNode.RandomBranches.IsValidIndex(BranchIndex - 1))
					{
						MutableNode->ReorderRandomBranch(ParsedBranchId, MutableNode->RuntimeNode.RandomBranches[BranchIndex - 1].BranchId);
					}
					else if (MutableNode->EditorNodeType == EDialogueEditorNodeType::RouteByCharacter)
					{
						MutableNode->MoveCharacterRouteBranch(ParsedBranchId, true);
					}
				}))));
	}

	if (EditorNodeType != EDialogueEditorNodeType::Sequence && BranchIndex != INDEX_NONE && BranchIndex + 1 < BranchCount)
	{
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("ARDialogueMovePinDown"),
			LOCTEXT("ARDialogueMovePinDownLabel", "Move Pin Down"),
			LOCTEXT("ARDialogueMovePinDownTooltip", "Move this branch one slot down."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateWeakLambda(
				MutableNode,
				[MutableNode, ParsedBranchId, BranchIndex]()
				{
					if (!IsValid(MutableNode))
					{
						return;
					}

					if (MutableNode->EditorNodeType == EDialogueEditorNodeType::Choice)
					{
						MutableNode->MoveChoiceBranch(ParsedBranchId, false);
					}
					else if (MutableNode->EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
					{
						MutableNode->MoveSwitchBranch(ParsedBranchId, false);
					}
					else if (MutableNode->EditorNodeType == EDialogueEditorNodeType::Random
						&& MutableNode->RuntimeNode.RandomBranches.IsValidIndex(BranchIndex + 1))
					{
						MutableNode->ReorderRandomBranch(ParsedBranchId, MutableNode->RuntimeNode.RandomBranches[BranchIndex + 1].BranchId);
					}
					else if (MutableNode->EditorNodeType == EDialogueEditorNodeType::RouteByCharacter)
					{
						MutableNode->MoveCharacterRouteBranch(ParsedBranchId, false);
					}
				}))));
	}

	const FName PinName = Context->Pin->PinName;
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("ARDialogueDeletePin"),
		LOCTEXT("ARDialogueDeletePinLabel", "Delete Pin"),
		LOCTEXT("ARDialogueDeletePinTooltip", "Delete this branch pin and clear any connection linked through it."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateWeakLambda(
			MutableNode,
			[MutableNode, PinName]()
			{
				if (IsValid(MutableNode))
				{
					MutableNode->RemoveDynamicBranchPinByName(PinName);
				}
			}))));
}

void UParleyDialogueEdGraphNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	SetFlags(RF_Transactional);
	CreateNewGuid();
	EnsureStableIds(false, false);
}

void UParleyDialogueEdGraphNode::PostPasteNode()
{
	Super::PostPasteNode();
	CreateNewGuid();
	EnsureStableIds(true, true);
	ReconstructNode();
}

void UParleyDialogueEdGraphNode::PrepareForCopying()
{
	Super::PrepareForCopying();
}

#if WITH_EDITOR
void UParleyDialogueEdGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
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

void UParleyDialogueEdGraphNode::ApplyValidation(const EDialogueValidationSeverity Severity, const FString& Message)
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

void UParleyDialogueEdGraphNode::ClearValidation()
{
	ValidationSeverity = EDialogueValidationSeverity::Info;
	ValidationMessage.Empty();
	ErrorMsg.Empty();
	ErrorType = static_cast<int32>(EMessageSeverity::Info);
}

bool UParleyDialogueEdGraphNode::CommitRuntimeNodeMutation(const FText& TransactionText, TFunctionRef<bool()> MutateFn, const bool bReconstructPins)
{
	const FScopedTransaction Transaction(TransactionText);
	Modify();

	if (!MutateFn())
	{
		return false;
	}

	EnsureStableIds(false, false);
	if (bReconstructPins)
	{
		ReconstructNode();
	}

	if (UEdGraph* Graph = GetGraph())
	{
		Graph->Modify();
		Graph->NotifyNodeChanged(this);
		Graph->NotifyGraphChanged();
		if (UObject* GraphOuter = Graph->GetOuter())
		{
			GraphOuter->MarkPackageDirty();
		}
	}

	MarkPackageDirty();
	return true;
}

bool UParleyDialogueEdGraphNode::TryParseBranchGuidFromPinName(const FName PinName, const FString& Prefix, FGuid& OutBranchId)
{
	const FString PinNameString = PinName.ToString();
	if (!PinNameString.StartsWith(Prefix))
	{
		return false;
	}

	const FString GuidDigits = PinNameString.RightChop(Prefix.Len());
	return FGuid::ParseExact(GuidDigits, EGuidFormats::Digits, OutBranchId);
}

void UParleyDialogueEdGraphNode::EnsureNodeDataMatchesNodeType()
{
	switch (EditorNodeType)
	{
	case EDialogueEditorNodeType::Line:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueLineNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueLineNodeData>();
		}
		break;
	case EDialogueEditorNodeType::MultiLine:
	case EDialogueEditorNodeType::SplitLine:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueMultiLineNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueMultiLineNodeData>();
		}
		break;
	case EDialogueEditorNodeType::Branch:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorBranchNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorBranchNodeData>();
		}
		break;
	case EDialogueEditorNodeType::TagMutation:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueTagMutationNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueTagMutationNodeData>();
		}
		break;
	case EDialogueEditorNodeType::RelationshipMutation:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueRelationshipMutationNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueRelationshipMutationNodeData>();
		}
		break;
	case EDialogueEditorNodeType::FactionMutation:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueFactionMutationNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueFactionMutationNodeData>();
		}
		break;
	case EDialogueEditorNodeType::Signal:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueSignalNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueSignalNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckTags:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckTagsNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckTagsNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckRelationship:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckRelationshipNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckRelationshipNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckProgress:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckProgressNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckProgressNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckStats:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckStatsNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckStatsNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckLoadout:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckLoadoutNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckLoadoutNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckCharacter:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckCharacterNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckCharacterNodeData>();
		}
		break;
	case EDialogueEditorNodeType::CheckVariable:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueEditorCheckVariableNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueEditorCheckVariableNodeData>();
		}
		break;
	default:
		RuntimeNode.NodeData.Reset();
		break;
	}
}

void UParleyDialogueEdGraphNode::EnsureBranchAndLineIds(const bool bRegenerateBranches, const bool bRegenerateLineGuid)
{
	if (EditorNodeType == EDialogueEditorNodeType::Line)
	{
		if (FDialogueLineNodeData* LineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueLineNodeData>())
		{
			if (bRegenerateLineGuid || !LineData->Line.LocalLineGuid.IsValid())
			{
				LineData->Line.LocalLineGuid = FGuid::NewGuid();
			}
			if (LineData->Line.LengthSeconds <= 0.0f)
			{
				LineData->Line.LengthSeconds = 1.0f;
			}
		}
	}

	if (EditorNodeType == EDialogueEditorNodeType::MultiLine || EditorNodeType == EDialogueEditorNodeType::SplitLine)
	{
		FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetMutablePtr<FDialogueMultiLineNodeData>();
		if (MultiLineData)
		{
			if (MultiLineData->Lines.IsEmpty())
			{
				FDialogueMultiLineEntry& DefaultEntry = MultiLineData->Lines.AddDefaulted_GetRef();
				DefaultEntry.EntryId = FGuid::NewGuid();
				DefaultEntry.LineData.SkipBlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
				DefaultEntry.LineData.Line.LocalLineGuid = FGuid::NewGuid();
			}

			TSet<FGuid> SeenEntryIds;
			for (FDialogueMultiLineEntry& Entry : MultiLineData->Lines)
			{
				if (bRegenerateBranches || !Entry.EntryId.IsValid() || SeenEntryIds.Contains(Entry.EntryId))
				{
					Entry.EntryId = FGuid::NewGuid();
				}
				SeenEntryIds.Add(Entry.EntryId);

				if (bRegenerateLineGuid || !Entry.LineData.Line.LocalLineGuid.IsValid())
				{
					Entry.LineData.Line.LocalLineGuid = FGuid::NewGuid();
				}
				if (Entry.LineData.Line.LengthSeconds <= 0.0f)
				{
					Entry.LineData.Line.LengthSeconds = 1.0f;
				}
			}
		}
	}

	if (EditorNodeType == EDialogueEditorNodeType::Branch)
	{
		FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorBranchNodeData>();
		if (BranchData)
		{
			if (BranchData->Inputs.IsEmpty())
			{
				FDialogueEditorConditionInput& DefaultInput = BranchData->Inputs.AddDefaulted_GetRef();
				DefaultInput.InputId = FGuid::NewGuid();
			}

			TSet<FGuid> SeenInputIds;
			for (FDialogueEditorConditionInput& Input : BranchData->Inputs)
			{
				if (bRegenerateBranches || !Input.InputId.IsValid() || SeenInputIds.Contains(Input.InputId))
				{
					Input.InputId = FGuid::NewGuid();
				}
				SeenInputIds.Add(Input.InputId);
			}
		}
	}

	if (EditorNodeType == EDialogueEditorNodeType::RouteByCharacter)
	{
		if (RuntimeNode.CharacterRouteBranches.IsEmpty())
		{
			FDialogueCompiledCharacterRouteBranch DefaultBranch;
			DefaultBranch.BranchId = FGuid::NewGuid();
			RuntimeNode.CharacterRouteBranches.Add(MoveTemp(DefaultBranch));
		}

		TSet<FGuid> SeenBranchIds;
		for (FDialogueCompiledCharacterRouteBranch& Branch : RuntimeNode.CharacterRouteBranches)
		{
			if (bRegenerateBranches || !Branch.BranchId.IsValid() || SeenBranchIds.Contains(Branch.BranchId))
			{
				Branch.BranchId = FGuid::NewGuid();
			}
			SeenBranchIds.Add(Branch.BranchId);
		}
	}

	if (EditorNodeType == EDialogueEditorNodeType::Choice)
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

	if (EditorNodeType == EDialogueEditorNodeType::SwitchOnTagsByPriority)
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

	if (EditorNodeType == EDialogueEditorNodeType::Random)
	{
		if (RuntimeNode.RandomBranches.IsEmpty())
		{
			FDialogueCompiledRandomBranch DefaultBranch;
			DefaultBranch.BranchId = FGuid::NewGuid();
			DefaultBranch.Weight = 1.0f;
			RuntimeNode.RandomBranches.Add(MoveTemp(DefaultBranch));
		}

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

	if (EditorNodeType == EDialogueEditorNodeType::Sequence)
	{
		if (RuntimeNode.SequenceBranches.IsEmpty())
		{
			FDialogueCompiledSequenceBranch DefaultBranch;
			DefaultBranch.BranchId = FGuid::NewGuid();
			RuntimeNode.SequenceBranches.Add(MoveTemp(DefaultBranch));
		}

		TSet<FGuid> SeenBranchIds;
		for (FDialogueCompiledSequenceBranch& Branch : RuntimeNode.SequenceBranches)
		{
			if (bRegenerateBranches || !Branch.BranchId.IsValid() || SeenBranchIds.Contains(Branch.BranchId))
			{
				Branch.BranchId = FGuid::NewGuid();
			}
			SeenBranchIds.Add(Branch.BranchId);
		}
	}
}

FString UParleyDialogueEdGraphNode::BuildInlineSummary() const
{
	switch (EditorNodeType)
	{
	case EDialogueEditorNodeType::Line:
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
	case EDialogueEditorNodeType::MultiLine:
	{
		const FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
		return MultiLineData
			? FString::Printf(TEXT("Lines:%d"), MultiLineData->Lines.Num())
			: TEXT("Invalid multiline payload");
	}
	case EDialogueEditorNodeType::SplitLine:
	{
		const FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
		return MultiLineData
			? FString::Printf(TEXT("Split Lines:%d"), MultiLineData->Lines.Num())
			: TEXT("Invalid split-line payload");
	}
	case EDialogueEditorNodeType::Choice:
		return FString::Printf(TEXT("Choices:%d Fallback:\"%s\" Important:%s"),
			RuntimeNode.ChoiceBranches.Num(),
			*RuntimeNode.FallbackChoiceText.ToString(),
			RuntimeNode.bChoiceNodeImportant ? TEXT("Y") : TEXT("N"));
	case EDialogueEditorNodeType::Branch:
	{
		const FDialogueEditorBranchNodeData* BranchData = RuntimeNode.NodeData.GetPtr<FDialogueEditorBranchNodeData>();
		if (!BranchData)
		{
			return TEXT("Invalid branch payload");
		}
		return FString::Printf(TEXT("%s Conditions:%d"),
			BranchData->MatchMode == EDialogueConditionMatchMode::All ? TEXT("AND") : TEXT("OR"),
			BranchData->Inputs.Num());
	}
	case EDialogueEditorNodeType::SwitchOnTagsByPriority:
		return FString::Printf(TEXT("Branches:%d Default:%s"),
			RuntimeNode.SwitchBranches.Num(),
			RuntimeNode.bSwitchHasDefaultOutput ? TEXT("Y") : TEXT("N"));
	case EDialogueEditorNodeType::TagMutation:
	{
		const FDialogueTagMutationNodeData* MutationData = RuntimeNode.NodeData.GetPtr<FDialogueTagMutationNodeData>();
		return MutationData ? FString::Printf(TEXT("Mutations:%d"), MutationData->Mutations.Num()) : TEXT("Invalid tag mutation payload");
	}
	case EDialogueEditorNodeType::RelationshipMutation:
	{
		const FDialogueRelationshipMutationNodeData* MutationData = RuntimeNode.NodeData.GetPtr<FDialogueRelationshipMutationNodeData>();
		return MutationData
			? FString::Printf(
				TEXT("Source:%s Target:%s Delta:%+.2f"),
				*MutationData->SourceSpeakerTag.ToString(),
				*MutationData->TargetSpeakerTag.ToString(),
				MutationData->DeltaPoints)
			: TEXT("Invalid relationship payload");
	}
	case EDialogueEditorNodeType::FactionMutation:
	{
		const FDialogueFactionMutationNodeData* MutationData = RuntimeNode.NodeData.GetPtr<FDialogueFactionMutationNodeData>();
		return MutationData
			? FString::Printf(TEXT("Faction:%s Pop:%+.2f Speaker:%s Rep:%+.2f"),
				*MutationData->FactionTag.ToString(),
				MutationData->DeltaPopularity,
				*MutationData->TargetSpeakerTag.ToString(),
				MutationData->DeltaSpeakerReputation)
			: TEXT("Invalid faction payload");
	}
	case EDialogueEditorNodeType::Signal:
	{
		const FDialogueSignalNodeData* SignalData = RuntimeNode.NodeData.GetPtr<FDialogueSignalNodeData>();
		if (!SignalData)
		{
			return TEXT("Invalid signal payload");
		}

		return SignalData->SignalTag.IsValid()
			? SignalData->SignalTag.ToString()
			: TEXT("No signal tag");
	}
	case EDialogueEditorNodeType::Random:
		return FString::Printf(TEXT("Branches:%d"), RuntimeNode.RandomBranches.Num());
	case EDialogueEditorNodeType::Sequence:
		return FString::Printf(TEXT("Then Branches:%d"), RuntimeNode.SequenceBranches.Num());
	case EDialogueEditorNodeType::Route:
		return TEXT("Wire organizer (no runtime side effects)");
	case EDialogueEditorNodeType::RouteByCharacter:
		return FString::Printf(TEXT("Character Branches:%d"), RuntimeNode.CharacterRouteBranches.Num());
	case EDialogueEditorNodeType::CheckTags:
	{
		const FDialogueEditorCheckTagsNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckTagsNodeData>();
		return Data
			? FString::Printf(TEXT("Source:%d Tag:%s"), static_cast<int32>(Data->Source), *Data->TagValue.ToString())
			: TEXT("Invalid tags payload");
	}
	case EDialogueEditorNodeType::CheckRelationship:
	{
		const FDialogueEditorCheckRelationshipNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckRelationshipNodeData>();
		return Data
			? FString::Printf(TEXT("Source:%d Speaker:%s Faction:%s Value:%.2f"),
				static_cast<int32>(Data->Source),
				*Data->TargetSpeakerTag.ToString(),
				*Data->FactionTag.ToString(),
				Data->NumericValue)
			: TEXT("Invalid relationship check payload");
	}
	case EDialogueEditorNodeType::CheckProgress:
	{
		const FDialogueEditorCheckProgressNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckProgressNodeData>();
		return Data
			? FString::Printf(TEXT("Source:%d Expected:%s"), static_cast<int32>(Data->Source), Data->bExpectedValue ? TEXT("True") : TEXT("False"))
			: TEXT("Invalid progress payload");
	}
	case EDialogueEditorNodeType::CheckStats:
	{
		const FDialogueEditorCheckStatsNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckStatsNodeData>();
		return Data
			? FString::Printf(TEXT("Source:%d Value:%.2f"), static_cast<int32>(Data->Source), Data->NumericValue)
			: TEXT("Invalid stats payload");
	}
	case EDialogueEditorNodeType::CheckLoadout:
	{
		const FDialogueEditorCheckLoadoutNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckLoadoutNodeData>();
		return Data ? Data->TagValue.ToString() : TEXT("Invalid loadout payload");
	}
	case EDialogueEditorNodeType::CheckCharacter:
	{
		const FDialogueEditorCheckCharacterNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckCharacterNodeData>();
		return Data ? Data->CharacterTag.ToString() : TEXT("Invalid character payload");
	}
	case EDialogueEditorNodeType::CheckVariable:
	{
		const FDialogueEditorCheckVariableNodeData* Data = RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckVariableNodeData>();
		return Data ? Data->VariableName.ToString() : TEXT("Invalid variable payload");
	}
	default:
		return FString();
	}
}

#undef LOCTEXT_NAMESPACE
