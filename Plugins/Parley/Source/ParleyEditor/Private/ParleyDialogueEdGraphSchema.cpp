#include "ParleyDialogueEdGraphSchema.h"

#include "ParleyDialogueEdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"

namespace
{
	static const FName DialogueSchemaPinCategoryExec(TEXT("DialogueExec"));
	static const FName DialogueSchemaPinCategoryConditionBool(TEXT("DialogueConditionBool"));

	static bool GraphHasEnterNode(const UEdGraph* Graph)
	{
		if (!Graph)
		{
			return false;
		}

		for (const UEdGraphNode* GraphNode : Graph->Nodes)
		{
			const UParleyDialogueEdGraphNode* DialogueNode = Cast<UParleyDialogueEdGraphNode>(GraphNode);
			if (DialogueNode && DialogueNode->EditorNodeType == EDialogueEditorNodeType::Enter)
			{
				return true;
			}
		}
		return false;
	}

	static FText GetNodeDisplayName(const EDialogueEditorNodeType NodeType)
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
			return FText::FromString(TEXT("Switch On Tags By Priority"));
		case EDialogueEditorNodeType::TagMutation:
			return FText::FromString(TEXT("Add / Remove Tag"));
		case EDialogueEditorNodeType::RelationshipMutation:
			return FText::FromString(TEXT("Modify Relationship"));
		case EDialogueEditorNodeType::FactionMutation:
			return FText::FromString(TEXT("Modify Faction Popularity"));
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
			return FText::FromString(TEXT("Route By Character"));
		case EDialogueEditorNodeType::CheckTags:
			return FText::FromString(TEXT("Check Tags"));
		case EDialogueEditorNodeType::CheckRelationship:
			return FText::FromString(TEXT("Check Relationship"));
		case EDialogueEditorNodeType::CheckProgress:
			return FText::FromString(TEXT("Check Progress"));
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

	static FText GetNodeTooltip(const EDialogueEditorNodeType NodeType)
	{
		switch (NodeType)
		{
		case EDialogueEditorNodeType::Completed:
			return FText::FromString(TEXT("Ends the conversation and commits completion/memory results."));
		case EDialogueEditorNodeType::Line:
			return FText::FromString(TEXT("Presents one dialogue line and waits for advance input."));
		case EDialogueEditorNodeType::Choice:
			return FText::FromString(TEXT("Presents player choices and routes to the selected branch."));
		case EDialogueEditorNodeType::Branch:
			return FText::FromString(TEXT("Combines connected bool condition sources and routes to True or False."));
		case EDialogueEditorNodeType::SwitchOnTagsByPriority:
			return FText::FromString(TEXT("Evaluates branch conditions in order and routes to the first passing branch."));
		case EDialogueEditorNodeType::TagMutation:
			return FText::FromString(TEXT("Adds/removes gameplay tags on configured dialogue targets."));
		case EDialogueEditorNodeType::RelationshipMutation:
			return FText::FromString(TEXT("Adjusts relationship points for a target speaker."));
		case EDialogueEditorNodeType::FactionMutation:
			return FText::FromString(TEXT("Adjusts popularity for a target faction."));
		case EDialogueEditorNodeType::Signal:
			return FText::FromString(TEXT("Broadcasts a gameplay tag signal for game systems."));
		case EDialogueEditorNodeType::Random:
			return FText::FromString(TEXT("Selects an outgoing branch by authored weights."));
		case EDialogueEditorNodeType::Route:
			return FText::FromString(TEXT("Organizes graph flow without adding runtime side effects."));
		case EDialogueEditorNodeType::Sequence:
			return FText::FromString(TEXT("Runs each connected branch in order."));
		case EDialogueEditorNodeType::MultiLine:
			return FText::FromString(TEXT("Presents multiple lines from one node in authored order."));
		case EDialogueEditorNodeType::SplitLine:
			return FText::FromString(TEXT("Presents line entries that can be split/filtered by conditions."));
		case EDialogueEditorNodeType::RouteByCharacter:
			return FText::FromString(TEXT("Routes execution based on the active player speaker tag."));
		case EDialogueEditorNodeType::CheckTags:
			return FText::FromString(TEXT("Produces a bool from one tag-based condition."));
		case EDialogueEditorNodeType::CheckRelationship:
			return FText::FromString(TEXT("Produces a bool from relationship points or level."));
		case EDialogueEditorNodeType::CheckProgress:
			return FText::FromString(TEXT("Produces a bool from seen/completed dialogue progress."));
		case EDialogueEditorNodeType::CheckLoadout:
			return FText::FromString(TEXT("Produces a bool from loadout tag checks."));
		case EDialogueEditorNodeType::CheckCharacter:
			return FText::FromString(TEXT("Produces a bool from the active character check."));
		case EDialogueEditorNodeType::CheckVariable:
			return FText::FromString(TEXT("Produces a bool from an injected variable check."));
		case EDialogueEditorNodeType::Enter:
		default:
			return FText::FromString(TEXT("Conversation entry node."));
		}
	}

