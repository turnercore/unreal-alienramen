#include "ARDialogueConversationAsset.h"

bool UARDialogueConversationAsset::IsCompiledGraphValid() const
{
	return CompiledData.EnterNodeId.IsValid() && CompiledData.Nodes.Num() > 0 && !LastCompileValidation.HasErrors();
}

void UARDialogueConversationAsset::ClearCompiledData()
{
	CompiledData = FDialogueCompiledConversationData();
	LastCompileValidation = FDialogueValidationReport();
	bLastCompileSucceeded = false;
	CompileVersion = 0;
}

const FDialogueCompiledNode* UARDialogueConversationAsset::FindCompiledNode(const FGuid& NodeId) const
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

FDialogueCompiledNode* UARDialogueConversationAsset::FindCompiledNodeMutable(const FGuid& NodeId)
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
