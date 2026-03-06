/**
 * @file ARInvaderGameState.h
 * @brief ARInvaderGameState header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "ARGameStateBase.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARInvaderTypes.h"
#include "ARInvaderGameState.generated.h"

class AAREnemyBase;
class UARInvaderSpicyTrackSettings;

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

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetSharedFullBlastTier() const { return SharedFullBlastTier; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetSharedMaxSpice() const;

	// Returns the highest currently selectable spicy-track cursor tier for the player.
	// Tier 0 is always valid (no slotted upgrade selected); tiers >=1 require both
	// sufficient spice and a valid slotted upgrade at that tier.
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
	float ResolveEnemyBaseSpiceValue(const AAREnemyBase* Enemy) const;
	AARPlayerStateBase* ResolvePlayerStateFromInstigatorActor(AActor* InstigatorActor) const;
	static EARAffinityColor ToPlayerColor(EARAffinityColor EnemyColor);

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
};
