/**
 * @file ARDialogueSubsystem.h
 * @brief Server-authoritative compiled-graph dialogue runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARDialogueTypes.h"
#include "ARDialogueSubsystem.generated.h"

class AARPlayerController;
class UARDialogueConversationAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnDialogueSessionUpdated, const FDialogueClientView&, View);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnDialogueSessionEnded, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnConversationCompletedSignature, FGameplayTag, ConversationTag);

UCLASS()
class ALIENRAMEN_API UARDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Required runtime API contracts ----

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool GetAvailableConversationForNPC(AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag, FDialogueConversationOffer& OutOffer, bool bNpcLocalStateAllowsDialogue = true);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool StartConversation(AARPlayerController* RequestingController, FGameplayTag ConversationTag, FGameplayTag PrimarySpeakerTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool AdvanceConversation(AARPlayerController* RequestingController);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SubmitChoice(AARPlayerController* RequestingController, FGuid ChoiceBranchId);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ForceEavesdrop(AARPlayerController* RequestingController, bool bEnable, EARPlayerSlot TargetSlot);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	FGameplayTagContainer GetCombinedDialogueTags(const FGameplayTagContainer& PlayerOnlyProgressionTags, const FGameplayTagContainer& GameOnlyProgressionTags) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool EvaluateDialogueCondition(const FDialogueCondition& Condition, const FDialogueRuntimeContext& Context) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyDialogueTagMutation(const FDialogueTagMutation& Mutation, const FDialogueRuntimeContext& Context);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyDialogueRelationshipMutation(const FDialogueRelationshipMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ApplyDialogueFactionMutation(const FDialogueFactionMutationNodeData& Mutation, const FDialogueRuntimeContext& Context);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ValidateConversation(UARDialogueConversationAsset* ConversationAsset, FDialogueValidationReport& OutReport) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool ValidateSpeaker(const FARDialogueSpeakerRow& SpeakerRow, FDialogueValidationReport& OutReport) const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool PreviewConversation(UARDialogueConversationAsset* ConversationAsset, const FDialogueRuntimeContext& PreviewContext, FDialogueClientView& OutFirstView, FDialogueValidationReport& OutReport) const;

	// Editor/tooling preview runner: simulates a full conversation trace with auto-advance and auto-choice routing.
	bool PreviewConversationTrace(
		UARDialogueConversationAsset* ConversationAsset,
		const FDialogueRuntimeContext& PreviewContext,
		int32 MaxInteractiveSteps,
		TArray<FDialogueClientView>& OutViews,
		TArray<FGuid>& OutAutoSelectedChoiceBranchIds,
		bool& bOutEndedCompleted,
		FDialogueValidationReport& OutReport) const;

	// ---- Compatibility wrappers used by gameplay code ----

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool TryStartDialogueWithNpc(AARPlayerController* RequestingController, FGameplayTag PrimarySpeakerTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SubmitDialogueChoice(AARPlayerController* RequestingController, FGuid ChoiceBranchId)
	{
		return SubmitChoice(RequestingController, ChoiceBranchId);
	}

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SetShopEavesdropTarget(AARPlayerController* RequestingController, EARPlayerSlot TargetSlot, bool bEnable)
	{
		return ForceEavesdrop(RequestingController, bEnable, TargetSlot);
	}

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasUnlockedDialogueForNpcForSlot(FGameplayTag PrimarySpeakerTag, EARPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasUnlockedDialogueForNpcForAnyPlayer(FGameplayTag PrimarySpeakerTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool GetLocalViewForController(const AARPlayerController* RequestingController, FDialogueClientView& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasActiveDialogueSession() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	float GetRelationshipPointsForSpeaker(FGameplayTag SpeakerTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	int32 GetRelationshipLevelForSpeaker(FGameplayTag SpeakerTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionUpdated OnDialogueSessionUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionEnded OnDialogueSessionEnded;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnConversationCompletedSignature OnConversationCompleted;

private:
	struct FARDialogueRuntimeState;

	FARDialogueRuntimeState& GetRuntimeState();
	const FARDialogueRuntimeState& GetRuntimeState() const;

	mutable FARDialogueRuntimeState* RuntimeState = nullptr;
};
