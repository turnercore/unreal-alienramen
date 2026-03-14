#pragma once

#include "CoreMinimal.h"
#include "ParleyDialogueTypes.h"

class UParleyDialogueEdGraphNode;
enum class EDialogueEditorNodeType : uint8;

namespace ParleyDialogueConditionCompile
{
	bool IsConditionSourceNodeType(EDialogueEditorNodeType NodeType);

	bool BuildConditionFromSourceNode(
		const UParleyDialogueEdGraphNode* SourceNode,
		FDialogueCondition& OutCondition,
		FString& OutError);

	bool BuildConditionGroupFromBranchNode(
		const UParleyDialogueEdGraphNode* BranchNode,
		FDialogueConditionGroup& OutGroup,
		TArray<FString>& OutWarnings,
		TArray<FString>& OutErrors);
}

