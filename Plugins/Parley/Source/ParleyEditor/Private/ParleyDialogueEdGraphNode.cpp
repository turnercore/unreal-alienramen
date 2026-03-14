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
	static const FName PinCategoryExec(TEXT("DialogueExec"));
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
		case EDialogueNodeType::Signal:
			return FText::FromString(TEXT("Signal"));
		case EDialogueNodeType::Random:
			return FText::FromString(TEXT("Random"));
		case EDialogueNodeType::Route:
			return FText::FromString(TEXT("Route"));
		case EDialogueNodeType::Sequence:
			return FText::FromString(TEXT("Sequence"));
		case EDialogueNodeType::MultiLine:
			return FText::FromString(TEXT("Multi-Line"));
		case EDialogueNodeType::SplitLine:
			return FText::FromString(TEXT("Split Line"));
		case EDialogueNodeType::RouteByCharacter:
			return FText::FromString(TEXT("Route Character"));
		default:
			return FText::FromString(TEXT("Unknown"));
		}
	}

	static FString BuildNodeTypeTooltip(const EDialogueNodeType NodeType)
	{
		switch (NodeType)
		{
		case EDialogueNodeType::Enter:
			return TEXT("Entry point for this conversation graph.");
		case EDialogueNodeType::Completed:
			return TEXT("Conversation completion node.");
		case EDialogueNodeType::Line:
			return TEXT("Single spoken line.");
		case EDialogueNodeType::Choice:
			return TEXT("Player choice branch node.");
		case EDialogueNodeType::Bool:
			return TEXT("Conditional true/false branch node.");
		case EDialogueNodeType::SwitchOnTagsByPriority:
			return TEXT("Priority switch branch node.");
		case EDialogueNodeType::TagMutation:
			return TEXT("Gameplay tag mutation node.");
		case EDialogueNodeType::RelationshipMutation:
			return TEXT("Relationship mutation node.");
		case EDialogueNodeType::FactionMutation:
			return TEXT("Faction mutation node.");
		case EDialogueNodeType::Signal:
			return TEXT("Fires a gameplay tag signal for game systems to react to.");
		case EDialogueNodeType::Random:
			return TEXT("Weighted random branch node.");
		case EDialogueNodeType::Route:
			return TEXT("Flow routing helper node.");
		case EDialogueNodeType::Sequence:
			return TEXT("Sequential branch node.");
		case EDialogueNodeType::MultiLine:
			return TEXT("Multi-line sequence node.");
		case EDialogueNodeType::SplitLine:
			return TEXT("Split-line variant node.");
		case EDialogueNodeType::RouteByCharacter:
			return TEXT("Route by active player character.");
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

void UParleyDialogueEdGraphNode::InitializeForNodeType(const EDialogueNodeType NodeType)
{
	RuntimeNode = FDialogueCompiledNode();
	RuntimeNode.NodeType = NodeType;
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
	return RuntimeNode.NodeType == EDialogueNodeType::Choice
		|| RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority
		|| RuntimeNode.NodeType == EDialogueNodeType::Random
		|| RuntimeNode.NodeType == EDialogueNodeType::Sequence
		|| RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter;
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
	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Choice:
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
	case EDialogueNodeType::SwitchOnTagsByPriority:
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
	case EDialogueNodeType::Random:
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
	case EDialogueNodeType::Sequence:
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
	case EDialogueNodeType::RouteByCharacter:
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

	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Choice:
		if (RuntimeNode.ChoiceBranches.IsEmpty())
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeChoicePinName(RuntimeNode.ChoiceBranches.Last().ChoiceBranchId));
	case EDialogueNodeType::SwitchOnTagsByPriority:
		if (RuntimeNode.SwitchBranches.IsEmpty())
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeSwitchPinName(RuntimeNode.SwitchBranches.Last().BranchId));
	case EDialogueNodeType::Random:
		if (RuntimeNode.RandomBranches.Num() <= 1)
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeRandomPinName(RuntimeNode.RandomBranches.Last().BranchId));
	case EDialogueNodeType::Sequence:
		if (RuntimeNode.SequenceBranches.Num() <= 1)
		{
			return false;
		}
		return RemoveDynamicBranchPinByName(MakeSequencePinName(RuntimeNode.SequenceBranches.Last().BranchId));
	case EDialogueNodeType::RouteByCharacter:
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
			if (RuntimeNode.NodeType == EDialogueNodeType::Choice
				&& TryParseBranchGuidFromPinName(PinName, ChoicePinPrefix, BranchId))
			{
				const int32 RemovedCount = RuntimeNode.ChoiceBranches.RemoveAll([BranchId](const FDialogueCompiledChoiceBranch& Branch)
				{
					return Branch.ChoiceBranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority
				&& TryParseBranchGuidFromPinName(PinName, SwitchPinPrefix, BranchId))
			{
				const int32 RemovedCount = RuntimeNode.SwitchBranches.RemoveAll([BranchId](const FDialogueCompiledSwitchBranch& Branch)
				{
					return Branch.BranchId == BranchId;
				});
				return RemovedCount > 0;
			}

			if (RuntimeNode.NodeType == EDialogueNodeType::Random
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

			if (RuntimeNode.NodeType == EDialogueNodeType::Sequence
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

			if (RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter
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
			if (RuntimeNode.NodeType != EDialogueNodeType::Choice || RuntimeNode.FallbackChoiceText.EqualTo(NewFallbackText))
			{
				return false;
			}

			RuntimeNode.FallbackChoiceText = NewFallbackText;
			return true;
		},
		true);
}

bool UParleyDialogueEdGraphNode::SetRelationshipTargetSpeakerTag(const FGameplayTag& NewTag)
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("SetRelationshipSpeakerTag", "Set Relationship Speaker Tag"),
		[this, NewTag]() -> bool
		{
			if (RuntimeNode.NodeType != EDialogueNodeType::RelationshipMutation)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::RelationshipMutation)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::FactionMutation)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::FactionMutation)
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

