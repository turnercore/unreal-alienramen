/**
 * @file ARSaveGame.h
 * @brief ARSaveGame header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ParleyDialogueTypes.h"
#include "ParleyFactionTypes.h"
#include "ARRunBuffTypes.h"
#include "ARSaveTypes.h"
#include "ARSaveGame.generated.h"

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCharacterSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Canonical gameplay-tag identity for the character this save row belongs to."))
	FGameplayTag CharacterTag;

	// Character-owned dialogue progression/completion/choice-memory state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue", meta = (ToolTip = "Persistent dialogue progression, completion, and choice memory that belongs to this character."))
	FDialoguePlayerPersistentState DialogueState;

	// Canonical character-owned loadout state keyed by CharacterTag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Character", meta = (ToolTip = "Canonical loadout state owned by this character and reused when any player takes control of the same character."))
	FGameplayTagContainer LoadoutTags;

	// Character-owned core attribute values (health/spice/speed/strength) captured from runtime ASC state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Character")
	FARCharacterRuntimeCoreAttributeSaveData CoreAttributes;

	// Character-owned life state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Character")
	bool bIsDowned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Character")
	bool bIsDeadState = false;

	// Character-owned invader runtime state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Character|Invader")
	FARCharacterInvaderRuntimeSaveData InvaderRuntime;

	// Shop-only world snapshot used when loading directly back into the saved shop state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Shop-only world snapshot restored when loading directly back into the saved shop state."))
	FARCharacterShopSnapshot ShopSnapshot;
};

/** Canonical save payload persisted to disk; schema is versioned manually via CurrentSchemaVersion. */
UCLASS(BlueprintType)
class ALIENRAMEN_API UARSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 19;
	static constexpr int32 MinSupportedSchemaVersion = 18;

	UARSaveGame();

	/** Schema version baked into this build. Bump when save layout changes. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static int32 GetCurrentSchemaVersion() { return CurrentSchemaVersion; }

	/** Oldest schema version we can still load (for migration). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static int32 GetMinSupportedSchemaVersion() { return MinSupportedSchemaVersion; }

	/** Returns true when a save schema version is loadable by this build. */
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
	TArray<FParleyFactionRuntimeState> FactionPopularityStates;

	// Persistent faction reputation keyed by (FactionTag, SpeakerTag).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	TArray<FParleyFactionSpeakerReputationState> FactionSpeakerReputationStates;

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

	// Transient loose shop carryables (for example world energy drinks/meat) restored on pre-run shop reload.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	TArray<FARShopTransientCarryableSnapshot> ShopTransientCarryables;

	// One-shot gate to clear loose shop carryables on first shop entry after a completed run/results transition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	bool bClearShopTransientCarryablesOnNextShopLoad = false;

	// Deferred vending sales settled on next shop entry. Each entry is one completed bowl and machine quality.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	TArray<FARVendingStockedBowlEntry> PendingVendingStockedBowls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	FName SaveSlot = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveGameVersion = CurrentSchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveSlotNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	FDateTime LastSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta", meta = (ToolTip = "Gameplay mode tag the save was created in, used for restore validation."))
	FGameplayTag LastSavedModeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta", meta = (ToolTip = "Map path the save was created in, used for restore validation and debugging."))
	FString LastSavedMapPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Player-owned save rows keyed by player identity."))
	TArray<FARPlayerStateSaveData> PlayerStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Character-owned save rows keyed by canonical character gameplay tag."))
	TArray<FARCharacterSaveData> CharacterStates;

	// Global directed speaker relationship points.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	TArray<FDialogueSpeakerRelationshipState> DialogueSpeakerRelationshipStates;

	// Persistent game-scope completed conversations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue")
	FGameplayTagContainer DialogueCompletedConversationTagsByGame;

	// Legacy compatibility root retained only for migration of older save schema versions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Dialogue", meta = (ToolTip = "Legacy dialogue state array retained only for migration from older save versions."))
	TArray<FDialoguePlayerPersistentState> DialoguePlayerPersistentStates;

	/** Finds a player save row by identity (platform/user + display name). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	/** Finds a character save row by canonical tag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindCharacterStateDataByTag(FGameplayTag CharacterTag, FARCharacterSaveData& OutData, int32& OutIndex) const;

	FARCharacterSaveData* FindCharacterStateDataMutable(FGameplayTag CharacterTag, int32& OutIndex);
	FARCharacterSaveData& FindOrAddCharacterStateData(FGameplayTag CharacterTag);
	int32 MigrateToCurrentSchema(TArray<FString>* OutWarnings);
	int32 ValidateAndSanitize(TArray<FString>* OutWarnings);
};
