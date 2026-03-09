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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueConversationHeader Header;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueCompiledConversationData CompiledData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueValidationReport LastCompileValidation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
	bool bLastCompileSucceeded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
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
