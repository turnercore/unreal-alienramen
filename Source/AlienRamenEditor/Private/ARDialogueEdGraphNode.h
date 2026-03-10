#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "ARDialogueTypes.h"
#include "ARDialogueEdGraphNode.generated.h"

class UGraphNodeContextMenuContext;
class UToolMenu;

UCLASS(CollapseCategories)
class UARDialogueEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
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

	void InitializeForNodeType(EDialogueNodeType NodeType);
	void EnsureStableIds(bool bRegenerateNodeId, bool bRegenerateBranchIds);
	void ClearRuntimeLinks();

	UEdGraphPin* GetExecInputPin() const;
	UEdGraphPin* GetOutputPinByName(const FName PinName) const;
	UEdGraphPin* GetChoiceOutputPin(const FGuid& ChoiceBranchId) const;
	UEdGraphPin* GetSwitchOutputPin(const FGuid& BranchId) const;
	UEdGraphPin* GetRandomOutputPin(const FGuid& BranchId) const;
	UEdGraphPin* GetSequenceOutputPin(const FGuid& BranchId) const;
	UEdGraphPin* GetCharacterRouteOutputPin(const FGuid& BranchId) const;

	bool SupportsDynamicBranchPins() const;
	void AddDynamicBranchPin();
	bool RemoveLastDynamicBranchPin();
	bool RemoveDynamicBranchPinByName(FName PinName);

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

	static FName MakeChoicePinName(const FGuid& ChoiceBranchId);
	static FName MakeSwitchPinName(const FGuid& BranchId);
	static FName MakeRandomPinName(const FGuid& BranchId);
	static FName MakeSequencePinName(const FGuid& BranchId);
	static FName MakeCharacterRoutePinName(const FGuid& BranchId);

private:
	bool CommitRuntimeNodeMutation(const FText& TransactionText, TFunctionRef<bool()> MutateFn, bool bReconstructPins);
	static bool TryParseBranchGuidFromPinName(const FName PinName, const FString& Prefix, FGuid& OutBranchId);

	void EnsureNodeDataMatchesNodeType();
	void EnsureBranchAndLineIds(bool bRegenerateBranches, bool bRegenerateLineGuid);
	void AddInputPinIfNeeded();
	void AddNextOutputPinIfNeeded();
	void AddChoicePins();
	void AddSwitchPins();
	void AddRandomPins();
	void AddSequencePins();
	void AddCharacterRoutePins();
	FString BuildInlineSummary() const;

	EDialogueValidationSeverity ValidationSeverity = EDialogueValidationSeverity::Info;
	FString ValidationMessage;
};
