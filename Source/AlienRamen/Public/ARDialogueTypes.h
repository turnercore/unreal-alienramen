/**
 * @file ARDialogueTypes.h
 * @brief Shared dialogue runtime, persistence, and authoring data types for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"
#include "ARPlayerTypes.h"
#include "ARSaveTypes.h"
#include "ARDialogueTypes.generated.h"

class AActor;
class APlayerController;
class APlayerState;
class APawn;
class AGameStateBase;
class UARDialogueConversationAsset;
class USoundBase;
class UTexture2D;
class UWorld;

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FSpeakerPortraitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TSoftObjectPtr<UTexture2D> PortraitTexture;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FSpeakerPortraitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag PortraitTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FSpeakerPortraitData Portrait;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueSpeakerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FSpeakerPortraitData DefaultPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<FSpeakerPortraitEntry> Portraits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<float> RelationshipThresholds = { 50.0f, 150.0f, 300.0f, 500.0f };
};

UENUM(BlueprintType)
enum class EDialogueInjectedValueType : uint8
{
	None = 0,
	Bool,
	Integer,
	Float,
	Tag,
	Text
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueInjectedValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Value Type", ToolTip = "Type used to interpret this injected value at runtime and during condition checks."))
	EDialogueInjectedValueType ValueType = EDialogueInjectedValueType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Bool Value", ToolTip = "Boolean payload when Value Type is Bool."))
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Int Value", ToolTip = "Integer payload when Value Type is Integer."))
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Float Value", ToolTip = "Float payload when Value Type is Float."))
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Tag Value", ToolTip = "Gameplay tag payload when Value Type is Tag."))
	FGameplayTag TagValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Text Value", ToolTip = "Text payload when Value Type is Text."))
	FText TextValue;
};

UENUM(BlueprintType)
enum class EDialogueConditionMatchMode : uint8
{
	All = 0,
	Any
};

UENUM(BlueprintType)
enum class EDialogueConditionSource : uint8
{
	CombinedTags = 0,
	PlayerTags,
	GameTags,
	TransientConversationTags,
	ActiveCharacter,
	RelationshipPoints,
	RelationshipLevel,
	SeenByPlayer,
	SeenByGame,
	CompletedByPlayer,
	CompletedByGame,
	PlayerKills,
	TimePlayed,
	Loadout,
	InjectedVariable
};

UENUM(BlueprintType)
enum class EDialogueComparisonOp : uint8
{
	Equals = 0,
	NotEquals,
	GreaterThan,
	GreaterOrEqual,
	LessThan,
	LessOrEqual,
	Contains,
	NotContains,
	Present,
	Absent
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Where this condition reads its value from (tags, runtime stats, booleans, loadout, or injected variables)."))
	EDialogueConditionSource Source = EDialogueConditionSource::CombinedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Comparison operation applied to the selected source."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Present;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Tag Value", ToolTip = "Tag operand for tag-based comparisons (Contains/Present/Equals, etc)."))
	FGameplayTag TagValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Numeric Value", ToolTip = "Numeric operand used for numeric comparisons (relationship points, kills, time played, etc)."))
	float NumericValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Variable Name", ToolTip = "Injected variable key used when Source is InjectedVariable."))
	FName VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Injected Value", ToolTip = "Injected variable value compared against at runtime when Source is InjectedVariable."))
	FDialogueInjectedValue InjectedValue;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConditionGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Match Mode", ToolTip = "How conditions in this group are evaluated: All means every condition must pass, Any means one passing condition is enough."))
	EDialogueConditionMatchMode MatchMode = EDialogueConditionMatchMode::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Conditions evaluated together using Match Mode."))
	TArray<FDialogueCondition> Conditions;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConversationHeader
{
	GENERATED_BODY()

	FDialogueConversationHeader()
	{
		BlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Conversation Tag", ToolTip = "Unique gameplay tag identifier for this conversation. This is the key most systems resolve by."))
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Display Title", ToolTip = "Editor-facing title for authoring and debugging. Does not control spoken line text."))
	FText DisplayTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Speaker", DisplayName = "Primary Speaker Tag", ToolTip = "Primary speaker for gating and runtime context resolution."))
	FGameplayTag PrimarySpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Speaker", DisplayName = "Participating Speaker Tags", ToolTip = "Additional speakers that may participate in this conversation."))
	TArray<FGameplayTag> ParticipatingSpeakerTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Offer-selection priority. Higher values are preferred when multiple conversations are eligible."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Repeatable", ToolTip = "If enabled, this conversation may be offered again after completion when other gates allow it."))
	bool bRepeatable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Important", ToolTip = "Marks this conversation as important for systems that treat priority conversations differently."))
	bool bImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Minimum Relationship Points", ToolTip = "Minimum relationship points required with the primary speaker before this conversation can be offered."))
	float MinimumRelationshipPoints = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Locked Conditions", ToolTip = "Required conditions. If these do not pass, the conversation is locked out."))
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Blocked Conditions", ToolTip = "Blocking conditions. If these pass, the conversation is blocked."))
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Seen By Game Blocks Reoffer", ToolTip = "If enabled, once the game has marked this conversation as seen, it will no longer be reoffered."))
	bool bSeenByGameBlocksReoffer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Seen By Player Blocks Reoffer", ToolTip = "If enabled, once the active player has seen this conversation, it will no longer be reoffered."))
	bool bSeenByPlayerBlocksReoffer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Completed By Game Blocks Reoffer", ToolTip = "If enabled, once this conversation is completed at game scope, it will no longer be reoffered."))
	bool bCompletedByGameBlocksReoffer = false;
};

// TagContentResolver row shape for conversation asset registration.
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueConversationAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	// Authoritative conversation tag. If left unset, the loader will try to build one from the conversation root + row name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Conversation Tag", ToolTip = "Conversation tag served by this registry row. If unset, systems may derive a tag from root+row name."))
	FGameplayTag ConversationTag;

	// Reference to the authored conversation asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Conversation Asset", ToolTip = "Conversation asset resolved for this row."))
	TSoftObjectPtr<UARDialogueConversationAsset> Conversation;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConversationLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Line ID", ToolTip = "Stable per-line guid used for identity and downstream tracking. Auto-generated for new lines."))
	FGuid LocalLineGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Text", ToolTip = "Line text shown to the player when this node executes."))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Sound", ToolTip = "Optional voice/audio clip played with this line."))
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Speaker", DisplayName = "Speaker Tag", ToolTip = "Speaker used when presenting this line. Leave empty to fall back to conversation context."))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Length Seconds", ToolTip = "Optional explicit line duration in seconds for pacing/auto-advance logic."))
	float LengthSeconds = 0.0f;
};

UENUM(BlueprintType)
enum class EDialogueNodeType : uint8
{
	Enter = 0,
	Completed,
	Line,
	Choice,
	Bool,
	SwitchOnTagsByPriority,
	TagMutation,
	RelationshipMutation,
	FactionMutation,
	Random,
	Route,
	Sequence,
	MultiLine
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueLineNodeData
{
	GENERATED_BODY()

	FDialogueLineNodeData()
	{
		SkipBlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ShowOnlyInnerProperties, DisplayName = "Line", ToolTip = "Dialogue line payload for this node."))
	FDialogueConversationLine Line;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Skip Locked Conditions", ToolTip = "If this condition group passes, this line is skipped even if the conversation is active."))
	FDialogueConditionGroup SkipLockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Skip Blocked Conditions", ToolTip = "If this condition group passes, this line is skipped. Defaults to Match Any for convenience."))
	FDialogueConditionGroup SkipBlockedConditions;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueMultiLineEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Entry ID (Compile Managed)", ToolTip = "Stable per-entry identifier used by editor tooling for ordering and editing."))
	FGuid EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ShowOnlyInnerProperties, DisplayName = "Line", ToolTip = "Line payload and skip conditions for this step in the multiline sequence."))
	FDialogueLineNodeData LineData;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueMultiLineNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Lines", ToolTip = "Ordered authored lines executed top-to-bottom inside one node."))
	TArray<FDialogueMultiLineEntry> Lines;
};

UENUM(BlueprintType)
enum class EDialogueCompletedChoicePolicy : uint8
{
	LockedToRecordedChoice = 0,
	FreeChoice
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceOption
{
	GENERATED_BODY()

	FDialogueChoiceOption()
	{
		BlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Choice Branch ID", ToolTip = "Stable identifier for this choice branch. Auto-generated and used to track recorded selections."))
	FGuid ChoiceBranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Choice Text", ToolTip = "Text shown to the player for this choice."))
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Locked Conditions", ToolTip = "Required conditions for this choice to be selectable."))
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Blocked Conditions", ToolTip = "Blocking conditions for this choice. If they pass, the choice is unavailable."))
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Important", ToolTip = "Marks this choice as important for UI/flow systems that highlight critical options."))
	bool bImportant = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Choices", ToolTip = "Authored choices for this node. Each choice creates an output branch."))
	TArray<FDialogueChoiceOption> Choices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Fallback Choice Text", ToolTip = "Text shown for the fallback path when no authored choice can be selected."))
	FText FallbackChoiceText = FText::FromString(TEXT("..."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Node Important", ToolTip = "Marks the whole choice node as important (separate from per-choice importance)."))
	bool bNodeImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Completed Choice Policy", ToolTip = "Controls behavior for revisiting this choice node after completion."))
	EDialogueCompletedChoicePolicy CompletedChoicePolicy = EDialogueCompletedChoicePolicy::LockedToRecordedChoice;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueBoolNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ShowOnlyInnerProperties, DisplayName = "Condition", ToolTip = "Condition evaluated by this Bool node. True/False outputs are chosen from this result."))
	FDialogueCondition Condition;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueSwitchBranch
{
	GENERATED_BODY()

	FDialogueSwitchBranch()
	{
		BlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branch ID", ToolTip = "Stable identifier for this switch branch. Auto-generated when needed."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Label", ToolTip = "Editor-facing label for this branch output pin."))
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Locked Conditions", ToolTip = "Required conditions for this branch to match."))
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Blocked Conditions", ToolTip = "Blocking conditions for this branch. If they pass, this branch is ignored."))
	FDialogueConditionGroup BlockedConditions;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueSwitchNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branches", ToolTip = "Priority-ordered switch branches. First passing branch is selected."))
	TArray<FDialogueSwitchBranch> Branches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Has Default Output", ToolTip = "If enabled, an explicit default output is used when no branch matches."))
	bool bHasDefaultOutput = true;
};

UENUM(BlueprintType)
enum class EDialogueTagMutationTarget : uint8
{
	GameStateProgression = 0,
	ActivePlayerProgression,
	ActivePlayerTransientConversation
};

UENUM(BlueprintType)
enum class EDialogueTagMutationOp : uint8
{
	Add = 0,
	Remove
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueTagMutation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Target", ToolTip = "Where the tag mutation is applied at runtime."))
	EDialogueTagMutationTarget Target = EDialogueTagMutationTarget::GameStateProgression;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Operation", ToolTip = "Whether the tag is added or removed."))
	EDialogueTagMutationOp Operation = EDialogueTagMutationOp::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Tag", ToolTip = "Tag to add/remove from the selected target container."))
	FGameplayTag Tag;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueTagMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Mutations", ToolTip = "Tag mutations executed when this node runs."))
	TArray<FDialogueTagMutation> Mutations;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRelationshipMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Speaker", DisplayName = "Target Speaker Tag", ToolTip = "Speaker whose relationship points are modified."))
	FGameplayTag TargetSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Delta Points", ToolTip = "Signed amount added to relationship points for the target speaker."))
	float DeltaPoints = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueFactionMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Faction Tag", ToolTip = "Faction to modify."))
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Delta Popularity", ToolTip = "Signed amount added to faction popularity."))
	float DeltaPopularity = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRandomBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branch ID", ToolTip = "Stable identifier for this random branch. Auto-generated when needed."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Weight", ToolTip = "Relative random weight. Higher weights are selected more often."))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRandomNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branches", ToolTip = "Weighted branches sampled when this random node executes."))
	TArray<FDialogueRandomBranch> Branches;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledChoiceBranch
{
	GENERATED_BODY()

	FDialogueCompiledChoiceBranch()
	{
		BlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Choice Branch ID", ToolTip = "Compile-managed branch identifier used for linking and choice-memory records."))
	FGuid ChoiceBranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Choice Text", ToolTip = "Displayed text for this compiled choice branch."))
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Locked Conditions", ToolTip = "Compiled required conditions for this choice branch."))
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Blocked Conditions", ToolTip = "Compiled blocking conditions for this choice branch."))
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Important", ToolTip = "Whether this compiled choice branch is flagged important."))
	bool bImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed link target for this branch output. Set by graph compile from output pin links."))
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledSwitchBranch
{
	GENERATED_BODY()

	FDialogueCompiledSwitchBranch()
	{
		BlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branch ID", ToolTip = "Compile-managed switch branch identifier used for linking."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Label", ToolTip = "Display label for this compiled switch branch."))
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Locked Conditions", ToolTip = "Compiled required conditions for this switch branch."))
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Blocked Conditions", ToolTip = "Compiled blocking conditions for this switch branch."))
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed link target for this switch branch output."))
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledRandomBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Branch ID (Compile Managed)", ToolTip = "Compile-managed random branch identifier used for linking this output pin."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Weight", ClampMin = "0.0", UIMin = "0.0", ToolTip = "Relative weight for this output branch. With default weights (1.0 each), valid branches are selected uniformly."))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed link target selected when this random branch is chosen."))
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledSequenceBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Branch ID (Compile Managed)", ToolTip = "Compile-managed sequence branch identifier used to keep branch ordering stable."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed link target for this sequence branch output."))
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Node ID (Compile Managed)", ToolTip = "Stable node identifier used for all runtime links and validation messages."))
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Node Type", ToolTip = "Node behavior type that controls payload and available outputs."))
	EDialogueNodeType NodeType = EDialogueNodeType::Line;

	// Per-node data payload keyed by NodeType (line/choice/bool/switch/mutator/random).
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "",
		meta = (ShowOnlyInnerProperties, EditCondition = "NodeType == EDialogueNodeType::Line || NodeType == EDialogueNodeType::MultiLine || NodeType == EDialogueNodeType::Bool || NodeType == EDialogueNodeType::TagMutation || NodeType == EDialogueNodeType::RelationshipMutation || NodeType == EDialogueNodeType::FactionMutation", EditConditionHides, DisplayName = "Node Payload", ToolTip = "Type-specific payload for this node. Only shown for node types that use direct payload data."))
	FInstancedStruct NodeData;

	// Single output edge for Enter/Line/TagMutation/RelationshipMutation/FactionMutation.
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "",
		AdvancedDisplay,
		meta = (EditCondition = "NodeType == EDialogueNodeType::Enter || NodeType == EDialogueNodeType::Line || NodeType == EDialogueNodeType::MultiLine || NodeType == EDialogueNodeType::TagMutation || NodeType == EDialogueNodeType::RelationshipMutation || NodeType == EDialogueNodeType::FactionMutation || NodeType == EDialogueNodeType::Route", EditConditionHides, DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed single-output link for Enter/Line/MultiLine/Mutation/Route nodes."))
	FGuid NextNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Bool", EditConditionHides, DisplayName = "True Node ID (Compile Managed)", ToolTip = "Compile-managed output target when the Bool condition evaluates true."))
	FGuid TrueNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Bool", EditConditionHides, DisplayName = "False Node ID (Compile Managed)", ToolTip = "Compile-managed output target when the Bool condition evaluates false."))
	FGuid FalseNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Choice", EditConditionHides, DisplayName = "Choice Branches (Compile Managed)", ToolTip = "Compile-managed choice branch list used by runtime execution."))
	TArray<FDialogueCompiledChoiceBranch> ChoiceBranches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Choice", EditConditionHides, DisplayName = "Fallback Node ID (Compile Managed)", ToolTip = "Compile-managed fallback output when no authored choice can be selected."))
	FGuid FallbackNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Choice", EditConditionHides, DisplayName = "Fallback Choice Text", ToolTip = "Fallback text presented when using the fallback branch."))
	FText FallbackChoiceText = FText::FromString(TEXT("..."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Choice", EditConditionHides, DisplayName = "Choice Node Important", ToolTip = "Whether this choice node is flagged important at node level."))
	bool bChoiceNodeImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Choice", EditConditionHides, DisplayName = "Completed Choice Policy", ToolTip = "Behavior when this choice node is revisited after completion."))
	EDialogueCompletedChoicePolicy CompletedChoicePolicy = EDialogueCompletedChoicePolicy::LockedToRecordedChoice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::SwitchOnTagsByPriority", EditConditionHides, DisplayName = "Switch Branches (Compile Managed)", ToolTip = "Compile-managed switch branch list and link targets."))
	TArray<FDialogueCompiledSwitchBranch> SwitchBranches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::SwitchOnTagsByPriority", EditConditionHides, DisplayName = "Switch Has Default Output", ToolTip = "If enabled, switch uses a compile-managed default output when no branch matches."))
	bool bSwitchHasDefaultOutput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::SwitchOnTagsByPriority && bSwitchHasDefaultOutput", EditConditionHides, DisplayName = "Switch Default Node ID (Compile Managed)", ToolTip = "Compile-managed default output target for switch nodes."))
	FGuid SwitchDefaultNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (EditCondition = "NodeType == EDialogueNodeType::Random", EditConditionHides, DisplayName = "Random Branches", ToolTip = "Random output branches. Edit per-branch Weight here; link targets are compile-managed from graph pin connections."))
	TArray<FDialogueCompiledRandomBranch> RandomBranches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::Sequence", EditConditionHides, DisplayName = "Sequence Branches (Compile Managed)", ToolTip = "Ordered sequence output branches executed first-to-last. Link targets are compile-managed from graph pin connections."))
	TArray<FDialogueCompiledSequenceBranch> SequenceBranches;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledConversationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Enter Node ID (Compile Managed)", ToolTip = "Compile-managed entry node identifier for runtime execution."))
	FGuid EnterNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Compiled Nodes", ToolTip = "Compile-managed list of runtime nodes generated from the editor graph."))
	TArray<FDialogueCompiledNode> Nodes;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueLoadoutView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer LoadoutTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<UARDialogueConversationAsset> ConversationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<APlayerState> ActivePlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<APlayerController> ActivePlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<APawn> ActivePawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<APlayerState> OtherPlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<APlayerController> OtherPlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<APawn> OtherPawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<AGameStateBase> GameState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<UWorld> World = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag PrimarySpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TObjectPtr<AActor> PrimarySpeakerActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag ResolvedPlayerSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer PlayerOnlyProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer GameOnlyProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer CombinedProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer TransientConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TMap<FName, FDialogueInjectedValue> InjectedVariables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float RelationshipPointsForPrimarySpeaker = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 RelationshipLevelForPrimarySpeaker = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 PlayerKills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float TimePlayed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FDialogueLoadoutView LoadoutView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bSeenByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bSeenByGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bCompletedByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	bool bCompletedByGame = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "")
	FGuid ChoiceBranchId;

	UPROPERTY(BlueprintReadOnly, Category = "")
	FText ChoiceText;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bCanChoose = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bImportant = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueClientView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "")
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "")
	FGuid CurrentNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "")
	FGameplayTag SpeakerTag;

	UPROPERTY(BlueprintReadOnly, Category = "")
	FText LineText;

	UPROPERTY(BlueprintReadOnly, Category = "")
	FSpeakerPortraitData SpeakerPortrait;

	UPROPERTY(BlueprintReadOnly, Category = "")
	TArray<FDialogueChoiceView> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bWaitingForChoice = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bIsEavesdropping = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bConversationImportant = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	EARPlayerSlot InitiatorSlot = EARPlayerSlot::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "")
	EARPlayerSlot OwnerSlot = EARPlayerSlot::Unknown;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConversationOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "")
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "")
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bUnseenByGame = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bUnseenByPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bCatchUpCandidate = false;

	UPROPERTY(BlueprintReadOnly, Category = "")
	bool bRepeatableCandidate = false;
};

UENUM(BlueprintType)
enum class EDialogueValidationSeverity : uint8
{
	Info = 0,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EDialogueValidationSeverity Severity = EDialogueValidationSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FText Message;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueValidationReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<FDialogueValidationIssue> Issues;

	bool HasErrors() const
	{
		for (const FDialogueValidationIssue& Issue : Issues)
		{
			if (Issue.Severity == EDialogueValidationSeverity::Error)
			{
				return true;
			}
		}
		return false;
	}
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRelationshipState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float RelationshipPoints = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceMemoryRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGuid ChoiceNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGuid SelectedBranchId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialoguePlayerPersistentState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FARPlayerIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer ProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FGameplayTagContainer CompletedConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	TArray<FDialogueChoiceMemoryRecord> CompletedChoiceRecords;
};