	static bool IsConditionNodeType(const EDialogueEditorNodeType NodeType)
	{
		return NodeType == EDialogueEditorNodeType::CheckTags
			|| NodeType == EDialogueEditorNodeType::CheckRelationship
			|| NodeType == EDialogueEditorNodeType::CheckProgress
			|| NodeType == EDialogueEditorNodeType::CheckLoadout
			|| NodeType == EDialogueEditorNodeType::CheckCharacter
			|| NodeType == EDialogueEditorNodeType::CheckVariable;
	}

	struct FParleyDialogueGraphSchemaAction_NewNode final : public FEdGraphSchemaAction
	{
		FParleyDialogueGraphSchemaAction_NewNode()
			: FEdGraphSchemaAction()
		{
		}

		FParleyDialogueGraphSchemaAction_NewNode(const FText& Category, const FText& MenuDesc, const FText& ToolTip, const int32 Grouping, const EDialogueEditorNodeType InNodeType)
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
			if (NodeType == EDialogueEditorNodeType::Enter && GraphHasEnterNode(ParentGraph))
			{
				return nullptr;
			}

			ParentGraph->Modify();
			UParleyDialogueEdGraphNode* NewNode = NewObject<UParleyDialogueEdGraphNode>(ParentGraph);
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

		EDialogueEditorNodeType NodeType = EDialogueEditorNodeType::Line;
	};

	struct FParleyDialogueGraphSchemaAction_NewComment final : public FEdGraphSchemaAction
	{
		FParleyDialogueGraphSchemaAction_NewComment()
			: FEdGraphSchemaAction(
				FText::GetEmpty(),
				FText::FromString(TEXT("Add Comment...")),
				FText::FromString(TEXT("Create a resizable comment box.")),
				0)
		{
		}

		virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, const bool bSelectNewNode = true) override
		{
			(void)FromPin;

			if (!ParentGraph)
			{
				return nullptr;
			}

			UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();
			return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, FVector2f(Location), bSelectNewNode);
		}
	};
}

void UParleyDialogueEdGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	Graph.Modify();
	UParleyDialogueEdGraphNode* EnterNode = NewObject<UParleyDialogueEdGraphNode>(&Graph);
	EnterNode->SetFlags(RF_Transactional);
	EnterNode->InitializeForNodeType(EDialogueEditorNodeType::Enter);
	EnterNode->NodePosX = -300;
	EnterNode->NodePosY = 0;
	EnterNode->CreateNewGuid();
	EnterNode->AllocateDefaultPins();
	Graph.AddNode(EnterNode, true, false);
}

void UParleyDialogueEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	ContextMenuBuilder.AddAction(GetCreateCommentAction());

	static const TArray<EDialogueEditorNodeType> NodeTypes = {
		EDialogueEditorNodeType::Completed,
		EDialogueEditorNodeType::Line,
		EDialogueEditorNodeType::Choice,
		EDialogueEditorNodeType::Branch,
		EDialogueEditorNodeType::CheckTags,
		EDialogueEditorNodeType::CheckRelationship,
		EDialogueEditorNodeType::CheckProgress,
		EDialogueEditorNodeType::CheckLoadout,
		EDialogueEditorNodeType::CheckCharacter,
		EDialogueEditorNodeType::CheckVariable,
		EDialogueEditorNodeType::SwitchOnTagsByPriority,
		EDialogueEditorNodeType::TagMutation,
		EDialogueEditorNodeType::RelationshipMutation,
		EDialogueEditorNodeType::FactionMutation,
		EDialogueEditorNodeType::Signal,
		EDialogueEditorNodeType::Random,
		EDialogueEditorNodeType::Route,
		EDialogueEditorNodeType::Sequence,
		EDialogueEditorNodeType::MultiLine,
		EDialogueEditorNodeType::SplitLine,
		EDialogueEditorNodeType::RouteByCharacter
	};

	for (const EDialogueEditorNodeType NodeType : NodeTypes)
	{
		const FText DisplayName = GetNodeDisplayName(NodeType);
		const FText Category = IsConditionNodeType(NodeType)
			? FText::FromString(TEXT("Conditions"))
			: FText::GetEmpty();
		TSharedPtr<FParleyDialogueGraphSchemaAction_NewNode> NewAction = MakeShared<FParleyDialogueGraphSchemaAction_NewNode>(
			Category,
			DisplayName,
			GetNodeTooltip(NodeType),
			0,
			NodeType);
		ContextMenuBuilder.AddAction(NewAction);
	}
}

TSharedPtr<FEdGraphSchemaAction> UParleyDialogueEdGraphSchema::GetCreateCommentAction() const
{
	return MakeShared<FParleyDialogueGraphSchemaAction_NewComment>();
}

const FPinConnectionResponse UParleyDialogueEdGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
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

	const bool bBothExec = A->PinType.PinCategory == DialogueSchemaPinCategoryExec && B->PinType.PinCategory == DialogueSchemaPinCategoryExec;
	const bool bBothCondition = A->PinType.PinCategory == DialogueSchemaPinCategoryConditionBool && B->PinType.PinCategory == DialogueSchemaPinCategoryConditionBool;
	if (!bBothExec && !bBothCondition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Pins must share the same dialogue category."));
	}

	const UEdGraphPin* OutputPin = A->Direction == EGPD_Output ? A : B;
	const UEdGraphPin* InputPin = A->Direction == EGPD_Input ? A : B;

	if (bBothExec && OutputPin->LinkedTo.Num() > 0)
	{
		return A->Direction == EGPD_Output
			? FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("Output pin only supports one outgoing connection."))
			: FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT("Output pin only supports one outgoing connection."));
	}

	if (bBothCondition)
	{
		const UParleyDialogueEdGraphNode* OutputNode = OutputPin ? Cast<UParleyDialogueEdGraphNode>(OutputPin->GetOwningNode()) : nullptr;
		const UParleyDialogueEdGraphNode* InputNode = InputPin ? Cast<UParleyDialogueEdGraphNode>(InputPin->GetOwningNode()) : nullptr;
		const bool bOutputIsConditionSource = OutputNode
			&& (OutputNode->EditorNodeType == EDialogueEditorNodeType::CheckTags
				|| OutputNode->EditorNodeType == EDialogueEditorNodeType::CheckRelationship
				|| OutputNode->EditorNodeType == EDialogueEditorNodeType::CheckProgress
				|| OutputNode->EditorNodeType == EDialogueEditorNodeType::CheckLoadout
				|| OutputNode->EditorNodeType == EDialogueEditorNodeType::CheckCharacter
				|| OutputNode->EditorNodeType == EDialogueEditorNodeType::CheckVariable);
		const bool bInputIsBranch = InputNode && InputNode->EditorNodeType == EDialogueEditorNodeType::Branch;
		if (!bOutputIsConditionSource || !bInputIsBranch)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Condition wires only connect from Check* nodes into Branch condition inputs."));
		}

		if (InputPin && InputPin->LinkedTo.Num() > 0)
		{
			return A->Direction == EGPD_Input
				? FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("Branch condition inputs only support one incoming connection."))
				: FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT("Branch condition inputs only support one incoming connection."));
		}
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

FLinearColor UParleyDialogueEdGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == DialogueSchemaPinCategoryExec)
	{
		return FLinearColor(0.82f, 0.82f, 0.82f, 1.0f);
	}
	if (PinType.PinCategory == DialogueSchemaPinCategoryConditionBool)
	{
		return FLinearColor(0.82f, 0.22f, 0.22f, 1.0f);
	}

	return FLinearColor::White;
}

bool UParleyDialogueEdGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	(void)Pin;
	return true;
}
