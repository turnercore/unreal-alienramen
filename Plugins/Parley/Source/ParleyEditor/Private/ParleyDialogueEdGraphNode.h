#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "ParleyDialogueTypes.h"
#include "ParleyDialogueEdGraphNode.generated.h"

class UGraphNodeContextMenuContext;
class UToolMenu;

UENUM(meta = (ToolTip = "Editor-only dialogue node kinds used by the conversation graph authoring UI."))
enum class EDialogueEditorNodeType : uint8
{
	Enter = 0 UMETA(DisplayName = "Enter"),
	Completed UMETA(DisplayName = "Completed"),
	Line UMETA(DisplayName = "Line"),
	Choice UMETA(DisplayName = "Choice"),
	Branch UMETA(DisplayName = "Branch"),
	SwitchOnTagsByPriority UMETA(DisplayName = "Switch On Tags By Priority"),
	TagMutation UMETA(DisplayName = "Tag Mutation"),
	RelationshipMutation UMETA(DisplayName = "Relationship Mutation"),
	FactionMutation UMETA(DisplayName = "Faction Mutation"),
	Random UMETA(DisplayName = "Random"),
	Route UMETA(DisplayName = "Route"),
	Sequence UMETA(DisplayName = "Sequence"),
	MultiLine UMETA(DisplayName = "Multi-Line"),
	SplitLine UMETA(DisplayName = "Split Line"),
	RouteByCharacter UMETA(DisplayName = "Route By Character"),
	Signal UMETA(DisplayName = "Signal"),
	CheckTags UMETA(DisplayName = "Check Tags"),
	CheckRelationship UMETA(DisplayName = "Check Relationship"),
	CheckProgress UMETA(DisplayName = "Check Progress"),
	CheckStats UMETA(DisplayName = "Check Stats"),
	CheckLoadout UMETA(DisplayName = "Check Loadout"),
	CheckCharacter UMETA(DisplayName = "Check Character"),
	CheckVariable UMETA(DisplayName = "Check Variable")
};

UENUM(meta = (ToolTip = "Editor-only source selector for CheckTags nodes."))
enum class EDialogueEditorTagConditionSource : uint8
{
	CombinedTags = 0 UMETA(DisplayName = "Combined Tags"),
	PlayerTags UMETA(DisplayName = "Player Tags"),
	GameTags UMETA(DisplayName = "Game Tags"),
	TransientConversationTags UMETA(DisplayName = "Transient Conversation Tags")
};

UENUM(meta = (ToolTip = "Editor-only source selector for CheckRelationship nodes."))
enum class EDialogueEditorRelationshipConditionSource : uint8
{
	RelationshipPoints = 0 UMETA(DisplayName = "Relationship Points"),
	RelationshipLevel UMETA(DisplayName = "Relationship Level")
};

UENUM(meta = (ToolTip = "Editor-only source selector for CheckProgress nodes."))
enum class EDialogueEditorProgressConditionSource : uint8
{
	SeenByPlayer = 0 UMETA(DisplayName = "Seen By Player"),
	SeenByGame UMETA(DisplayName = "Seen By Game"),
	CompletedByPlayer UMETA(DisplayName = "Completed By Player"),
	CompletedByGame UMETA(DisplayName = "Completed By Game")
};

UENUM(meta = (ToolTip = "Editor-only source selector for CheckStats nodes."))
enum class EDialogueEditorStatsConditionSource : uint8
{
	PlayerKills = 0 UMETA(DisplayName = "Player Kills"),
	TimePlayed UMETA(DisplayName = "Time Played")
};

UENUM(meta = (ToolTip = "Editor-only active-character selector for CheckCharacter nodes."))
enum class EDialogueEditorCharacterCondition : uint8
{
	Brother = 0 UMETA(DisplayName = "Brother"),
	Sister UMETA(DisplayName = "Sister")
};

USTRUCT()
struct FDialogueEditorConditionInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", AdvancedDisplay, meta = (DisplayName = "Input ID", ToolTip = "Stable identifier for this branch condition input pin."))
	FGuid InputId;
};

