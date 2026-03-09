#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "ARDialogueTypes.h"
#include "ARDialogueEdGraphNode.generated.h"

UCLASS()
class UARDialogueEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	FDialogueCompiledNode RuntimeNode;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
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

private:
	void EnsureNodeDataMatchesNodeType();
	void EnsureBranchAndLineIds(bool bRegenerateBranches, bool bRegenerateLineGuid);
	void AddInputPinIfNeeded();
	void AddNextOutputPinIfNeeded();
	void AddChoicePins();
	void AddSwitchPins();
	void AddRandomPins();
	FString BuildInlineSummary() const;

	EDialogueValidationSeverity ValidationSeverity = EDialogueValidationSeverity::Info;
	FString ValidationMessage;
};
