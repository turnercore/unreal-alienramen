#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ParleyDialogueConditionCompileUtils.h"
#include "ParleyDialogueEdGraph.h"
#include "ParleyDialogueEdGraphNode.h"
#include "ParleyDialogueEdGraphSchema.h"
#include "EdGraph/EdGraphPin.h"
#include "GameplayTagsManager.h"

namespace
{
	UParleyDialogueEdGraph* CreateTestGraph()
	{
		UParleyDialogueEdGraph* Graph = NewObject<UParleyDialogueEdGraph>(GetTransientPackage());
		Graph->Schema = UParleyDialogueEdGraphSchema::StaticClass();
		return Graph;
	}

	UParleyDialogueEdGraphNode* AddTestNode(UParleyDialogueEdGraph* Graph, const EDialogueEditorNodeType NodeType)
	{
		UParleyDialogueEdGraphNode* Node = NewObject<UParleyDialogueEdGraphNode>(Graph);
		Node->SetFlags(RF_Transactional);
		Node->InitializeForNodeType(NodeType);
		Node->CreateNewGuid();
		Node->AllocateDefaultPins();
		Graph->AddNode(Node, false, false);
		return Node;
	}

	UEdGraphPin* GetBranchConditionInputPin(UParleyDialogueEdGraphNode* BranchNode, const int32 InputIndex)
	{
		if (!BranchNode)
		{
			return nullptr;
		}

		const FDialogueEditorBranchNodeData* BranchData = BranchNode->RuntimeNode.NodeData.GetPtr<FDialogueEditorBranchNodeData>();
		if (!BranchData || !BranchData->Inputs.IsValidIndex(InputIndex))
		{
			return nullptr;
		}

		return BranchNode->GetConditionInputPin(BranchData->Inputs[InputIndex].InputId);
	}

	void LinkSourceToBranchInput(UParleyDialogueEdGraphNode* SourceNode, UParleyDialogueEdGraphNode* BranchNode, const int32 InputIndex)
	{
		check(SourceNode);
		check(BranchNode);

		UEdGraphPin* SourceOutputPin = SourceNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameTrue());
		UEdGraphPin* BranchInputPin = GetBranchConditionInputPin(BranchNode, InputIndex);
		check(SourceOutputPin);
		check(BranchInputPin);

		SourceOutputPin->MakeLinkTo(BranchInputPin);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParleyConditionSourceMappingTest,
	"AlienRamen.Parley.Editor.ConditionCompile.SourceMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParleyConditionSourceMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UParleyDialogueEdGraph* Graph = CreateTestGraph();
	TestNotNull(TEXT("Graph created"), Graph);
	if (!Graph)
	{
		return false;
	}