USTRUCT()
struct FDialogueEditorBranchNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Match Mode", ToolTip = "How connected condition inputs are combined before routing to True/False."))
	EDialogueConditionMatchMode MatchMode = EDialogueConditionMatchMode::All;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Condition Inputs", ToolTip = "Stable ordered condition input slots for this branch node."))
	TArray<FDialogueEditorConditionInput> Inputs;
};

USTRUCT()
struct FDialogueEditorCheckTagsNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Source", ToolTip = "Tag container to evaluate."))
	EDialogueEditorTagConditionSource Source = EDialogueEditorTagConditionSource::CombinedTags;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Tag comparison operation."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Present;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Tag Value", ToolTip = "Tag operand used by the comparison."))
	FGameplayTag TagValue;
};

USTRUCT()
struct FDialogueEditorCheckRelationshipNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Source", ToolTip = "Relationship value to evaluate."))
	EDialogueEditorRelationshipConditionSource Source = EDialogueEditorRelationshipConditionSource::RelationshipPoints;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Numeric comparison operation."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::GreaterOrEqual;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Numeric Value", ToolTip = "Numeric operand used by the comparison."))
	float NumericValue = 0.0f;
};

USTRUCT()
struct FDialogueEditorCheckProgressNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Source", ToolTip = "Progress state to evaluate."))
	EDialogueEditorProgressConditionSource Source = EDialogueEditorProgressConditionSource::SeenByPlayer;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Boolean comparison operation."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Equals;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Expected Value", ToolTip = "Expected bool result for this progress flag."))
	bool bExpectedValue = true;
};

USTRUCT()
struct FDialogueEditorCheckStatsNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Source", ToolTip = "Player stat to evaluate."))
	EDialogueEditorStatsConditionSource Source = EDialogueEditorStatsConditionSource::PlayerKills;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Numeric comparison operation."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::GreaterOrEqual;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Numeric Value", ToolTip = "Numeric operand used by the comparison."))
	float NumericValue = 0.0f;
};

USTRUCT()
struct FDialogueEditorCheckLoadoutNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Loadout tag comparison operation."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Present;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Tag Value", ToolTip = "Loadout tag operand used by the comparison."))
	FGameplayTag TagValue;
};

USTRUCT()
struct FDialogueEditorCheckCharacterNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Character", ToolTip = "Active character required for this condition to pass."))
	EDialogueEditorCharacterCondition Character = EDialogueEditorCharacterCondition::Brother;
};

USTRUCT()
struct FDialogueEditorCheckVariableNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Variable Name", ToolTip = "Injected variable key to evaluate."))
	FName VariableName;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Comparison operation used for the injected variable."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Equals;

	UPROPERTY(EditAnywhere, Category = "", meta = (DisplayName = "Injected Value", ToolTip = "Injected variable value to compare against."))
	FDialogueInjectedValue InjectedValue;
};

