/**
 * @file ARGameStateBase.h
 * @brief ARGameStateBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARPlayerStateBase.h"
#include "ARSaveTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameStateBase.h"
#include "StructSerializable.h"
#include "ARGameStateBase.generated.h"

class AARPlayerStateBase;
class APlayerController;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnTrackedPlayersChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnGameStateHydratedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnPlayerReadyChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	EARPlayerSlot,
	SourcePlayerSlot,
	bool,
	bNewReady,
	bool,
	bOldReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnAllPlayersTravelReadyChangedSignature,
	bool,
	bNewAllPlayersTravelReady,
	bool,
	bOldAllPlayersTravelReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnUnlocksChangedSignature,
	FGameplayTagContainer,
	NewUnlocks,
	FGameplayTagContainer,
	OldUnlocks);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnMoneyChangedSignature,
	int32,
	NewMoney,
	int32,
	OldMoney);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnScrapChangedSignature,
	int32,
	NewScrap,
	int32,
	OldScrap);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnMeatChangedSignature,
	FARMeatState,
	NewMeat,
	FARMeatState,
	OldMeat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnCyclesChangedSignature,
	int32,
	NewCycles,
	int32,
	OldCycles);

/**
 * Server-authoritative game state for Alien Ramen.
 *
 * - Tracks replicated progression fields (unlock tags, money, scrap, meat, cycles).
 * - Owns travel readiness aggregation for the two-player coop model.
 * - Implements IStructSerializable so SaveSubsystem can persist/restore state across travel.
 * - Emits Blueprint events for UI when replicated values change.
 */
UCLASS()
class ALIENRAMEN_API AARGameStateBase : public AGameStateBase, public IStructSerializable
{
	GENERATED_BODY()

public:
	AARGameStateBase();

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	/** Returns the AR player occupying a specific coop slot (P1/P2), or nullptr if empty. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetPlayerBySlot(EARPlayerSlot Slot) const;

	/** Returns all AR player states (filtered view of PlayerArray). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	TArray<AARPlayerStateBase*> GetPlayerStates() const;

	/** True when at least one AR player exists and both players are travel-ready. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	bool AreAllPlayersTravelReady() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromPlayerState(const AARPlayerStateBase* CurrentPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromController(const APlayerController* CurrentPlayerController) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Players")
	AARPlayerStateBase* GetOtherPlayerStateFromPawn(const APawn* CurrentPlayerPawn) const;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnTrackedPlayersChangedSignature OnTrackedPlayersChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnPlayerReadyChangedSignature OnPlayerReadyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Players")
	FAROnAllPlayersTravelReadyChangedSignature OnAllPlayersTravelReadyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnGameStateHydratedSignature OnHydratedFromSave;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnUnlocksChangedSignature OnUnlocksChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnMoneyChangedSignature OnMoneyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnScrapChangedSignature OnScrapChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnMeatChangedSignature OnMeatChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Save")
	FAROnCyclesChangedSignature OnCyclesChanged;

	/** Runtime mirror of save-owned cycles; authority-only setter. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void SyncCyclesFromSave(int32 NewCycles);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	const FGameplayTagContainer& GetUnlocks() const { return Unlocks; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	int32 GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	int32 GetScrap() const { return Scrap; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	const FARMeatState& GetMeat() const { return Meat; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	int32 GetCycles() const { return Cycles; }

	/** Replaces unlock container from save data and broadcasts change. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void SetUnlocksFromSave(const FGameplayTagContainer& NewUnlocks);

	/** Adds a single unlock tag; returns true if added. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	bool AddUnlockTag(const FGameplayTag& UnlockTag);

	/** Removes a single unlock tag; returns true if removed. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	bool RemoveUnlockTag(const FGameplayTag& UnlockTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool HasUnlockTag(const FGameplayTag& UnlockTag) const;

	/** Writes money value from save/runtime and notifies listeners. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void SetMoneyFromSave(int32 NewMoney);

	/** Writes scrap value from save/runtime and notifies listeners. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void SetScrapFromSave(int32 NewScrap);

	/** Writes meat state from save/runtime and notifies listeners. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void SetMeatFromSave(const FARMeatState& NewMeat);

	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

	/** Called by SaveSubsystem after hydration completes to inform UI widgets. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save")
	void NotifyHydratedFromSave();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UFUNCTION()
	void HandlePlayerReadyStatusChanged(AARPlayerStateBase* SourcePlayerState, EARPlayerSlot SourcePlayerSlot, bool bNewReady, bool bOldReady);

	UFUNCTION()
	void HandlePlayerSlotChanged(EARPlayerSlot NewSlot, EARPlayerSlot OldSlot);

	UFUNCTION()
	void HandlePlayerCharacterPickedChanged(AARPlayerStateBase* SourcePlayerState, EARPlayerSlot SourcePlayerSlot, EARCharacterChoice NewCharacter, EARCharacterChoice OldCharacter);

	UFUNCTION()
	void OnRep_AllPlayersTravelReady(bool bOldAllPlayersTravelReady);

	UFUNCTION()
	void OnRep_Cycles(int32 OldCycles);

	UFUNCTION()
	void OnRep_Unlocks(FGameplayTagContainer OldUnlocks);

	UFUNCTION()
	void OnRep_Money(int32 OldMoney);

	UFUNCTION()
	void OnRep_Scrap(int32 OldScrap);

	UFUNCTION()
	void OnRep_Meat(FARMeatState OldMeat);

	void BindPlayerStateSignals(AARPlayerStateBase* PlayerState);
	void UnbindPlayerStateSignals(AARPlayerStateBase* PlayerState);
	bool ComputeAllPlayersTravelReady() const;
	void RefreshAllPlayersTravelReady();

	UPROPERTY(ReplicatedUsing = OnRep_AllPlayersTravelReady, BlueprintReadOnly, Category = "Alien Ramen|Players")
	bool bAllPlayersTravelReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_Unlocks, BlueprintReadOnly, Category = "Alien Ramen|Save")
	FGameplayTagContainer Unlocks;

	UPROPERTY(ReplicatedUsing = OnRep_Money, BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 Money = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Scrap, BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 Scrap = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Meat, BlueprintReadOnly, Category = "Alien Ramen|Save")
	FARMeatState Meat;

	UPROPERTY(ReplicatedUsing = OnRep_Cycles, BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 Cycles = 0;
};