bool UParleyDialogueEdGraphNode::AddMultiLineEntry()
{
	return CommitRuntimeNodeMutation(
		LOCTEXT("AddMultiLineEntry", "Add Multi-Line Entry"),
		[this]() -> bool
		{
			if (RuntimeNode.NodeType != EDialogueNodeType::MultiLine
				&& RuntimeNode.NodeType != EDialogueNodeType::SplitLine)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::MultiLine
				&& RuntimeNode.NodeType != EDialogueNodeType::SplitLine)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::MultiLine
				&& RuntimeNode.NodeType != EDialogueNodeType::SplitLine)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::MultiLine
				&& RuntimeNode.NodeType != EDialogueNodeType::SplitLine)
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
			if (RuntimeNode.NodeType != EDialogueNodeType::MultiLine
				&& RuntimeNode.NodeType != EDialogueNodeType::SplitLine)
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

void UParleyDialogueEdGraphNode::AllocateDefaultPins()
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
	case EDialogueNodeType::MultiLine:
	case EDialogueNodeType::SplitLine:
	case EDialogueNodeType::TagMutation:
	case EDialogueNodeType::RelationshipMutation:
	case EDialogueNodeType::FactionMutation:
	case EDialogueNodeType::Signal:
	case EDialogueNodeType::Route:
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
	case EDialogueNodeType::Sequence:
		AddInputPinIfNeeded();
		AddSequencePins();
		break;
	case EDialogueNodeType::RouteByCharacter:
		AddInputPinIfNeeded();
		AddCharacterRoutePins();
		break;
	default:
		AddInputPinIfNeeded();
		AddNextOutputPinIfNeeded();
		break;
	}
}

bool UParleyDialogueEdGraphNode::CanUserDeleteNode() const
{
	return RuntimeNode.NodeType != EDialogueNodeType::Enter;
}