UCLASS(CollapseCategories)
class UParleyDialogueEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "", meta = (ToolTip = "Editor-facing node type used for graph authoring."))
	EDialogueEditorNodeType EditorNodeType = EDialogueEditorNodeType::Line;

	UPROPERTY(EditAnywhere, Category = "", meta = (ShowOnlyInnerProperties, ToolTip = "Node payload and compile-managed links for this graph node."))
	FDialogueCompiledNode RuntimeNode;

	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void PrepareForCopying() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void InitializeForNodeType(EDialogueEditorNodeType NodeType);
	void EnsureStableIds(bool bRegenerateNodeId, bool bRegenerateBranchIds);
	void ClearRuntimeLinks();

	UEdGraphPin* GetExecInputPin() const;
	UEdGraphPin* GetOutputPinByName(const FName PinName) const;
	UEdGraphPin* GetConditionInputPin(const FGuid& InputId) const;
	UEdGraphPin* GetChoiceOutputPin(const FGuid& ChoiceBranchId) const;
	UEdGraphPin* GetSwitchOutputPin(const FGuid& BranchId) const;
	UEdGraphPin* GetRandomOutputPin(const FGuid& BranchId) const;
	UEdGraphPin* GetSequenceOutputPin(const FGuid& BranchId) const;
	UEdGraphPin* GetCharacterRouteOutputPin(const FGuid& BranchId) const;

	bool SupportsDynamicBranchPins() const;
	void AddDynamicBranchPin();
	bool RemoveLastDynamicBranchPin();
	bool RemoveDynamicBranchPinByName(FName PinName);
	bool SetBranchMatchMode(EDialogueConditionMatchMode NewMatchMode);

	bool SetChoiceBranchText(const FGuid& ChoiceBranchId, const FText& NewText);
	bool SetSwitchBranchLabel(const FGuid& BranchId, const FText& NewLabel);
	bool SetCharacterRouteBranchSpeakerTag(const FGuid& BranchId, const FGameplayTag& NewSpeakerTag);
	bool SetRandomBranchWeight(const FGuid& BranchId, float NewWeight);
	bool MoveChoiceBranch(const FGuid& ChoiceBranchId, bool bMoveUp);
	bool MoveSwitchBranch(const FGuid& BranchId, bool bMoveUp);
	bool MoveCharacterRouteBranch(const FGuid& BranchId, bool bMoveUp);
	bool ReorderChoiceBranch(const FGuid& MovingChoiceBranchId, const FGuid& TargetChoiceBranchId);
	bool ReorderSwitchBranch(const FGuid& MovingBranchId, const FGuid& TargetBranchId);
	bool ReorderRandomBranch(const FGuid& MovingBranchId, const FGuid& TargetBranchId);
	bool ReorderCharacterRouteBranch(const FGuid& MovingBranchId, const FGuid& TargetBranchId);
	bool SetChoiceFallbackText(const FText& NewFallbackText);
	bool SetRelationshipTargetSpeakerTag(const FGameplayTag& NewTag);
	bool SetRelationshipDeltaPoints(float NewDeltaPoints);
	bool SetFactionTag(const FGameplayTag& NewTag);
	bool SetFactionDeltaPopularity(float NewDeltaPopularity);
	bool AddMultiLineEntry();
	bool RemoveMultiLineEntry(const FGuid& EntryId);
	bool ReorderMultiLineEntry(const FGuid& MovingEntryId, const FGuid& TargetEntryId);
	bool SetMultiLineEntrySpeakerTag(const FGuid& EntryId, const FGameplayTag& NewSpeakerTag);
	bool SetMultiLineEntryText(const FGuid& EntryId, const FText& NewText);

	void ApplyValidation(EDialogueValidationSeverity Severity, const FString& Message);
	void ClearValidation();

	static FName GetPinNameIn();
	static FName GetPinNameNext();
	static FName GetPinNameTrue();
	static FName GetPinNameFalse();
	static FName GetPinNameFallback();
	static FName GetPinNameSwitchDefault();
	static FName MakeConditionPinName(const FGuid& InputId);

	static FName MakeChoicePinName(const FGuid& ChoiceBranchId);
	static FName MakeSwitchPinName(const FGuid& BranchId);
	static FName MakeRandomPinName(const FGuid& BranchId);
	static FName MakeSequencePinName(const FGuid& BranchId);
	static FName MakeCharacterRoutePinName(const FGuid& BranchId);
	static EDialogueEditorNodeType MakeEditorNodeTypeFromRuntime(EDialogueNodeType NodeType);
	static EDialogueNodeType MakeRuntimeNodeTypeFromEditor(EDialogueEditorNodeType NodeType);

private:
	bool CommitRuntimeNodeMutation(const FText& TransactionText, TFunctionRef<bool()> MutateFn, bool bReconstructPins);
	static bool TryParseBranchGuidFromPinName(const FName PinName, const FString& Prefix, FGuid& OutBranchId);

	void EnsureNodeDataMatchesNodeType();
	void EnsureBranchAndLineIds(bool bRegenerateBranches, bool bRegenerateLineGuid);
	void AddInputPinIfNeeded();
	void AddNextOutputPinIfNeeded();
	void AddConditionInputPins();
	void AddChoicePins();
	void AddSwitchPins();
	void AddRandomPins();
	void AddSequencePins();
	void AddCharacterRoutePins();
	FString BuildInlineSummary() const;

	EDialogueValidationSeverity ValidationSeverity = EDialogueValidationSeverity::Info;
	FString ValidationMessage;
};
