/**
 * @file ARInvaderGameState.h
 * @brief ARInvaderGameState header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "ARGameStateBase.h"
#include "ARInvaderDropTypes.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARInvaderTypes.h"
#include "ARInvaderGameState.generated.h"

class AAREnemyBase;
class AARInvaderDropBase;
class UARInvaderSpicyTrackSettings;
class IConsoleObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnInvaderSharedTrackChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnInvaderFullBlastSessionChangedSignature, bool, bIsActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAROnInvaderFullBlastResolvedSignature, bool, bSkipped, int32, ActivationTier, EARPlayerSlot, RequestingPlayerSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnInvaderKillCreditAwardedSignature, AARPlayerStateBase*, SourcePlayerState, EARPlayerSlot, SourcePlayerSlot, float, SpiceGained, int32, NewCombo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAROnInvaderOfferPresenceChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnInvaderKillCreditFxEventSignature, const FARInvaderKillCreditFxEvent&, EventData);

UCLASS()
class ALIENRAMEN_API AARInvaderGameState : public AARGameStateBase
{
	GENERATED_BODY()

public:
	AARInvaderGameState();

	virtual UScriptStruct* GetStateStruct_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	const TArray<FARInvaderTrackSlotState>& GetSharedTrackSlots() const { return SharedTrackSlots; }

	// Returns localized upgrade display names in slot order (index 0 = slot 1).
	// Empty text indicates an unoccupied slot or unresolved upgrade definition.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	void GetSharedTrackUpgradeDisplayNames(TArray<FText>& OutDisplayNames) const;

	// Returns localized display name for one slot (1-based index). Returns empty text when unresolved.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	FText GetSharedTrackUpgradeDisplayNameAtSlot(int32 SlotIndex) const;

	// Returns convenience UI data in slot order.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	void GetSharedTrackSlotDisplayStates(TArray<FARInvaderTrackSlotDisplayState>& OutSlots) const;

	// Returns 0-based index for placing "Full Blast" text in tier UI lanes (excluding the 0-99 empty lane).
	// Example: tier 1 -> index 0, tier 3 -> index 2.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetFullBlastDisplayIndex() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetSharedFullBlastTier() const { return SharedFullBlastTier; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetSharedMaxSpice() const;

	// Returns the highest currently selectable spicy-track cursor tier for the player.
	// Tier 0 is always valid (no selection). Tiers >=1 require sufficient spice and:
	// - track upgrade tiers (1..SharedFullBlastTier-1): a valid slotted upgrade at that tier
	// - full blast tier (SharedFullBlastTier): full blast activation is affordable
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetMaxSelectableTrackCursorTierForPlayer(const AARPlayerStateBase* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	const FARInvaderFullBlastSessionState& GetFullBlastSession() const { return FullBlastSession; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	const TArray<FARInvaderOfferPresenceState>& GetOfferPresenceStates() const { return OfferPresenceStates; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool RequestActivateFullBlast(AARPlayerStateBase* RequestingPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool ResolveFullBlastSelection(AARPlayerStateBase* RequestingPlayerState, FGameplayTag SelectedUpgradeTag, int32 DesiredDestinationSlot);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool ResolveFullBlastSkip(AARPlayerStateBase* RequestingPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool ActivateTrackUpgrade(AARPlayerStateBase* RequestingPlayerState, int32 SlotIndex);

	// Hold-to-share API (server authoritative).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void StartSharingSpice(AARPlayerStateBase* SourcePlayerState);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void StopSharingSpice(AARPlayerStateBase* SourcePlayerState);

	// Explicit/manual kill-credit path for scripted or non-standard kill attribution.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool AwardKillCredit(AARPlayerStateBase* KillerPlayerState, EARAffinityColor EnemyColor, float BaseSpiceValueOverride = -1.0f);

	// Automatic ingestion path called by enemies on death.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void NotifyEnemyKilled(AAREnemyBase* Enemy, AActor* InstigatorActor);

	// Replicated live offer-presence state for HUD cursors/highlights during full-blast selection.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool SetOfferPresence(
		AARPlayerStateBase* SourcePlayerState,
		FGameplayTag HoveredUpgradeTag,
		int32 HoveredDestinationSlot,
		FVector2D CursorNormalized,
		bool bHasCursor);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	bool ClearOfferPresence(AARPlayerStateBase* SourcePlayerState);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderSharedTrackChangedSignature OnInvaderSharedTrackChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderFullBlastSessionChangedSignature OnInvaderFullBlastSessionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderFullBlastResolvedSignature OnInvaderFullBlastResolved;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderKillCreditAwardedSignature OnInvaderKillCreditAwarded;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderOfferPresenceChangedSignature OnInvaderOfferPresenceChanged;

	// Fires on server + clients via NetMulticast when spice is actually awarded from kill credit.
	// Use for cosmetic/UI feedback (for example enemy->meter fly-in particles).
	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderKillCreditFxEventSignature OnInvaderKillCreditFxEvent;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UFUNCTION()
	void OnRep_SharedTrackSlots(const TArray<FARInvaderTrackSlotState>& OldSlots);

	UFUNCTION()
	void OnRep_SharedFullBlastTier(int32 OldTier);

	UFUNCTION()
	void OnRep_FullBlastSession(const FARInvaderFullBlastSessionState& OldSession);

	UFUNCTION()
	void OnRep_OfferPresenceStates(const TArray<FARInvaderOfferPresenceState>& OldPresenceStates);

	UFUNCTION()
	void HandleTrackedPlayersChanged();

private:
	struct FResolvedDropStackEntry
	{
		int32 Denomination = 1;
		TSubclassOf<AARInvaderDropBase> DropClass;
	};

	struct FDropSpawnPlanEntry
	{
		int32 Amount = 0;
		TSubclassOf<AARInvaderDropBase> DropClass;
	};

	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();
	void HandleConsoleSetSpice(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleAddSpice(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleAddScrap(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleAddMoney(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleAddMeat(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleSetDropEarthGravity(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleSetCursor(const TArray<FString>& Args, UWorld* World);
	void HandleConsoleInjectTopSlot(const TArray<FString>& Args, UWorld* World);
	AARPlayerStateBase* ResolvePlayerStateFromDebugToken(const FString& Token) const;
	bool ResolveUpgradeTagForDebugInject(const FString& TagToken, FGameplayTag& OutUpgradeTag) const;
	void ReconcilePlayerCursorSelection();

	const UARInvaderSpicyTrackSettings* GetSpicyTrackSettings() const;
	void InitializeSpicyTrackState();
	void EnsureTrackSlotCount(int32 RequiredSlots);
	void NormalizeTrackSlotIndices();
	void TrimTrackToTierLimit();
	void SyncSharedMaxSpiceToPlayers();
	void ResetAllPlayerSpiceMeters();
	void TickShareTransfers(float DeltaSeconds);
	void TickComboTimeouts(float ServerTimeSeconds);
	void RefreshWhileSlottedEffects();
	void ClearWhileSlottedEffects();
	void ClearWhileSlottedEffectsForPlayer(AARPlayerStateBase* PlayerState);
	void ResolveFullBlastCommonPostChoice(bool bSkipped, EARPlayerSlot RequestingSlot, int32 ActivationTier);
	void ApplyFullBlastGameplayCue();
	void ClearEnemyProjectilesByTag();
	bool BuildUpgradeDefinitionMap(TMap<FGameplayTag, FARInvaderUpgradeDefRow>& OutDefinitions) const;
	bool IsUpgradeEligibleForOffer(
		const FARInvaderUpgradeDefRow& UpgradeDef,
		int32 ActivationTier,
		const FGameplayTagContainer& TeamActivatedTags,
		const TMap<FGameplayTag, int32>& TeamActivationCounts,
		const FGameplayTagContainer& SlottedUpgradeTags) const;
	int32 RollOfferLevelFromTier(int32 BaseTier);
	int32 GetTeamActivationCount(const TMap<FGameplayTag, int32>& TeamActivationCounts, const FGameplayTag& UpgradeTag) const;
	void BuildTeamActivationState(FGameplayTagContainer& OutTeamActivatedTags, TMap<FGameplayTag, int32>& OutTeamActivationCounts) const;
	bool CanPlayerActivateUpgrade(const FARInvaderUpgradeDefRow& UpgradeDef, AARPlayerStateBase* RequestingPlayerState, const FGameplayTagContainer& TeamActivatedTags, const TMap<FGameplayTag, int32>& TeamActivationCounts) const;
	bool ApplyUpgradeActivation(AARPlayerStateBase* RequestingPlayerState, const FARInvaderUpgradeDefRow& UpgradeDef);
	bool AwardKillCreditInternal(
		AARPlayerStateBase* KillerPlayerState,
		EARAffinityColor EnemyColor,
		float BaseSpiceValueOverride,
		FVector EffectOrigin,
		bool bHasEffectOrigin,
		FGameplayTag EnemyIdentifierTag);
	void TrySpawnEnemyDrop(AAREnemyBase* Enemy, AARPlayerStateBase* KillerPlayerState);
	float RollDropAmountWithVariance(float BaseDropAmount, EARInvaderDropType DropType) const;
	float ResolveKillerDropMultiplier(const AARPlayerStateBase* KillerPlayerState, EARInvaderDropType DropType) const;
	void ResolveDropStackDefinitions(EARInvaderDropType DropType, TArray<FResolvedDropStackEntry>& OutDefinitions) const;
	bool BuildDropSpawnPlan(EARInvaderDropType DropType, int32 TotalAmount, TArray<FDropSpawnPlanEntry>& OutPlan) const;
	float ResolveEnemyBaseSpiceValue(const AAREnemyBase* Enemy) const;
	AARPlayerStateBase* ResolvePlayerStateFromInstigatorActor(AActor* InstigatorActor) const;
	static EARAffinityColor ToPlayerColor(EARAffinityColor EnemyColor);
	void SetDropEarthGravityEnabledForAll(bool bEnabled);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastNotifyKillCreditFxEvent(const FARInvaderKillCreditFxEvent& EventData);

	UPROPERTY(ReplicatedUsing = OnRep_SharedTrackSlots)
	TArray<FARInvaderTrackSlotState> SharedTrackSlots;

	UPROPERTY(ReplicatedUsing = OnRep_SharedFullBlastTier)
	int32 SharedFullBlastTier = 1;

	UPROPERTY(ReplicatedUsing = OnRep_FullBlastSession)
	FARInvaderFullBlastSessionState FullBlastSession;

	UPROPERTY(ReplicatedUsing = OnRep_OfferPresenceStates)
	TArray<FARInvaderOfferPresenceState> OfferPresenceStates;

	TSet<TWeakObjectPtr<AARPlayerStateBase>> ActiveSpiceSharers;

	TMap<TWeakObjectPtr<AARPlayerStateBase>, TArray<FActiveGameplayEffectHandle>> WhileSlottedEffectHandlesByPlayer;

	UPROPERTY(Transient)
	FRandomStream OfferRng;

	IConsoleObject* CmdDebugSetSpice = nullptr;
	IConsoleObject* CmdDebugAddSpice = nullptr;
	IConsoleObject* CmdDebugAddScrap = nullptr;
	IConsoleObject* CmdDebugAddMoney = nullptr;
	IConsoleObject* CmdDebugAddMeat = nullptr;
	IConsoleObject* CmdDebugSetDropEarthGravity = nullptr;
	IConsoleObject* CmdDebugSetCursor = nullptr;
	IConsoleObject* CmdDebugInjectTopSlot = nullptr;

	bool bDebugDropEarthGravityEnabled = false;
};
