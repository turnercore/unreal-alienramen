/**
 * @file ARSaveGame.h
 * @brief ARSaveGame header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARDialogueTypes.h"
#include "ARFactionTypes.h"
#include "ARSaveTypes.h"
#include "ARSaveGame.generated.h"

/** Canonical save payload persisted to disk; schema is versioned manually via CurrentSchemaVersion. */
UCLASS(BlueprintType)
class ALIENRAMEN_API UARSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 6;
	static constexpr int32 MinSupportedSchemaVersion = 6;

	UARSaveGame();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static int32 GetCurrentSchemaVersion() { return CurrentSchemaVersion; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static int32 GetMinSupportedSchemaVersion() { return MinSupportedSchemaVersion; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static bool IsSchemaVersionSupported(int32 InSchemaVersion)
	{
		return InSchemaVersion >= MinSupportedSchemaVersion && InSchemaVersion <= CurrentSchemaVersion;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 Scrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 Cycles = 0;

	// Persistent long-term modifiers and progression state (non-unlock progression).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FGameplayTagContainer ProgressionTags;

	// Persistent clout progression controlling number of faction vote candidates.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 FactionClout = 0;

	// Current elected faction identity and active effect tags.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FGameplayTag ActiveFactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FGameplayTagContainer ActiveFactionEffectTags;

	// Persistent background popularity state for faction ranking/drift.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	TArray<FARFactionRuntimeState> FactionPopularityStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	FName SaveSlot = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveGameVersion = CurrentSchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveSlotNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	FDateTime LastSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FARPlayerStateSaveData> PlayerStates;

	// Global per-NPC relationship state and wants.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|NPC")
	TArray<FARNpcRelationshipState> NpcRelationshipStates;

	// Canonical branch outcomes for choice nodes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	TArray<FARDialogueCanonicalChoiceState> DialogueCanonicalChoiceStates;

	// Per-player node seen history.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	TArray<FARPlayerDialogueHistoryState> PlayerDialogueHistoryStates;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataBySlot(EARPlayerSlot Slot, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	int32 ValidateAndSanitize(TArray<FString>* OutWarnings);
};
