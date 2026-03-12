/**
 * @file ARRunBuffSubsystem.h
 * @brief Save-backed temp run-buff inventory/runtime rotation for Invader + Scrapyard.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARRunBuffTypes.h"
#include "ARRunBuffSubsystem.generated.h"

class UARSaveSubsystem;
class UARSaveGame;
class AARGameStateBase;
class AARPlayerStateBase;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnRunBuffStateChangedSignature, const FARRunBuffStateSnapshot&, Snapshot);

UCLASS()
class ALIENRAMEN_API UARRunBuffSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Snapshot view for UI binding (stored + queued + active + cycle id). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Run Buff")
	FARRunBuffStateSnapshot GetRunBuffStateSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Run Buff")
	int32 GetStoredEnergyDrinkCount(FGameplayTag ItemTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Run Buff")
	int32 GetStoredEnergyDrinkCountForCharacter(FGameplayTag ItemTag, FGameplayTag CharacterTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Run Buff")
	int32 GetQueuedEnergyDrinkCount(FGameplayTag ItemTag) const;

	/** True when storage unlock is active; extracted drinks route to storage instead of queue. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Run Buff")
	bool HasEnergyDrinkStorageUnlock() const;

	/**
	 * Route extracted energy drink rewards into stored or queued inventory.
	 * Storage unlock present -> stored; no unlock -> queued for next run.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool AddExtractedEnergyDrink(FGameplayTag ItemTag, int32 Count = 1);

	/** Consume stored drink(s) and queue them for next Invader run rotation. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool UseStoredEnergyDrink(FGameplayTag ItemTag, int32 Count = 1);

	/** Queue additional drink stack directly for next Invader run rotation. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool QueueEnergyDrinkForNextRun(FGameplayTag ItemTag, int32 Count = 1);

	/** Shop-only consume path: applies one drink payload immediately for this character. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool ConsumeEnergyDrinkForCharacter(FGameplayTag ItemTag, FGameplayTag CharacterTag);

	/** Convenience consume wrapper that resolves character key from PlayerState. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool ConsumeEnergyDrinkForPlayerState(FGameplayTag ItemTag, AARPlayerStateBase* PlayerState);

	/** Shop-world consume path for spawned carry actors (does not require stored inventory count). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool ConsumeSpawnedEnergyDrinkForPlayerState(FGameplayTag ItemTag, AARPlayerStateBase* PlayerState);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Run Buff")
	bool IsEnergyDrinkActiveForCharacter(FGameplayTag ItemTag, FGameplayTag CharacterTag) const;

	/** Shop-entry hard clear for run-only buff payloads (keeps stored inventory). */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	void ClearRunBuffsForShopEntry();

	/** Invader-end clear for persisted queued buffers while keeping active payload until shop cleanup. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	void ClearQueuedRunBuffsAtInvaderEnd();

	/** Sell stored drink(s) for money based on scrapyard item definition sell values. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Run Buff")
	bool SellStoredEnergyDrink(FGameplayTag ItemTag, int32 Count, int32& OutMoneyAwarded);

	/**
	 * Invader-init rotation (authority):
	 * - remove previous active runtime applications
	 * - consume queued drinks into active payload
	 * - increment cycle id
	 * - apply new active payload once for this world init
	 */
	bool RotateRunBuffsAtInvaderInit();

	/** Applies current active payload to a specific player (late join/respawn-safe). */
	bool ApplyActiveRunBuffsToPlayerState(AARPlayerStateBase* PlayerState);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Run Buff")
	FAROnRunBuffStateChangedSignature OnRunBuffStateChanged;

private:
	struct FAppliedTagCount
	{
		FGameplayTag Tag;
		int32 Count = 0;
	};

	bool EnsureAuthorityWorld(const TCHAR* Context) const;
	UARSaveSubsystem* ResolveSaveSubsystem() const;
	UARSaveGame* ResolveMutableSave() const;
	const UARSaveGame* ResolveSave() const;
	AARGameStateBase* ResolveGameState() const;
	bool ResolveScrapyardItemDefinition(FGameplayTag ItemTag, struct FARScrapyardItemDefRow& OutDef) const;
	bool ResolveEnergyDrinkDefinition(FGameplayTag ItemTag, struct FAREnergyDrinkDefRow& OutDef) const;
	int32 ResolveMaxStackCountForItem(FGameplayTag ItemTag) const;
	FGameplayTag ResolveEnergyDrinkStorageUnlockTag() const;
	FGameplayTag ResolveCharacterTagFromPlayerState(const AARPlayerStateBase* PlayerState) const;
	static int32 GetStackCount(const TArray<FARRunBuffItemStack>& Stacks, FGameplayTag ItemTag, FGameplayTag CharacterTag = FGameplayTag());
	static int32 UpsertStackCount(TArray<FARRunBuffItemStack>& Stacks, FGameplayTag ItemTag, FGameplayTag CharacterTag, int32 Delta);
	static bool IsMatchingStackKey(const FARRunBuffItemStack& Stack, FGameplayTag ItemTag, FGameplayTag CharacterTag);
	static bool IsMatchingPayloadKey(const FARRunBuffActivePayload& Payload, FGameplayTag ItemTag, FGameplayTag CharacterTag);
	static void NormalizeStacks(TArray<FARRunBuffItemStack>& Stacks);
	static void NormalizePayloads(TArray<FARRunBuffActivePayload>& Payloads);
	void MarkSaveDirty() const;
	void BroadcastSnapshotChanged() const;
	void ResetRuntimeApplications();
	void RemoveRuntimeApplicationsFromPlayer(AARPlayerStateBase* PlayerState);
	void ApplyPayloadToPlayer(AARPlayerStateBase* PlayerState, const FARRunBuffActivePayload& Payload);
	bool ApplyEnergyDrinkPayloadForCharacter(UARSaveGame* SaveGame, FGameplayTag ItemTag, FGameplayTag CharacterTag);

	TMap<TWeakObjectPtr<AARPlayerStateBase>, TArray<FActiveGameplayEffectHandle>> AppliedEffectHandlesByPlayer;

	TMap<TWeakObjectPtr<AARPlayerStateBase>, TArray<FAppliedTagCount>> AppliedTagCountsByPlayer;

	TWeakObjectPtr<UWorld> LastRotationWorld;

	int32 LastRotationCycleId = INDEX_NONE;
};
