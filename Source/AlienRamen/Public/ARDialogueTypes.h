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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSoftObjectPtr<UTexture2D> PortraitTexture;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FSpeakerPortraitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag PortraitTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FSpeakerPortraitData Portrait;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueSpeakerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FSpeakerPortraitData DefaultPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FSpeakerPortraitEntry> Portraits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueInjectedValueType ValueType = EDialogueInjectedValueType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag TagValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueConditionSource Source = EDialogueConditionSource::CombinedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Present;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag TagValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float NumericValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueInjectedValue InjectedValue;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConditionGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueConditionMatchMode MatchMode = EDialogueConditionMatchMode::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DisplayTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag PrimarySpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (Categories = "Dialogue.Speaker"))
	TArray<FGameplayTag> ParticipatingSpeakerTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bRepeatable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float MinimumRelationshipPoints = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bSeenByGameBlocksReoffer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bSeenByPlayerBlocksReoffer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bCompletedByGameBlocksReoffer = false;
};

// ContentLookup row shape for conversation asset registration.
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARDialogueConversationAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	// Authoritative conversation tag. If left unset, the loader will try to build one from the conversation root + row name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ConversationTag;

	// Reference to the authored conversation asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSoftObjectPtr<UARDialogueConversationAsset> Conversation;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConversationLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid LocalLineGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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
	Random
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueLineNodeData
{
	GENERATED_BODY()

	FDialogueLineNodeData()
	{
		SkipBlockedConditions.MatchMode = EDialogueConditionMatchMode::Any;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConversationLine Line;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bAutoAdvance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup SkipLockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup SkipBlockedConditions;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid ChoiceBranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bImportant = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueChoiceOption> Choices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText FallbackChoiceText = FText::FromString(TEXT("..."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bNodeImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueCompletedChoicePolicy CompletedChoicePolicy = EDialogueCompletedChoicePolicy::LockedToRecordedChoice;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueBoolNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup BlockedConditions;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueSwitchNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueSwitchBranch> Branches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueTagMutationTarget Target = EDialogueTagMutationTarget::GameStateProgression;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueTagMutationOp Operation = EDialogueTagMutationOp::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag Tag;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueTagMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueTagMutation> Mutations;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRelationshipMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (Categories = "Dialogue.Speaker"))
	FGameplayTag TargetSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float DeltaPoints = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueFactionMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float DeltaPopularity = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRandomBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRandomNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid ChoiceBranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup LockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueConditionGroup BlockedConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledRandomBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueNodeType NodeType = EDialogueNodeType::Line;

	// Per-node data payload keyed by NodeType (line/choice/bool/switch/mutator/random).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FInstancedStruct NodeData;

	// Single output edge for Enter/Line/TagMutation/RelationshipMutation/FactionMutation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid NextNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid TrueNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid FalseNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueCompiledChoiceBranch> ChoiceBranches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid FallbackNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText FallbackChoiceText = FText::FromString(TEXT("..."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bChoiceNodeImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueCompletedChoicePolicy CompletedChoicePolicy = EDialogueCompletedChoicePolicy::LockedToRecordedChoice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueCompiledSwitchBranch> SwitchBranches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bSwitchHasDefaultOutput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid SwitchDefaultNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueCompiledRandomBranch> RandomBranches;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueCompiledConversationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid EnterNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueCompiledNode> Nodes;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueLoadoutView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer LoadoutTags;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UARDialogueConversationAsset> ConversationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<APlayerState> ActivePlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<APlayerController> ActivePlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<APawn> ActivePawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<APlayerState> OtherPlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<APlayerController> OtherPlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<APawn> OtherPawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<AGameStateBase> GameState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UWorld> World = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag PrimarySpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<AActor> PrimarySpeakerActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ResolvedPlayerSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer PlayerOnlyProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer GameOnlyProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer CombinedProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer TransientConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TMap<FName, FDialogueInjectedValue> InjectedVariables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float RelationshipPointsForPrimarySpeaker = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 RelationshipLevelForPrimarySpeaker = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 PlayerKills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float TimePlayed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueLoadoutView LoadoutView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bSeenByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bSeenByGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bCompletedByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bCompletedByGame = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGuid ChoiceBranchId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bCanChoose = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bImportant = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueClientView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGuid CurrentNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag SpeakerTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText LineText;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FSpeakerPortraitData SpeakerPortrait;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueChoiceView> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bWaitingForChoice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bIsEavesdropping = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bConversationImportant = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EARPlayerSlot InitiatorSlot = EARPlayerSlot::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EARPlayerSlot OwnerSlot = EARPlayerSlot::Unknown;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueConversationOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bUnseenByGame = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bUnseenByPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bCatchUpCandidate = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueValidationSeverity Severity = EDialogueValidationSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Message;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueValidationReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float RelationshipPoints = 0.0f;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialogueChoiceMemoryRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid ChoiceNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGuid SelectedBranchId;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FDialoguePlayerPersistentState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FARPlayerIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer ProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTagContainer CompletedConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueChoiceMemoryRecord> CompletedChoiceRecords;
};