	FString Error;
	FDialogueCondition Condition;

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckTags);
		FDialogueEditorCheckTagsNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckTagsNodeData>();
		Data->Source = EDialogueEditorTagConditionSource::GameTags;
		Data->Operator = EDialogueComparisonOp::Contains;
		TestTrue(TEXT("CheckTags condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckTags source"), Condition.Source, EDialogueConditionSource::GameTags);
		TestEqual(TEXT("CheckTags operator"), Condition.Operator, EDialogueComparisonOp::Contains);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckRelationship);
		FDialogueEditorCheckRelationshipNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
		Data->Source = EDialogueEditorRelationshipConditionSource::RelationshipLevel;
		Data->Operator = EDialogueComparisonOp::GreaterOrEqual;
		Data->TargetSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Test")), false);
		Data->NumericValue = 3.0f;
		TestTrue(TEXT("CheckRelationship condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckRelationship source"), Condition.Source, EDialogueConditionSource::RelationshipLevel);
		TestEqual(TEXT("CheckRelationship operator"), Condition.Operator, EDialogueComparisonOp::GreaterOrEqual);
		TestEqual(TEXT("CheckRelationship target speaker"), Condition.TagValue, Data->TargetSpeakerTag);
		TestEqual(TEXT("CheckRelationship numeric"), Condition.NumericValue, 3.0f);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckRelationship);
		FDialogueEditorCheckRelationshipNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckRelationshipNodeData>();
		Data->Source = EDialogueEditorRelationshipConditionSource::FactionSpeakerReputation;
		Data->Operator = EDialogueComparisonOp::LessOrEqual;
		Data->FactionTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Faction.Identity.Test")), false);
		Data->TargetSpeakerTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.Speaker.TestB")), false);
		Data->NumericValue = 7.0f;
		TestTrue(TEXT("CheckRelationship faction condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckRelationship faction source"), Condition.Source, EDialogueConditionSource::FactionSpeakerReputation);
		TestEqual(TEXT("CheckRelationship faction operator"), Condition.Operator, EDialogueComparisonOp::LessOrEqual);
		TestEqual(TEXT("CheckRelationship faction tag"), Condition.TagValue, Data->FactionTag);
		TestEqual(TEXT("CheckRelationship faction speaker tag"), Condition.SecondaryTagValue, Data->TargetSpeakerTag);
		TestEqual(TEXT("CheckRelationship faction numeric"), Condition.NumericValue, 7.0f);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckProgress);
		FDialogueEditorCheckProgressNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckProgressNodeData>();
		Data->Source = EDialogueEditorProgressConditionSource::CompletedByPlayer;
		Data->Operator = EDialogueComparisonOp::NotEquals;
		Data->bExpectedValue = false;
		TestTrue(TEXT("CheckProgress condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckProgress source"), Condition.Source, EDialogueConditionSource::CompletedByPlayer);
		TestEqual(TEXT("CheckProgress operator"), Condition.Operator, EDialogueComparisonOp::NotEquals);
		TestEqual(TEXT("CheckProgress numeric payload"), Condition.NumericValue, 0.0f);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckTags);
		FDialogueEditorCheckTagsNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckTagsNodeData>();
		Data->Source = EDialogueEditorTagConditionSource::PlayerTags;
		Data->Operator = EDialogueComparisonOp::Present;
		Data->TagValue = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.TestTag")), false);
		TestTrue(TEXT("Second CheckTags condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("Second CheckTags source"), Condition.Source, EDialogueConditionSource::PlayerTags);
		TestEqual(TEXT("Second CheckTags operator"), Condition.Operator, EDialogueComparisonOp::Present);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckLoadout);
		FDialogueEditorCheckLoadoutNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckLoadoutNodeData>();
		Data->Operator = EDialogueComparisonOp::Present;
		TestTrue(TEXT("CheckLoadout condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckLoadout source"), Condition.Source, EDialogueConditionSource::Loadout);
		TestEqual(TEXT("CheckLoadout operator"), Condition.Operator, EDialogueComparisonOp::Present);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckCharacter);
		FDialogueEditorCheckCharacterNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckCharacterNodeData>();
		Data->Character = EDialogueEditorCharacterCondition::Sister;
		const FGameplayTag ExpectedTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.Speaker.Sister")), false);
		TestTrue(TEXT("CheckCharacter condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckCharacter source"), Condition.Source, EDialogueConditionSource::ActiveCharacter);
		TestEqual(TEXT("CheckCharacter operator"), Condition.Operator, EDialogueComparisonOp::Present);
		TestTrue(TEXT("CheckCharacter tag matches expected"), Condition.TagValue == ExpectedTag);
	}

	{
		UParleyDialogueEdGraphNode* Node = AddTestNode(Graph, EDialogueEditorNodeType::CheckVariable);
		FDialogueEditorCheckVariableNodeData* Data = Node->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckVariableNodeData>();
		Data->VariableName = TEXT("QuestStage");
		Data->Operator = EDialogueComparisonOp::Equals;
		Data->InjectedValue.ValueType = EDialogueInjectedValueType::Integer;
		Data->InjectedValue.IntValue = 7;
		TestTrue(TEXT("CheckVariable condition builds"), ParleyDialogueConditionCompile::BuildConditionFromSourceNode(Node, Condition, Error));
		TestEqual(TEXT("CheckVariable source"), Condition.Source, EDialogueConditionSource::InjectedVariable);
		TestEqual(TEXT("CheckVariable operator"), Condition.Operator, EDialogueComparisonOp::Equals);
		TestEqual(TEXT("CheckVariable name"), Condition.VariableName, FName(TEXT("QuestStage")));
		TestEqual(TEXT("CheckVariable value type"), Condition.InjectedValue.ValueType, EDialogueInjectedValueType::Integer);
		TestEqual(TEXT("CheckVariable int payload"), Condition.InjectedValue.IntValue, 7);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParleyBranchConditionGroupAndOrTest,
	"AlienRamen.Parley.Editor.ConditionCompile.BranchAndOr",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParleyBranchConditionGroupAndOrTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UParleyDialogueEdGraph* Graph = CreateTestGraph();
	TestNotNull(TEXT("Graph created"), Graph);
	if (!Graph)
	{
		return false;
	}

	UParleyDialogueEdGraphNode* BranchNode = AddTestNode(Graph, EDialogueEditorNodeType::Branch);
	UParleyDialogueEdGraphNode* CheckProgressNode = AddTestNode(Graph, EDialogueEditorNodeType::CheckProgress);
	UParleyDialogueEdGraphNode* CheckTagsNode = AddTestNode(Graph, EDialogueEditorNodeType::CheckTags);
	TestNotNull(TEXT("Branch node created"), BranchNode);
	TestNotNull(TEXT("CheckProgress node created"), CheckProgressNode);
	TestNotNull(TEXT("CheckTags node created"), CheckTagsNode);
	if (!BranchNode || !CheckProgressNode || !CheckTagsNode)
	{
		return false;
	}

	FDialogueEditorCheckProgressNodeData* ProgressData = CheckProgressNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckProgressNodeData>();
	ProgressData->Source = EDialogueEditorProgressConditionSource::SeenByPlayer;
	ProgressData->Operator = EDialogueComparisonOp::Equals;
	ProgressData->bExpectedValue = true;

	FDialogueEditorCheckTagsNodeData* TagsData = CheckTagsNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorCheckTagsNodeData>();
	TagsData->Source = EDialogueEditorTagConditionSource::CombinedTags;
	TagsData->Operator = EDialogueComparisonOp::Present;
	TagsData->TagValue = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Dialogue.TestCondition")), false);

	BranchNode->AddDynamicBranchPin();
	TestNotNull(TEXT("Second condition input pin exists"), GetBranchConditionInputPin(BranchNode, 1));
	LinkSourceToBranchInput(CheckProgressNode, BranchNode, 0);
	LinkSourceToBranchInput(CheckTagsNode, BranchNode, 1);

	FDialogueEditorBranchNodeData* BranchData = BranchNode->RuntimeNode.NodeData.GetMutablePtr<FDialogueEditorBranchNodeData>();
	TestNotNull(TEXT("Branch payload is valid"), BranchData);
	if (!BranchData)
	{
		return false;
	}

	{
		BranchData->MatchMode = EDialogueConditionMatchMode::All;
		FDialogueConditionGroup Group;
		TArray<FString> Warnings;
		TArray<FString> Errors;
		TestTrue(TEXT("Build condition group (AND)"), ParleyDialogueConditionCompile::BuildConditionGroupFromBranchNode(BranchNode, Group, Warnings, Errors));
		TestEqual(TEXT("AND match mode"), Group.MatchMode, EDialogueConditionMatchMode::All);
		TestEqual(TEXT("AND conditions count"), Group.Conditions.Num(), 2);
		if (Group.Conditions.Num() == 2)
		{
			TestEqual(TEXT("AND first source preserves pin order"), Group.Conditions[0].Source, EDialogueConditionSource::SeenByPlayer);
			TestEqual(TEXT("AND second source preserves pin order"), Group.Conditions[1].Source, EDialogueConditionSource::CombinedTags);
		}
	}

	{
		BranchData->MatchMode = EDialogueConditionMatchMode::Any;
		FDialogueConditionGroup Group;
		TArray<FString> Warnings;
		TArray<FString> Errors;
		TestTrue(TEXT("Build condition group (OR)"), ParleyDialogueConditionCompile::BuildConditionGroupFromBranchNode(BranchNode, Group, Warnings, Errors));
		TestEqual(TEXT("OR match mode"), Group.MatchMode, EDialogueConditionMatchMode::Any);
		TestEqual(TEXT("OR conditions count"), Group.Conditions.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParleyBranchConditionValidationTest,
	"AlienRamen.Parley.Editor.ConditionCompile.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParleyBranchConditionValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UParleyDialogueEdGraph* Graph = CreateTestGraph();
	TestNotNull(TEXT("Graph created"), Graph);
	if (!Graph)
	{
		return false;
	}

	UParleyDialogueEdGraphNode* BranchNode = AddTestNode(Graph, EDialogueEditorNodeType::Branch);
	TestNotNull(TEXT("Branch node created"), BranchNode);
	if (!BranchNode)
	{
		return false;
	}

	FDialogueConditionGroup Group;
	TArray<FString> Warnings;
	TArray<FString> Errors;
	const bool bBuilt = ParleyDialogueConditionCompile::BuildConditionGroupFromBranchNode(BranchNode, Group, Warnings, Errors);
	TestFalse(TEXT("Branch with no connected condition inputs fails"), bBuilt);
	TestTrue(
		TEXT("No-condition branch reports explicit error"),
		Errors.ContainsByPredicate([](const FString& Message)
		{
			return Message.Contains(TEXT("requires at least one connected condition input"));
		}));

	UParleyDialogueEdGraphNode* SourceA = AddTestNode(Graph, EDialogueEditorNodeType::CheckProgress);
	UParleyDialogueEdGraphNode* SourceB = AddTestNode(Graph, EDialogueEditorNodeType::CheckTags);
	TestNotNull(TEXT("Validation source A created"), SourceA);
	TestNotNull(TEXT("Validation source B created"), SourceB);
	if (!SourceA || !SourceB)
	{
		return false;
	}

	UEdGraphPin* BranchInput = GetBranchConditionInputPin(BranchNode, 0);
	UEdGraphPin* SourceAOut = SourceA->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameTrue());
	UEdGraphPin* SourceBOut = SourceB->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameTrue());
	TestNotNull(TEXT("Validation branch input pin"), BranchInput);
	TestNotNull(TEXT("Validation source A output"), SourceAOut);
	TestNotNull(TEXT("Validation source B output"), SourceBOut);
	if (!BranchInput || !SourceAOut || !SourceBOut)
	{
		return false;
	}

	SourceAOut->MakeLinkTo(BranchInput);
	// Intentionally corrupt the link list to validate compile-time protection for multiple incoming links.
	BranchInput->LinkedTo.AddUnique(SourceBOut);
	SourceBOut->LinkedTo.AddUnique(BranchInput);

	Warnings.Reset();
	Errors.Reset();
	const bool bBuiltWithCorruptLinks = ParleyDialogueConditionCompile::BuildConditionGroupFromBranchNode(BranchNode, Group, Warnings, Errors);
	TestFalse(TEXT("Branch with multiple incoming links fails"), bBuiltWithCorruptLinks);
	TestTrue(
		TEXT("Multiple-link branch reports explicit error"),
		Errors.ContainsByPredicate([](const FString& Message)
		{
			return Message.Contains(TEXT("multiple links"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParleyConditionSchemaRulesTest,
	"AlienRamen.Parley.Editor.ConditionCompile.SchemaRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParleyConditionSchemaRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UParleyDialogueEdGraph* Graph = CreateTestGraph();
	TestNotNull(TEXT("Graph created"), Graph);
	if (!Graph)
	{
		return false;
	}

	const UParleyDialogueEdGraphSchema* Schema = GetDefault<UParleyDialogueEdGraphSchema>();
	TestNotNull(TEXT("Schema available"), Schema);
	if (!Schema)
	{
		return false;
	}

	UParleyDialogueEdGraphNode* RouteNode = AddTestNode(Graph, EDialogueEditorNodeType::Route);
	UParleyDialogueEdGraphNode* BranchA = AddTestNode(Graph, EDialogueEditorNodeType::Branch);
	UParleyDialogueEdGraphNode* BranchB = AddTestNode(Graph, EDialogueEditorNodeType::Branch);
	UParleyDialogueEdGraphNode* SourceA = AddTestNode(Graph, EDialogueEditorNodeType::CheckProgress);
	UParleyDialogueEdGraphNode* SourceB = AddTestNode(Graph, EDialogueEditorNodeType::CheckTags);
	TestNotNull(TEXT("Route node created"), RouteNode);
	TestNotNull(TEXT("Branch A created"), BranchA);
	TestNotNull(TEXT("Branch B created"), BranchB);
	TestNotNull(TEXT("Source A created"), SourceA);
	TestNotNull(TEXT("Source B created"), SourceB);
	if (!RouteNode || !BranchA || !BranchB || !SourceA || !SourceB)
	{
		return false;
	}

	UEdGraphPin* RouteExecOut = RouteNode->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameNext());
	UEdGraphPin* BranchExecIn = BranchA->GetExecInputPin();
	UEdGraphPin* BranchConditionIn = GetBranchConditionInputPin(BranchA, 0);
	UEdGraphPin* BranchBConditionIn = GetBranchConditionInputPin(BranchB, 0);
	UEdGraphPin* SourceAOut = SourceA->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameTrue());
	UEdGraphPin* SourceBOut = SourceB->GetOutputPinByName(UParleyDialogueEdGraphNode::GetPinNameTrue());
	TestNotNull(TEXT("Route exec output pin"), RouteExecOut);
	TestNotNull(TEXT("Branch exec input pin"), BranchExecIn);
	TestNotNull(TEXT("Branch A condition input pin"), BranchConditionIn);
	TestNotNull(TEXT("Branch B condition input pin"), BranchBConditionIn);
	TestNotNull(TEXT("Source A output pin"), SourceAOut);
	TestNotNull(TEXT("Source B output pin"), SourceBOut);
	if (!RouteExecOut || !BranchExecIn || !BranchConditionIn || !BranchBConditionIn || !SourceAOut || !SourceBOut)
	{
		return false;
	}

	{
		const FPinConnectionResponse Response = Schema->CanCreateConnection(RouteExecOut, BranchConditionIn);
		TestEqual(TEXT("Exec output to condition input is disallowed"), Response.Response, CONNECT_RESPONSE_DISALLOW);
	}

	{
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourceAOut, BranchExecIn);
		TestEqual(TEXT("Condition output to exec input is disallowed"), Response.Response, CONNECT_RESPONSE_DISALLOW);
	}

	{
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourceAOut, BranchConditionIn);
		TestEqual(TEXT("Condition source to branch condition input is allowed"), Response.Response, CONNECT_RESPONSE_MAKE);
	}

	SourceAOut->MakeLinkTo(BranchConditionIn);

	{
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourceBOut, BranchConditionIn);
		const bool bBreakOthers = Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_A
			|| Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_B;
		TestTrue(TEXT("Second link into same branch condition input requests break-others"), bBreakOthers);
	}

	{
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourceAOut, BranchBConditionIn);
		TestEqual(TEXT("Condition source output can fan out to another branch input"), Response.Response, CONNECT_RESPONSE_MAKE);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
