#include "ParleyDialogueConditionCompileUtils.h"

#include "ParleyDialogueEdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "GameplayTagsManager.h"

namespace ParleyDialogueConditionCompile
{
	bool IsConditionSourceNodeType(const EDialogueEditorNodeType NodeType)
	{
		return NodeType == EDialogueEditorNodeType::CheckTags
			|| NodeType == EDialogueEditorNodeType::CheckRelationship
			|| NodeType == EDialogueEditorNodeType::CheckProgress
			|| NodeType == EDialogueEditorNodeType::CheckLoadout
			|| NodeType == EDialogueEditorNodeType::CheckCharacter
			|| NodeType == EDialogueEditorNodeType::CheckVariable;
	}

	bool BuildConditionFromSourceNode(
		const UParleyDialogueEdGraphNode* SourceNode,
		FDialogueCondition& OutCondition,
		FString& OutError)
	{
		if (!SourceNode)
		{
			OutError = TEXT("Condition source node is null.");
			return false;
		}

		OutCondition = FDialogueCondition();
		OutError.Reset();

		switch (SourceNode->EditorNodeType)
		{
		case EDialogueEditorNodeType::CheckTags:
		{
			const FDialogueEditorCheckTagsNodeData* Data = SourceNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckTagsNodeData>();
			if (!Data)
			{
				OutError = TEXT("CheckTags payload missing.");
				return false;
			}

			switch (Data->Source)
			{
			case EDialogueEditorTagConditionSource::CombinedTags:
				OutCondition.Source = EDialogueConditionSource::CombinedTags;
				break;
			case EDialogueEditorTagConditionSource::PlayerTags:
				OutCondition.Source = EDialogueConditionSource::PlayerTags;
				break;
			case EDialogueEditorTagConditionSource::GameTags:
				OutCondition.Source = EDialogueConditionSource::GameTags;
				break;
			case EDialogueEditorTagConditionSource::TransientConversationTags:
				OutCondition.Source = EDialogueConditionSource::TransientConversationTags;
				break;
			}
			OutCondition.Operator = Data->Operator;
			OutCondition.TagValue = Data->TagValue;
			return true;
		}
		case EDialogueEditorNodeType::CheckRelationship:
		{
			const FDialogueEditorCheckRelationshipNodeData* Data = SourceNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckRelationshipNodeData>();
			if (!Data)
			{
				OutError = TEXT("CheckRelationship payload missing.");
				return false;
			}

			switch (Data->Source)
			{
			case EDialogueEditorRelationshipConditionSource::RelationshipLevel:
				OutCondition.Source = EDialogueConditionSource::RelationshipLevel;
				OutCondition.TagValue = Data->TargetSpeakerTag;
				break;
			case EDialogueEditorRelationshipConditionSource::FactionPopularity:
				OutCondition.Source = EDialogueConditionSource::FactionPopularity;
				OutCondition.TagValue = Data->FactionTag;
				break;
			case EDialogueEditorRelationshipConditionSource::FactionSpeakerReputation:
				OutCondition.Source = EDialogueConditionSource::FactionSpeakerReputation;
				OutCondition.TagValue = Data->FactionTag;
				OutCondition.SecondaryTagValue = Data->TargetSpeakerTag;
				break;
			case EDialogueEditorRelationshipConditionSource::RelationshipPoints:
			default:
				OutCondition.Source = EDialogueConditionSource::RelationshipPoints;
				OutCondition.TagValue = Data->TargetSpeakerTag;
				break;
			}
			OutCondition.Operator = Data->Operator;
			OutCondition.NumericValue = Data->NumericValue;
			return true;
		}
		case EDialogueEditorNodeType::CheckProgress:
		{
			const FDialogueEditorCheckProgressNodeData* Data = SourceNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckProgressNodeData>();
			if (!Data)
			{
				OutError = TEXT("CheckProgress payload missing.");
				return false;
			}

			switch (Data->Source)
			{
			case EDialogueEditorProgressConditionSource::SeenByPlayer:
				OutCondition.Source = EDialogueConditionSource::SeenByPlayer;
				break;
			case EDialogueEditorProgressConditionSource::SeenByGame:
				OutCondition.Source = EDialogueConditionSource::SeenByGame;
				break;
			case EDialogueEditorProgressConditionSource::CompletedByPlayer:
				OutCondition.Source = EDialogueConditionSource::CompletedByPlayer;
				break;
			case EDialogueEditorProgressConditionSource::CompletedByGame:
				OutCondition.Source = EDialogueConditionSource::CompletedByGame;
				break;
			}
			OutCondition.Operator = Data->Operator;
			OutCondition.NumericValue = Data->bExpectedValue ? 1.0f : 0.0f;
			return true;
		}
		case EDialogueEditorNodeType::CheckLoadout:
		{
			const FDialogueEditorCheckLoadoutNodeData* Data = SourceNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckLoadoutNodeData>();
			if (!Data)
			{
				OutError = TEXT("CheckLoadout payload missing.");
				return false;
			}

			OutCondition.Source = EDialogueConditionSource::Loadout;
			OutCondition.Operator = Data->Operator;
			OutCondition.TagValue = Data->TagValue;
			return true;
		}
		case EDialogueEditorNodeType::CheckCharacter:
		{
			const FDialogueEditorCheckCharacterNodeData* Data = SourceNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckCharacterNodeData>();
			if (!Data)
			{
				OutError = TEXT("CheckCharacter payload missing.");
				return false;
			}

			OutCondition.Source = EDialogueConditionSource::ActiveCharacter;
			OutCondition.Operator = EDialogueComparisonOp::Present;
			OutCondition.TagValue = Data->CharacterTag;
			return true;
		}
		case EDialogueEditorNodeType::CheckVariable:
		{
			const FDialogueEditorCheckVariableNodeData* Data = SourceNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorCheckVariableNodeData>();
			if (!Data)
			{
				OutError = TEXT("CheckVariable payload missing.");
				return false;
			}

			OutCondition.Source = EDialogueConditionSource::InjectedVariable;
			OutCondition.Operator = Data->Operator;
			OutCondition.VariableName = Data->VariableName;
			OutCondition.InjectedValue = Data->InjectedValue;
			return true;
		}
		default:
			OutError = TEXT("Node is not a valid condition source.");
			return false;
		}
	}

