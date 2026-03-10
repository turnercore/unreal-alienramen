/**
 * @file ARDialogueConversationAsset.h
 * @brief Graph-authored conversation asset with compiled runtime data.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ARDialogueTypes.h"
#include "ARDialogueConversationAsset.generated.h"

class UEdGraph;

UCLASS(BlueprintType)
class ALIENRAMEN_API UARDialogueConversationAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "", meta = (ShowOnlyInnerProperties, ToolTip = "Authoring header used to select and gate this conversation at runtime."))
	FDialogueConversationHeader Header;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "", AdvancedDisplay, meta = (ToolTip = "Compile-managed runtime graph data generated from the editor graph. Do not hand-edit unless you are debugging compile output."))
	FDialogueCompiledConversationData CompiledData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "", AdvancedDisplay, meta = (ToolTip = "Most recent validation report produced by Validate/Compile/Save."))
	FDialogueValidationReport LastCompileValidation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "", AdvancedDisplay, meta = (DisplayName = "Last Compile Succeeded", ToolTip = "True when the most recent compile completed without errors."))
	bool bLastCompileSucceeded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "", AdvancedDisplay, meta = (ToolTip = "Incremented each time compile output is regenerated. Useful for tracking compile freshness and debugging stale data."))
	int32 CompileVersion = 0;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool IsCompiledGraphValid() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void ClearCompiledData();

	const FDialogueCompiledNode* FindCompiledNode(const FGuid& NodeId) const;

	FDialogueCompiledNode* FindCompiledNodeMutable(const FGuid& NodeId);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UEdGraph> EditorGraph = nullptr;
#endif
};
