#include "ParleyConversationAsset.h"

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