FText UParleyDialogueEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	(void)TitleType;
	return BuildNodeTypeText(RuntimeNode.NodeType);
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
		return FText::FromString(BuildNodeTypeTooltip(RuntimeNode.NodeType));
	}

	return FText::FromString(FString::Printf(TEXT("%s\n%s"), *BuildNodeTypeTooltip(RuntimeNode.NodeType), *InlineSummary));
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

	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Enter:
		return FLinearColor(0.16f, 0.22f, 0.30f, 1.0f);
	case EDialogueNodeType::Completed:
		return FLinearColor(0.16f, 0.27f, 0.18f, 1.0f);
	case EDialogueNodeType::Line:
		return FLinearColor(0.14f, 0.21f, 0.23f, 1.0f);
	case EDialogueNodeType::MultiLine:
		return FLinearColor(0.14f, 0.24f, 0.28f, 1.0f);
	case EDialogueNodeType::SplitLine:
		return FLinearColor(0.13f, 0.25f, 0.31f, 1.0f);
	case EDialogueNodeType::Choice:
		return FLinearColor(0.22f, 0.20f, 0.30f, 1.0f);
	case EDialogueNodeType::Bool:
		return FLinearColor(0.19f, 0.20f, 0.27f, 1.0f);
	case EDialogueNodeType::SwitchOnTagsByPriority:
		return FLinearColor(0.20f, 0.19f, 0.28f, 1.0f);
	case EDialogueNodeType::Random:
		return FLinearColor(0.21f, 0.19f, 0.29f, 1.0f);
	case EDialogueNodeType::Sequence:
		return FLinearColor(0.20f, 0.22f, 0.28f, 1.0f);
	case EDialogueNodeType::TagMutation:
		return FLinearColor(0.16f, 0.24f, 0.17f, 1.0f);
	case EDialogueNodeType::RelationshipMutation:
		return FLinearColor(0.27f, 0.21f, 0.16f, 1.0f);
	case EDialogueNodeType::FactionMutation:
		return FLinearColor(0.26f, 0.24f, 0.16f, 1.0f);
	case EDialogueNodeType::Signal:
		return FLinearColor(0.24f, 0.16f, 0.27f, 1.0f);
	case EDialogueNodeType::Route:
		return FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);
	case EDialogueNodeType::RouteByCharacter:
		return FLinearColor(0.17f, 0.20f, 0.30f, 1.0f);
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
					if (!IsValid(MutableNode) || !MutableNode->CanDuplicateNode() || MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
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
					return IsValid(MutableNode) && MutableNode->CanDuplicateNode() && MutableNode->RuntimeNode.NodeType != EDialogueNodeType::Enter;
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
					if (!IsValid(MutableNode) || MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Enter || !MutableNode->CanUserDeleteNode())
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
						&& MutableNode->RuntimeNode.NodeType != EDialogueNodeType::Enter
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
					if (!IsValid(MutableNode) || !MutableNode->CanDuplicateNode() || MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
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
							if (DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
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
					return IsValid(MutableNode) && MutableNode->CanDuplicateNode() && MutableNode->RuntimeNode.NodeType != EDialogueNodeType::Enter;
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
					if (!IsValid(MutableNode) || MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Enter || !MutableNode->CanUserDeleteNode())
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
					return IsValid(MutableNode) && MutableNode->RuntimeNode.NodeType != EDialogueNodeType::Enter && MutableNode->CanUserDeleteNode();
				}))));

	if (RuntimeNode.NodeType == EDialogueNodeType::MultiLine || RuntimeNode.NodeType == EDialogueNodeType::SplitLine)
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
		if (RuntimeNode.NodeType == EDialogueNodeType::Choice)
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
		else if (RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority)
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
		else if (RuntimeNode.NodeType == EDialogueNodeType::Random)
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
		else if (RuntimeNode.NodeType == EDialogueNodeType::Sequence)
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
		else if (RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter)
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

	if (!Context->Pin || Context->Pin->Direction != EGPD_Output)
	{
		return;
	}

	FGuid ParsedBranchId;
	bool bCanDeletePin = false;
	if (RuntimeNode.NodeType == EDialogueNodeType::Choice)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, ChoicePinPrefix, ParsedBranchId);
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, SwitchPinPrefix, ParsedBranchId);
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::Random)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, RandomPinPrefix, ParsedBranchId);
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::Sequence)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, SequencePinPrefix, ParsedBranchId);
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter)
	{
		bCanDeletePin = TryParseBranchGuidFromPinName(Context->Pin->PinName, CharacterRoutePinPrefix, ParsedBranchId);
	}

	if (!bCanDeletePin)
	{
		return;
	}

	int32 BranchIndex = INDEX_NONE;
	int32 BranchCount = 0;
	if (RuntimeNode.NodeType == EDialogueNodeType::Choice)
	{
		BranchIndex = RuntimeNode.ChoiceBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledChoiceBranch& Branch)
		{
			return Branch.ChoiceBranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.ChoiceBranches.Num();
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority)
	{
		BranchIndex = RuntimeNode.SwitchBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledSwitchBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.SwitchBranches.Num();
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::Random)
	{
		BranchIndex = RuntimeNode.RandomBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledRandomBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.RandomBranches.Num();
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::Sequence)
	{
		BranchIndex = RuntimeNode.SequenceBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledSequenceBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.SequenceBranches.Num();
	}
	else if (RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter)
	{
		BranchIndex = RuntimeNode.CharacterRouteBranches.IndexOfByPredicate([ParsedBranchId](const FDialogueCompiledCharacterRouteBranch& Branch)
		{
			return Branch.BranchId == ParsedBranchId;
		});
		BranchCount = RuntimeNode.CharacterRouteBranches.Num();
	}

	if (RuntimeNode.NodeType != EDialogueNodeType::Sequence && BranchIndex > 0)
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

					if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Choice)
					{
						MutableNode->MoveChoiceBranch(ParsedBranchId, true);
					}
					else if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority)
					{
						MutableNode->MoveSwitchBranch(ParsedBranchId, true);
					}
					else if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Random
						&& MutableNode->RuntimeNode.RandomBranches.IsValidIndex(BranchIndex - 1))
					{
						MutableNode->ReorderRandomBranch(ParsedBranchId, MutableNode->RuntimeNode.RandomBranches[BranchIndex - 1].BranchId);
					}
					else if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter)
					{
						MutableNode->MoveCharacterRouteBranch(ParsedBranchId, true);
					}
				}))));
	}

	if (RuntimeNode.NodeType != EDialogueNodeType::Sequence && BranchIndex != INDEX_NONE && BranchIndex + 1 < BranchCount)
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

					if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Choice)
					{
						MutableNode->MoveChoiceBranch(ParsedBranchId, false);
					}
					else if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::SwitchOnTagsByPriority)
					{
						MutableNode->MoveSwitchBranch(ParsedBranchId, false);
					}
					else if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::Random
						&& MutableNode->RuntimeNode.RandomBranches.IsValidIndex(BranchIndex + 1))
					{
						MutableNode->ReorderRandomBranch(ParsedBranchId, MutableNode->RuntimeNode.RandomBranches[BranchIndex + 1].BranchId);
					}
					else if (MutableNode->RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter)
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
	switch (RuntimeNode.NodeType)
	{
	case EDialogueNodeType::Line:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueLineNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueLineNodeData>();
		}
		break;
	case EDialogueNodeType::MultiLine:
	case EDialogueNodeType::SplitLine:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueMultiLineNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueMultiLineNodeData>();
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
	case EDialogueNodeType::Signal:
		if (RuntimeNode.NodeData.GetScriptStruct() != FDialogueSignalNodeData::StaticStruct())
		{
			RuntimeNode.NodeData.InitializeAs<FDialogueSignalNodeData>();
		}
		break;
	default:
		RuntimeNode.NodeData.Reset();
		break;
	}
}

