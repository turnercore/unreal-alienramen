#include "ARDialogueEdGraphSchema.h"

#include "ARDialogueEdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"

namespace
{
	static const FName PinCategoryExec(TEXT("DialogueExec"));

	static bool GraphHasEnterNode(const UEdGraph* Graph)
	{
		if (!Graph)
		{
			return false;
		}

		for (const UEdGraphNode* GraphNode : Graph->Nodes)
		{
			const UARDialogueEdGraphNode* DialogueNode = Cast<UARDialogueEdGraphNode>(GraphNode);
			if (DialogueNode && DialogueNode->RuntimeNode.NodeType == EDialogueNodeType::Enter)
			{
				return true;
			}
		}
		return false;
	}

	static FText GetNodeDisplayName(const EDialogueNodeType NodeType)
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
			return FText::FromString(TEXT("Switch On Tags By Priority"));
		case EDialogueNodeType::TagMutation:
			return FText::FromString(TEXT("Add / Remove Tag"));
		case EDialogueNodeType::RelationshipMutation:
			return FText::FromString(TEXT("Modify Relationship"));
		case EDialogueNodeType::FactionMutation:
			return FText::FromString(TEXT("Modify Faction Popularity"));
		case EDialogueNodeType::Random:
			return FText::FromString(TEXT("Random"));
		default:
			return FText::FromString(TEXT("Unknown"));
		}
	}

	struct FARDialogueGraphSchemaAction_NewNode final : public FEdGraphSchemaAction
	{
		FARDialogueGraphSchemaAction_NewNode()
			: FEdGraphSchemaAction()
		{
		}

		FARDialogueGraphSchemaAction_NewNode(const FText& Category, const FText& MenuDesc, const FText& ToolTip, const int32 Grouping, const EDialogueNodeType InNodeType)
			: FEdGraphSchemaAction(Category, MenuDesc, ToolTip, Grouping)
			, NodeType(InNodeType)
		{
		}

		virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, const bool bSelectNewNode = true) override
		{
			if (!ParentGraph)
			{
				return nullptr;
			}
			if (NodeType == EDialogueNodeType::Enter && GraphHasEnterNode(ParentGraph))
			{
				return nullptr;
			}

			ParentGraph->Modify();
			UARDialogueEdGraphNode* NewNode = NewObject<UARDialogueEdGraphNode>(ParentGraph);
			NewNode->SetFlags(RF_Transactional);
			NewNode->InitializeForNodeType(NodeType);
			NewNode->NodePosX = static_cast<int32>(Location.X);
			NewNode->NodePosY = static_cast<int32>(Location.Y);
			NewNode->CreateNewGuid();
			NewNode->PostPlacedNewNode();
			NewNode->AllocateDefaultPins();
			ParentGraph->AddNode(NewNode, true, bSelectNewNode);

			if (FromPin)
			{
				NewNode->AutowireNewNode(FromPin);
			}

			return NewNode;
		}

		EDialogueNodeType NodeType = EDialogueNodeType::Line;
	};
}

void UARDialogueEdGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	Graph.Modify();
	UARDialogueEdGraphNode* EnterNode = NewObject<UARDialogueEdGraphNode>(&Graph);
	EnterNode->SetFlags(RF_Transactional);
	EnterNode->InitializeForNodeType(EDialogueNodeType::Enter);
	EnterNode->NodePosX = -300;
	EnterNode->NodePosY = 0;
	EnterNode->CreateNewGuid();
	EnterNode->AllocateDefaultPins();
	Graph.AddNode(EnterNode, true, false);
}

void UARDialogueEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	static const TArray<EDialogueNodeType> NodeTypes = {
		EDialogueNodeType::Completed,
		EDialogueNodeType::Line,
		EDialogueNodeType::Choice,
		EDialogueNodeType::Bool,
		EDialogueNodeType::SwitchOnTagsByPriority,
		EDialogueNodeType::TagMutation,
		EDialogueNodeType::RelationshipMutation,
		EDialogueNodeType::FactionMutation,
		EDialogueNodeType::Random
	};

	for (const EDialogueNodeType NodeType : NodeTypes)
	{
		const FText DisplayName = GetNodeDisplayName(NodeType);
		TSharedPtr<FARDialogueGraphSchemaAction_NewNode> NewAction = MakeShared<FARDialogueGraphSchemaAction_NewNode>(
			FText::GetEmpty(),
			DisplayName,
			DisplayName,
			0,
			NodeType);
		ContextMenuBuilder.AddAction(NewAction);
	}
}

const FPinConnectionResponse UARDialogueEdGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (!A || !B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Invalid pins."));
	}

	if (A == B || A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Cannot connect a node to itself."));
	}

	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Pins must have opposite direction."));
	}

	if (A->PinType.PinCategory != PinCategoryExec || B->PinType.PinCategory != PinCategoryExec)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Only dialogue exec pins can be connected."));
	}

	const UEdGraphPin* OutputPin = A->Direction == EGPD_Output ? A : B;
	if (OutputPin->LinkedTo.Num() > 0)
	{
		return A->Direction == EGPD_Output
			? FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("Output pin only supports one outgoing connection."))
			: FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT("Output pin only supports one outgoing connection."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

FLinearColor UARDialogueEdGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == PinCategoryExec)
	{
		return FLinearColor(0.82f, 0.82f, 0.82f, 1.0f);
	}

	return FLinearColor::White;
}

bool UARDialogueEdGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	(void)Pin;
	return true;
}
