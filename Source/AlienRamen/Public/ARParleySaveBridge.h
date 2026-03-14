#pragma once

#include "CoreMinimal.h"
#include "ARPlayerTypes.h"
#include "UObject/Object.h"
#include "ParleyFactionTypes.h"
#include "ARParleySaveBridge.generated.h"

class UARSaveSubsystem;
class UARSaveGame;
class UParleyDialogueSubsystem;
class UParleyFactionSubsystem;

UCLASS()
class ALIENRAMEN_API UARParleySaveBridge : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UARSaveSubsystem* InSaveSubsystem, UParleyDialogueSubsystem* InParleySubsystem, UParleyFactionSubsystem* InFactionSubsystem);
	void Shutdown();

	UFUNCTION()
	void HandleSaveLoaded();

	UFUNCTION()
	void HandleConversationCompleted(FGameplayTag ConversationTag, FGameplayTag PlayerSlotTag, FGameplayTag CharacterTag);

	UFUNCTION()
	void HandleRelationshipChanged(FGameplayTag SpeakerTag, FGameplayTag PlayerSlotTag, float Delta, float NewTotal);

	UFUNCTION()
	void HandleProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag PlayerSlotTag);

	UFUNCTION()
	void HandleFactionPopularityChanged(FGameplayTag FactionTag, float Delta, float NewTotal);

	UFUNCTION()
	void HandleFactionSpeakerReputationChanged(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float Delta, float NewTotal);

	bool IsConversationCompletedForPlayer(FGameplayTag ConversationTag, FGameplayTag PlayerSlotTag) const;
	FGameplayTag ResolveCurrentModeTag() const;

private:
	void InjectAllFromCurrentSave();
	UARSaveGame* GetCurrentSave() const;
	FGameplayTag ResolveCharacterTagForSlot(UARSaveGame* SaveGame, EARPlayerSlot Slot) const;

	UPROPERTY(Transient)
	TObjectPtr<UARSaveSubsystem> SaveSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UParleyDialogueSubsystem> ParleySubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UParleyFactionSubsystem> FactionSubsystem;
};