void UParleyDialogueEdGraphNode::EnsureBranchAndLineIds(const bool bRegenerateBranches, const bool bRegenerateLineGuid)
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

	if (RuntimeNode.NodeType == EDialogueNodeType::MultiLine || RuntimeNode.NodeType == EDialogueNodeType::SplitLine)
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
			}
		}
	}

	if (RuntimeNode.NodeType == EDialogueNodeType::RouteByCharacter)
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

	if (RuntimeNode.NodeType == EDialogueNodeType::Sequence)
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
	case EDialogueNodeType::MultiLine:
	{
		const FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
		return MultiLineData
			? FString::Printf(TEXT("Lines:%d"), MultiLineData->Lines.Num())
			: TEXT("Invalid multiline payload");
	}
	case EDialogueNodeType::SplitLine:
	{
		const FDialogueMultiLineNodeData* MultiLineData = RuntimeNode.NodeData.GetPtr<FDialogueMultiLineNodeData>();
		return MultiLineData
			? FString::Printf(TEXT("Split Lines:%d"), MultiLineData->Lines.Num())
			: TEXT("Invalid split-line payload");
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
	case EDialogueNodeType::Signal:
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
	case EDialogueNodeType::Random:
		return FString::Printf(TEXT("Branches:%d"), RuntimeNode.RandomBranches.Num());
	case EDialogueNodeType::Sequence:
		return FString::Printf(TEXT("Then Branches:%d"), RuntimeNode.SequenceBranches.Num());
	case EDialogueNodeType::Route:
		return TEXT("Wire organizer (no runtime side effects)");
	case EDialogueNodeType::RouteByCharacter:
		return FString::Printf(TEXT("Character Branches:%d"), RuntimeNode.CharacterRouteBranches.Num());
	default:
		return FString();
	}
}

#undef LOCTEXT_NAMESPACE
