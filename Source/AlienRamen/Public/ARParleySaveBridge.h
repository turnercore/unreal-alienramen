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
	void HandleConversationCompleted(FGameplayTag ConversationTag, FGameplayTag OwnerCharacterTag, FGameplayTag CharacterTag);

	UFUNCTION()
	void HandleSpeakerRelationshipChanged(FGameplayTag SourceSpeakerTag, FGameplayTag TargetSpeakerTag, FGameplayTag OwnerCharacterTag, float Delta, float NewTotal);

	UFUNCTION()
	void HandleProgressionTagMutated(FGameplayTag ProgressionTag, bool bAdded, FGameplayTag OwnerCharacterTag);

	UFUNCTION()
	void HandleFactionPopularityChanged(FGameplayTag FactionTag, float Delta, float NewTotal);

	UFUNCTION()
	void HandleFactionSpeakerReputationChanged(FGameplayTag FactionTag, FGameplayTag SpeakerTag, float Delta, float NewTotal);

	bool IsConversationCompletedForCharacter(FGameplayTag ConversationTag, FGameplayTag CharacterTag) const;
	FGameplayTag ResolveCurrentModeTag() const;

private:
	void InjectAllFromCurrentSave();
	UARSaveGame* GetCurrentSave() const;
	UPROPERTY(Transient)
	TObjectPtr<UARSaveSubsystem> SaveSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UParleyDialogueSubsystem> ParleySubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UParleyFactionSubsystem> FactionSubsystem;
};
