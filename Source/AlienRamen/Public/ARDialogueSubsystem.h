/**
 * @file ARDialogueSubsystem.h
 * @brief Server-authoritative dialogue runtime for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARDialogueTypes.h"
#include "ARDialogueSubsystem.generated.h"

class AARPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnDialogueSessionUpdated, const FARDialogueClientView&, View);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnDialogueSessionEnded, const FString&, SessionId);

UCLASS()
class ALIENRAMEN_API UARDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool TryStartDialogueWithNpc(AARPlayerController* RequestingController, FGameplayTag NpcTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool AdvanceDialogue(AARPlayerController* RequestingController);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SubmitDialogueChoice(AARPlayerController* RequestingController, FGameplayTag ChoiceTag);

	// Shop-only helper: mirror a partner's dialogue session as a co-pilot viewer.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	bool SetShopEavesdropTarget(AARPlayerController* RequestingController, EARPlayerSlot TargetSlot, bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasUnlockedDialogueForNpcForSlot(FGameplayTag NpcTag, EARPlayerSlot PlayerSlot) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool HasUnlockedDialogueForNpcForAnyPlayer(FGameplayTag NpcTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool GetLocalViewForController(const AARPlayerController* RequestingController, FARDialogueClientView& OutView) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionUpdated OnDialogueSessionUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueSessionEnded OnDialogueSessionEnded;

	struct FARActiveDialogueSession
	{
		FString SessionId;
		FGameplayTag NpcTag;
		FGameplayTag CurrentNodeTag;
		EARPlayerSlot InitiatorSlot = EARPlayerSlot::Unknown;
		EARPlayerSlot OwnerSlot = EARPlayerSlot::Unknown;
		bool bIsSharedSession = false;
		bool bWaitingForChoice = false;
		EARDialogueChoiceParticipation ChoiceParticipation = EARDialogueChoiceParticipation::InitiatorOnly;
		bool bForceEavesdropForImportantDecision = false;
		TArray<FARDialogueChoiceDef> CurrentChoices;
		TMap<EARPlayerSlot, FGameplayTag> ChoiceVotes;
		TSet<EARPlayerSlot> Participants;
		FARDialogueNodeRow ActiveRow;
	};

private:
	TArray<FARActiveDialogueSession> ActiveSessions;

	TMap<EARPlayerSlot, EARPlayerSlot> ShopEavesdropTargetByViewer;
};
