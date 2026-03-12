/**
 * @file ARSaveGame.h
 * @brief ARSaveGame header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARDialogueTypes.h"
#include "ARFactionTypes.h"
#include "ARRunBuffTypes.h"
#include "ARSaveTypes.h"
#include "ARSaveGame.generated.h"

/** Canonical save payload persisted to disk; schema is versioned manually via CurrentSchemaVersion. */
UCLASS(BlueprintType)
class ALIENRAMEN_API UARSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 8;
	static constexpr int32 MinSupportedSchemaVersion = 7;

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

	// Stored inventory of extracted energy drinks (counted, save-persistent).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run Buff")
	TArray<FARRunBuffItemStack> StoredEnergyDrinkStacks;

	// Queued energy drinks to be rotated into active buffs at next invader init.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run Buff")
	TArray<FARRunBuffItemStack> QueuedEnergyDrinkStacks;

	// Active run-buff payload currently applied for this run cycle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run Buff")
	TArray<FARRunBuffActivePayload> ActiveRunBuffPayloads;

	// Monotonic cycle marker incremented each time queued drinks rotate into active payload.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run Buff")
	int32 ActiveRunBuffCycleId = 0;

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

	// Global speaker relationship points.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	TArray<FDialogueRelationshipState> DialogueRelationshipStates;

	// Persistent game-scope completed conversations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	FGameplayTagContainer DialogueCompletedConversationTagsByGame;

	// Per-player dialogue progression/completion/choice-memory state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	TArray<FDialoguePlayerPersistentState> DialoguePlayerPersistentStates;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataBySlot(EARPlayerSlot Slot, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	int32 ValidateAndSanitize(TArray<FString>* OutWarnings);
};
