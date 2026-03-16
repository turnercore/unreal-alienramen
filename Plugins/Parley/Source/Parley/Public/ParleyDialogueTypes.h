/**
 * @file ParleyDialogueTypes.h
 * @brief Shared dialogue runtime, persistence, and authoring data types for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"
#include "ParleyDialogueTypes.generated.h"

class AActor;
class APlayerController;
class APlayerState;
class APawn;
class AGameStateBase;
class UParleyConversationAsset;
class UFont;
class USoundBase;
class UTexture2D;
class UWorld;

USTRUCT(BlueprintType)
struct PARLEY_API FSpeakerPortraitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TSoftObjectPtr<UTexture2D> PortraitTexture;
};

USTRUCT(BlueprintType)
struct PARLEY_API FSpeakerPortraitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Speaker identity tag used by dialogue authoring and runtime lookups."))
	FGameplayTag PortraitTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FSpeakerPortraitData Portrait;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleySpeakerEmotionAudioEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Emotion", DisplayName = "Emotion Tag", ToolTip = "Emotion tag that maps to this speaker fallback audio entry."))
	FGameplayTag EmotionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Native Sound", ToolTip = "Optional native fallback sound for this emotion when dialogue audio mode is NativeAudio."))
	TObjectPtr<USoundBase> NativeSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.AudioCue", DisplayName = "Audio Cue Tag", ToolTip = "Optional cue tag emitted when dialogue audio mode is AudioSignals."))
	FGameplayTag AudioCueTag;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleySpeakerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Speaker identity tag used by dialogue authoring and runtime lookups."))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FText Description;

	// Optional font asset used by dialogue widgets for this speaker's spoken lines.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TSoftObjectPtr<UFont> LineFont;

	// Deprecated rich-text style tag fallback for legacy widgets. New authoring should use LineFont.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "", meta = (DeprecatedProperty, DisplayName = "Line Font Style Tag (Deprecated)", ToolTip = "Deprecated legacy style-tag fallback for line font presentation."))
	FName LineFontStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FSpeakerPortraitData DefaultPortrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TArray<FSpeakerPortraitEntry> Portraits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Emotion Audio Fallbacks", ToolTip = "Optional emotion-keyed audio fallbacks used when a line does not provide direct audio fields."))
	TArray<FParleySpeakerEmotionAudioEntry> EmotionAudioFallbacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TArray<float> RelationshipThresholds = { 50.0f, 150.0f, 300.0f, 500.0f };

	// Per-cycle interaction cap for this speaker per character state.
	// 0 means unlimited.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Max Offers Per Cycle", ToolTip = "Maximum number of times this speaker can be offered/started in one cycle for a single character state. 0 means unlimited."))
	int32 MaxOffersPerCycle = 0;
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
struct PARLEY_API FDialogueInjectedValue
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
	FactionPopularity,
	FactionSpeakerReputation,
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

UENUM(BlueprintType)
enum class EDialogueActiveCharacterRestriction : uint8
{
	Any = 0,
	BrotherOnly,
	SisterOnly
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Where this condition reads its value from (tags, runtime stats, booleans, loadout, or injected variables)."))
	EDialogueConditionSource Source = EDialogueConditionSource::CombinedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Comparison", ToolTip = "Comparison operation applied to the selected source."))
	EDialogueComparisonOp Operator = EDialogueComparisonOp::Present;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Tag Value", ToolTip = "Tag operand for tag-based comparisons (Contains/Present/Equals, etc)."))
	FGameplayTag TagValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Secondary Tag Value", ToolTip = "Optional secondary gameplay tag operand used by condition sources that require two tag identifiers."))
	FGameplayTag SecondaryTagValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Numeric Value", ToolTip = "Numeric operand used for numeric comparisons (relationship points, kills, time played, etc)."))
	float NumericValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Variable Name", ToolTip = "Injected variable key used when Source is InjectedVariable."))
	FName VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Injected Value", ToolTip = "Injected variable value compared against at runtime when Source is InjectedVariable."))
	FDialogueInjectedValue InjectedValue;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueConditionGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Match Mode", ToolTip = "How conditions in this group are evaluated: All means every condition must pass, Any means one passing condition is enough."))
	EDialogueConditionMatchMode MatchMode = EDialogueConditionMatchMode::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Conditions evaluated together using Match Mode."))
	TArray<FDialogueCondition> Conditions;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueConversationHeader
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Primary Speaker Tag", ToolTip = "Primary speaker for gating and runtime context resolution."))
	FGameplayTag PrimarySpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Participating Speaker Tags", ToolTip = "Additional speakers that may participate in this conversation."))
	TArray<FGameplayTag> ParticipatingSpeakerTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Offer-selection priority. Higher values are preferred when multiple conversations are eligible."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Offer Weight", ClampMin = "1", UIMin = "1", ToolTip = "Weighted tie-breaker used when multiple eligible conversations have the same effective priority."))
	int32 OfferWeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Chance Offered", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Chance roll applied during offer selection. 1.0 always offers, 0.0 always skips."))
	float ChanceOffered = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Block Offer Per Cycle", ToolTip = "When true, once this conversation is seen or skipped for a player in the current cycle it will not be offered again until temporary cycle state is cleared."))
	bool bBlockOfferPerCycle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Character Restriction", ToolTip = "Convenience filter for active player character. Use Any to allow all characters."))
	EDialogueActiveCharacterRestriction CharacterRestriction = EDialogueActiveCharacterRestriction::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Repeatable", ToolTip = "If enabled, this conversation may be offered again after completion when other gates allow it."))
	bool bRepeatable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Important", ToolTip = "Marks this conversation as important for systems that treat priority conversations differently."))
	bool bImportant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Private Conversation", ToolTip = "When enabled, this conversation rejects eavesdrop while active. Important choice moments can still override this and force all viewers."))
	bool bPrivateConversation = false;

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

// TagKey row shape for conversation asset registration.
USTRUCT(BlueprintType)
struct PARLEY_API FParleyConversationAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	// Authoritative conversation tag. If left unset, the loader will try to build one from the conversation root + row name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Conversation Tag", ToolTip = "Conversation tag served by this registry row. If unset, systems may derive a tag from root+row name."))
	FGameplayTag ConversationTag;

	// Reference to the authored conversation asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Conversation Asset", ToolTip = "Conversation asset resolved for this row."))
	TSoftObjectPtr<UParleyConversationAsset> Conversation;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueConversationLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Line ID", ToolTip = "Stable per-line guid used for identity and downstream tracking. Auto-generated for new lines."))
	FGuid LocalLineGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Text", ToolTip = "Line text shown to the player when this node executes."))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Sound", ToolTip = "Optional voice/audio clip played with this line."))
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.AudioCue", DisplayName = "Audio Cue Tag", ToolTip = "Optional cue tag emitted for this line when dialogue audio mode is AudioSignals."))
	FGameplayTag AudioCueTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Speaker Tag", ToolTip = "Speaker used when presenting this line. Optional leaf segments can encode presentation emotion (for example Parley.Speaker.Fred.Angry). Leave empty to fall back to conversation context."))
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
	MultiLine,
	SplitLine,
	RouteByCharacter,
	Signal
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueLineNodeData
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Character Restriction", ToolTip = "Convenience skip filter for active player character. Use Any to allow all characters."))
	EDialogueActiveCharacterRestriction CharacterRestriction = EDialogueActiveCharacterRestriction::Any;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueMultiLineEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Entry ID (Compile Managed)", ToolTip = "Stable per-entry identifier used by editor tooling for ordering and editing."))
	FGuid EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ShowOnlyInnerProperties, DisplayName = "Line", ToolTip = "Line payload and skip conditions for this step in the multiline sequence."))
	FDialogueLineNodeData LineData;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueMultiLineNodeData
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
struct PARLEY_API FDialogueChoiceOption
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
struct PARLEY_API FDialogueChoiceNodeData
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
struct PARLEY_API FDialogueBoolNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ShowOnlyInnerProperties, DisplayName = "Condition", ToolTip = "Condition evaluated by this Bool node. True/False outputs are chosen from this result."))
	FDialogueCondition Condition;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueSwitchBranch
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
struct PARLEY_API FDialogueSwitchNodeData
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
struct PARLEY_API FDialogueTagMutation
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
struct PARLEY_API FDialogueTagMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Mutations", ToolTip = "Tag mutations executed when this node runs."))
	TArray<FDialogueTagMutation> Mutations;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueRelationshipMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Source Speaker Tag", ToolTip = "Optional relationship source speaker override. When unset, runtime uses the active source speaker from dialogue context."))
	FGameplayTag SourceSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Target Speaker Tag", ToolTip = "Speaker whose relationship points are modified."))
	FGameplayTag TargetSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Delta Points", ToolTip = "Signed amount added to relationship points for the target speaker."))
	float DeltaPoints = 0.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueFactionMutationNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Faction Tag", ToolTip = "Faction to modify."))
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Delta Popularity", ToolTip = "Signed amount added to faction popularity."))
	float DeltaPopularity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Target Speaker Tag", ToolTip = "Optional speaker tag to mutate faction reputation for. If unset and Delta Speaker Reputation is non-zero, runtime falls back to primary speaker in context."))
	FGameplayTag TargetSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Delta Speaker Reputation", ToolTip = "Signed amount added to faction reputation for Target Speaker Tag."))
	float DeltaSpeakerReputation = 0.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueSignalNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Dialogue.Signal", ToolTip = "Gameplay tag identifying this signal. Game systems bind to OnDialogueSignalFired and filter by this tag."))
	FGameplayTag SignalTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Optional payload tags broadcast with the signal for additional context."))
	FGameplayTagContainer PayloadTags;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueRandomBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branch ID", ToolTip = "Stable identifier for this random branch. Auto-generated when needed."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Weight", ToolTip = "Relative random weight. Higher weights are selected more often."))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueRandomNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Branches", ToolTip = "Weighted branches sampled when this random node executes."))
	TArray<FDialogueRandomBranch> Branches;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueCompiledChoiceBranch
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
struct PARLEY_API FDialogueCompiledSwitchBranch
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
struct PARLEY_API FDialogueCompiledRandomBranch
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
struct PARLEY_API FDialogueCompiledSequenceBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Branch ID (Compile Managed)", ToolTip = "Compile-managed sequence branch identifier used to keep branch ordering stable."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed link target for this sequence branch output."))
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueCompiledCharacterRouteBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Branch ID (Compile Managed)", ToolTip = "Compile-managed branch identifier used to keep character route branches stable."))
	FGuid BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", DisplayName = "Speaker Tag", ToolTip = "Player speaker tag this branch matches. The first matching branch routes."))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed link target for this character route branch output."))
	FGuid NextNodeId;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueCompiledNode
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
		meta = (ShowOnlyInnerProperties, EditCondition = "NodeType == EDialogueNodeType::Line || NodeType == EDialogueNodeType::MultiLine || NodeType == EDialogueNodeType::SplitLine || NodeType == EDialogueNodeType::Bool || NodeType == EDialogueNodeType::TagMutation || NodeType == EDialogueNodeType::RelationshipMutation || NodeType == EDialogueNodeType::FactionMutation || NodeType == EDialogueNodeType::Signal", EditConditionHides, DisplayName = "Node Payload", ToolTip = "Type-specific payload for this node. Only shown for node types that use direct payload data."))
	FInstancedStruct NodeData;

	// Single output edge for Enter/Line/TagMutation/RelationshipMutation/FactionMutation/Signal.
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "",
		AdvancedDisplay,
		meta = (EditCondition = "NodeType == EDialogueNodeType::Enter || NodeType == EDialogueNodeType::Line || NodeType == EDialogueNodeType::MultiLine || NodeType == EDialogueNodeType::SplitLine || NodeType == EDialogueNodeType::TagMutation || NodeType == EDialogueNodeType::RelationshipMutation || NodeType == EDialogueNodeType::FactionMutation || NodeType == EDialogueNodeType::Signal || NodeType == EDialogueNodeType::Route", EditConditionHides, DisplayName = "Next Node ID (Compile Managed)", ToolTip = "Compile-managed single-output link for Enter/Line/MultiLine/SplitLine/Mutation/Signal/Route nodes."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", AdvancedDisplay, meta = (EditCondition = "NodeType == EDialogueNodeType::RouteByCharacter", EditConditionHides, DisplayName = "Character Route Branches (Compile Managed)", ToolTip = "Priority-ordered player character branches. First matching speaker tag routes to its linked output."))
	TArray<FDialogueCompiledCharacterRouteBranch> CharacterRouteBranches;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueCompiledConversationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Enter Node ID (Compile Managed)", ToolTip = "Compile-managed entry node identifier for runtime execution."))
	FGuid EnterNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (DisplayName = "Compiled Nodes", ToolTip = "Compile-managed list of runtime nodes generated from the editor graph."))
	TArray<FDialogueCompiledNode> Nodes;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueLoadoutView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer LoadoutTags;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<UParleyConversationAsset> ConversationAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<APlayerState> ActivePlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<APlayerController> ActivePlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<APawn> ActivePawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<APlayerState> OtherPlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<APlayerController> OtherPlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<APawn> OtherPawn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<AGameStateBase> GameState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<UWorld> World = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTag PrimarySpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TObjectPtr<AActor> PrimarySpeakerActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTag ResolvedPlayerSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Resolved source speaker for relationship-style runtime evaluation."))
	FGameplayTag SourceSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer PlayerOnlyProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer GameOnlyProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer CombinedProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer TransientConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TMap<FName, FDialogueInjectedValue> InjectedVariables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	float RelationshipPointsForPrimarySpeaker = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	int32 RelationshipLevelForPrimarySpeaker = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	int32 PlayerKills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	float TimePlayed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FDialogueLoadoutView LoadoutView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	bool bSeenByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	bool bSeenByGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	bool bCompletedByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	bool bCompletedByGame = false;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FGuid ChoiceBranchId;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FText ChoiceText;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bCanChoose = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bImportant = false;
};

UENUM(BlueprintType)
enum class EDialogueAudioRequestSource : uint8
{
	None = 0,
	Line,
	EmotionFallback
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueAudioRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Dialogue session identifier for this audio request."))
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Stable line guid for dedupe and listener routing."))
	FGuid LineGuid;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Conversation tag context for this audio request."))
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Resolved speaker tag context for this audio request."))
	FGameplayTag SpeakerTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Target listener character tag for this local audio request."))
	FGameplayTag ListenerCharacterTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Whether this request came from direct line audio or speaker-emotion fallback."))
	EDialogueAudioRequestSource Source = EDialogueAudioRequestSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Resolved native sound payload for NativeAudio mode requests."))
	TObjectPtr<USoundBase> NativeSound = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Resolved cue tag payload for AudioSignals mode requests."))
	FGameplayTag AudioCueTag;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueClientView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FGuid CurrentNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FGameplayTag SpeakerTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FName SpeakerLineFontStyleTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	TSoftObjectPtr<UFont> SpeakerLineFont;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FText LineText;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FSpeakerPortraitData SpeakerPortrait;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	TArray<FDialogueChoiceView> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bWaitingForChoice = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bIsEavesdropping = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bConversationImportant = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Canonical initiating character tag for this session view."))
	FGameplayTag InitiatorCharacterTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Canonical owning character tag for this session view."))
	FGameplayTag OwnerCharacterTag;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueConversationOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	FGameplayTag ConversationTag;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bUnseenByGame = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bUnseenByPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
	bool bCatchUpCandidate = false;

	UPROPERTY(BlueprintReadOnly, Category = "", meta = (ToolTip = "Blueprint read-only Parley runtime output field."))
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
struct PARLEY_API FDialogueValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	EDialogueValidationSeverity Severity = EDialogueValidationSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FText Message;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueValidationReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
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
struct PARLEY_API FDialogueSpeakerRelationshipState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Source speaker identity for this directed relationship edge."))
	FGameplayTag SourceSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Target speaker identity for this directed relationship edge."))
	FGameplayTag TargetSpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Current relationship points stored for this source-target edge."))
	float RelationshipPoints = 0.0f;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueChoiceMemoryRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTag ConversationTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGuid ChoiceNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGuid SelectedBranchId;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialogueSpeakerCycleOfferCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Non-negative numeric value used by dialogue runtime progression data."))
	int32 OfferCount = 0;
};

USTRUCT(BlueprintType)
struct PARLEY_API FDialoguePlayerPersistentState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Owning character tag for this progression state."))
	FGameplayTag OwnerCharacterTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Canonical speaker/character tag that owns this progression state."))
	FGameplayTag CharacterTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer ProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer CompletedConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TArray<FDialogueChoiceMemoryRecord> CompletedChoiceRecords;

	// Character-owned temporary cycle blockers for dialogue offer suppression.
	// These persist in save until explicitly cleared by subsystem API.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer SeenConversationTagsThisCycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	FGameplayTagContainer SkippedConversationTagsThisCycle;

	// Per-speaker offer/start counters for the current cycle (character-owned state).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Blueprint-exposed Parley data field used by runtime or authoring tools."))
	TArray<FDialogueSpeakerCycleOfferCount> SpeakerOfferCountsThisCycle;
};

USTRUCT(BlueprintType)
struct PARLEY_API FParleyProgressionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Persistent progression tags owned by this player/character dialogue state."))
	FGameplayTagContainer ProgressionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Conversations completed for this player/character dialogue state."))
	FGameplayTagContainer CompletedConversationTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Recorded choice selections for completed conversations."))
	TArray<FDialogueChoiceMemoryRecord> CompletedChoiceRecords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Transient cycle blocker tags marked as seen in the current cycle."))
	FGameplayTagContainer SeenConversationTagsThisCycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Transient cycle blocker tags marked as skipped in the current cycle."))
	FGameplayTagContainer SkippedConversationTagsThisCycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ToolTip = "Per-speaker offer/start counters for the current cycle."))
	TArray<FDialogueSpeakerCycleOfferCount> SpeakerOfferCountsThisCycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (Categories = "Parley.Speaker", ToolTip = "Canonical speaker/character tag for this progression state."))
	FGameplayTag CharacterTag;
};