	bool BuildConditionGroupFromBranchNode(
		const UParleyDialogueEdGraphNode* BranchNode,
		FDialogueConditionGroup& OutGroup,
		TArray<FString>& OutWarnings,
		TArray<FString>& OutErrors)
	{
		OutGroup = FDialogueConditionGroup();
		OutWarnings.Reset();
		OutErrors.Reset();

		if (!BranchNode || BranchNode->EditorNodeType != EDialogueEditorNodeType::Branch)
		{
			OutErrors.Add(TEXT("Node is not a Branch node."));
			return false;
		}

		const FDialogueEditorBranchNodeData* BranchData = BranchNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorBranchNodeData>();
		if (!BranchData)
		{
			OutErrors.Add(TEXT("Branch payload missing."));
			return false;
		}

		OutGroup.MatchMode = BranchData->MatchMode;
		bool bAnyConnected = false;

		for (int32 InputIndex = 0; InputIndex < BranchData->Inputs.Num(); ++InputIndex)
		{
			const FDialogueEditorConditionInput& Input = BranchData->Inputs[InputIndex];
			const UEdGraphPin* InputPin = BranchNode->GetConditionInputPin(Input.InputId);
			if (!InputPin || InputPin->LinkedTo.IsEmpty())
			{
				continue;
			}

			bAnyConnected = true;
			if (InputPin->LinkedTo.Num() > 1)
			{
				OutErrors.Add(FString::Printf(TEXT("Branch condition input %d has multiple links; only one link is allowed."), InputIndex + 1));
			}

			const UEdGraphPin* LinkedPin = InputPin->LinkedTo[0];
			const UParleyDialogueEdGraphNode* LinkedNode = LinkedPin ? Cast<UParleyDialogueEdGraphNode>(LinkedPin->GetOwningNode()) : nullptr;
			if (!LinkedNode || !IsConditionSourceNodeType(LinkedNode->EditorNodeType))
			{
				OutErrors.Add(FString::Printf(TEXT("Branch condition input %d is not linked to a valid Check* node."), InputIndex + 1));
				continue;
			}

			FDialogueCondition Condition;
			FString ConditionError;
			if (BuildConditionFromSourceNode(LinkedNode, Condition, ConditionError))
			{
				OutGroup.Conditions.Add(Condition);
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("Condition input %d failed to build: %s"), InputIndex + 1, *ConditionError));
			}
		}

		if (!bAnyConnected || OutGroup.Conditions.IsEmpty())
		{
			OutErrors.Add(TEXT("Branch node requires at least one connected condition input."));
		}

		return OutErrors.IsEmpty();
	}
}

