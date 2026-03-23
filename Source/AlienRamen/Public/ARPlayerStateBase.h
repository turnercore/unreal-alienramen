/**
 * @file ARPlayerStateBase.h
 * @brief ARPlayerStateBase header for Alien Ramen.
 */
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerState.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARPlayerTypes.h"
#include "StructSerializable.h"
#include "ARPlayerStateBase.generated.h"

class UAbilitySystemComponent;
class AARPlayerStateBase;
class AARCharacterStateRuntime;
class APawn;

USTRUCT(BlueprintType)
struct FARPlayerCoreAttributeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float MoveSpeed = 0.f;
};

USTRUCT(BlueprintType)
struct FARPlayerAttributeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float Spice = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float MaxSpice = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Player|Attributes")
	float Strength = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FAROnCoreAttributeChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	EARCoreAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FAROnPlayerAttributeChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	EARPlayerAttributeType,
	AttributeType,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnScalarAttributeChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	float,
	NewValue,
	float,
	OldValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnCurrentCharacterTagChangedSignature,
	FGameplayTag,
	NewCharacterTag,
	FGameplayTag,
	OldCharacterTag);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnCharacterPickedChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	EARCharacterChoice,
	NewCharacter,
	EARCharacterChoice,
	OldCharacter);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnDisplayNameChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	const FString&,
	NewDisplayName,
	const FString&,
	OldDisplayName);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnReadyStatusChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	bool,
	bNewReady,
	bool,
	bOldReady);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAROnPlayerSlotIdChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	int32,
	NewPlayerSlotId,
	int32,
	OldPlayerSlotId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnDownedStateChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	bool,
	bNewDowned,
	bool,
	bOldDowned);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnDeadStateChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	bool,
	bNewDead,
	bool,
	bOldDead);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAROnSetupStateChangedSignature,
	bool,
	bNewIsSetup,
	bool,
	bOldIsSetup);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnLoadoutTagsChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	const FGameplayTagContainer&,
	NewLoadoutTags,
	const FGameplayTagContainer&,
	OldLoadoutTags);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAROnTravelReadinessChangedSignature, bool, bIsReadyForTravel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAROnInvaderPlayerColorChangedSignature, EARAffinityColor, NewColor, EARAffinityColor, OldColor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnInvaderComboChangedSignature, AARPlayerStateBase*, SourcePlayerState, FGameplayTag, SourceCharacterTag, int32, NewCombo, int32, OldCombo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnInvaderActivatedUpgradesChangedSignature, AARPlayerStateBase*, SourcePlayerState, FGameplayTag, SourceCharacterTag, const FGameplayTagContainer&, NewActivatedTags, const FGameplayTagContainer&, OldActivatedTags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnSpiceSharingStateChangedSignature, AARPlayerStateBase*, SourcePlayerState, FGameplayTag, SourceCharacterTag, bool, bIsSharingNow, bool, bWasSharingBefore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAROnSpicyTrackCursorChangedSignature, AARPlayerStateBase*, SourcePlayerState, FGameplayTag, SourceCharacterTag, int32, NewCursorTier, int32, OldCursorTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAROnDialogueAutoAdvancePreferenceChangedSignature, AARPlayerStateBase*, SourcePlayerState, bool, bNewAutoAdvanceEnabled, bool, bOldAutoAdvanceEnabled);

/**
 * PlayerState backbone for Alien Ramen.
 *
 * - Owns player identity/slot/preferences and current-character pointer state.
 * - Resolves gameplay/combat reads through the current AARCharacterStateRuntime.
 * - Replicates identity (display name + active character) and lobby readiness.
 * - Carries loadout tags that drive ability/equipment initialization.
 * - Implements IStructSerializable for save/load handoff across travel.
 */
UCLASS()
class ALIENRAMEN_API AARPlayerStateBase : public APlayerState, public IAbilitySystemInterface, public IStructSerializable
{
	GENERATED_BODY()

public:
	AARPlayerStateBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAbilitySystemComponent* GetASC() const;

	/** Returns the current value of a core/shared attribute (health/max health/move speed). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	float GetCoreAttributeValue(EARCoreAttributeType AttributeType) const;

	/** Snapshot of core attributes for UI polling on remote/local players. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	FARPlayerCoreAttributeSnapshot GetCoreAttributeSnapshot() const;

	/** Returns the current value of a player-owned attribute (spice/strength and related player stats). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	float GetPlayerAttributeValue(EARPlayerAttributeType AttributeType) const;

	/** Snapshot of player-owned attributes for UI polling on remote/local players. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	FARPlayerAttributeSnapshot GetPlayerAttributeSnapshot() const;

	/** Normalized spice meter (0..1) derived from GAS attributes. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	float GetSpiceNormalized() const;

	/** Runtime-only controller/profile slot id (1-based local/session identity). */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	int32 GetPlayerSlotId() const { return PlayerSlotId; }

	/** Sets runtime-only controller/profile slot id. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void SetPlayerSlotId(int32 NewSlotId);

	/** Canonical player-slot tag for viewer-specific systems such as Emo HUD rendering. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	FGameplayTag GetPlayerSlotTag() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	EARCharacterChoice GetCharacterPicked() const { return CharacterPicked; }

	/** Canonical gameplay-tag identity for the currently controlled character. Prefer this over CharacterPicked in new logic. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	FGameplayTag GetCurrentCharacterTag() const { return CurrentCharacterTag; }

	/** Returns the replicated runtime actor that owns character-scoped combat/loadout state for CurrentCharacterTag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Character Runtime")
	AARCharacterStateRuntime* GetCurrentCharacterRuntime() const { return CurrentCharacterRuntime; }

	/** Returns the pawn currently bound to the active character runtime. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Character Runtime")
	APawn* GetCurrentCharacterPawn() const;

	/** Returns the effective loadout tags from current character runtime state. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Loadout")
	FGameplayTagContainer GetCurrentCharacterLoadoutTags() const;

	/** Authority-only runtime pointer update used by character subsystem orchestration flows. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Character Runtime", meta = (BlueprintAuthorityOnly))
	void SetCurrentCharacterRuntime(AARCharacterStateRuntime* NewRuntime);

	/** Sets picked character; client calls route to server. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetCharacterPicked(EARCharacterChoice NewCharacter);

	UFUNCTION(Server, Reliable)
	void ServerPickCharacter(EARCharacterChoice NewCharacter);

	/** Sets the active character using canonical gameplay-tag identity. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetCurrentCharacterTag(FGameplayTag NewCharacterTag);

	UFUNCTION(Server, Reliable)
	void ServerSetCurrentCharacterTag(FGameplayTag NewCharacterTag);

	/**
	 * Authority-only direct character assignment that bypasses occupancy auto-swap.
	 * Used by GameMode-coordinated multi-controller switch flows to apply final targets atomically.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void SetCurrentCharacterTagDirect(FGameplayTag NewCharacterTag);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	FString GetDisplayNameValue() const { return DisplayName; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetDisplayNameValue(const FString& NewDisplayName);

	UFUNCTION(Server, Reliable)
	void ServerUpdateDisplayName(const FString& NewDisplayName);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsReadyForRun() const { return bIsReady; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsDowned() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsDeadState() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Dialogue")
	bool IsDialogueAutoAdvanceEnabled() const { return bDialogueAutoAdvanceEnabled; }

	/** Designer/UI preference for whether dialogue should advance automatically for this player. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Dialogue")
	void SetDialogueAutoAdvanceEnabled(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void ServerSetDialogueAutoAdvanceEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetDownedState(bool bNewDowned);

	UFUNCTION(Server, Reliable)
	void ServerUpdateDownedState(bool bNewDowned);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetDeadState(bool bNewDead);

	UFUNCTION(Server, Reliable)
	void ServerUpdateDeadState(bool bNewDead);

	// Composite readiness for travel: requires a valid active character tag and ready flag.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsTravelReady() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player")
	void SetReadyForRun(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerUpdateReady(bool bNewReady);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player")
	bool IsSetupComplete() const { return bIsSetup; }

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void SetIsSetupComplete(bool bNewIsSetup);

	// First-join initialization path for non-travel PlayerStates when no saved identity row was found.
	// Intentionally keeps display name untouched and resets character choice to None.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player", meta = (BlueprintAuthorityOnly))
	void InitializeForFirstSessionJoin();

	/**
	 * Applies a hydrated player row onto this runtime PlayerState, then projects character-owned runtime data by CurrentCharacterTag.
	 * If the projected character-owned loadout resolves empty, default loadout tags are seeded so raw map/editor and join flows stay deterministic.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Save", meta = (BlueprintAuthorityOnly))
	void ApplyPlayerSaveData(const struct FARPlayerStateSaveData& PlayerData);

	// UI-friendly slot index for local co-op style displays (0-based, from GameState PlayerArray order).
	// Returns INDEX_NONE if not currently resolvable.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	int32 GetHUDPlayerSlotIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Attributes")
	void SetSpiceMeter(float NewSpiceValue);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Attributes")
	void ClearSpiceMeter();

	UFUNCTION(Server, Reliable)
	void ServerSetSpiceMeter(float NewSpiceValue);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Attributes")
	float GetStrength() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Attributes")
	void SetStrength(float NewStrength);

	UFUNCTION(Server, Reliable)
	void ServerSetStrength(float NewStrength);

	// ---- INVADER SPICY TRACK RUNTIME (non-persistent) ----

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	EARAffinityColor GetInvaderPlayerColor() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void SetInvaderPlayerColor(EARAffinityColor NewColor);

	UFUNCTION(Server, Reliable)
	void ServerSetInvaderPlayerColor(EARAffinityColor NewColor);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	int32 GetInvaderComboCount() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	float GetInvaderLastKillCreditServerTime() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ResetInvaderCombo();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ReportInvaderKillCredit(EARAffinityColor EnemyColor, float ServerTimeSeconds, float ComboTimeoutSeconds);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void MarkInvaderUpgradeActivated(FGameplayTag UpgradeTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ClearActivatedInvaderUpgrades();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	bool HasActivatedInvaderUpgrade(FGameplayTag UpgradeTag) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	const FGameplayTagContainer& GetActivatedInvaderUpgrades() const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track")
	bool IsSpiceSharingActive() const;

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track")
	void SetSpiceSharingActive(bool bNewIsSharing);

	UFUNCTION(Server, Reliable)
	void ServerSetSpiceSharingActive(bool bNewIsSharing);

	// Applies one authoritative share tick from this player into TargetPlayer.
	// If both players are sharing simultaneously, transfer is canceled (no drain/no grant).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track", meta = (BlueprintAuthorityOnly))
	void ApplySpiceShareTick(float DeltaSeconds, AARPlayerStateBase* TargetPlayer, float& OutSourceDrained, float& OutTargetGranted);

	// Local HUD prediction overlay; does not affect authoritative spice state.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Prediction")
	void SetPredictedSpiceValue(float NewPredictedSpice);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Prediction")
	void ClearPredictedSpiceValue();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Prediction")
	bool HasPredictedSpiceValue() const { return bHasPredictedSpiceValue; }

	// ---- INVADER SPICY TRACK CURSOR ----

	// Server-authoritative spicy-track cursor tier for this player.
	// 0 = no slotted tier selected, 1..N = track tier selected.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Cursor")
	int32 GetSpicyTrackCursorTier() const;

	// Local predicted cursor tier for responsive HUD input feedback.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Cursor|Prediction")
	int32 GetPredictedSpicyTrackCursorTier() const { return PredictedSpicyTrackCursorTier; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Cursor|Prediction")
	bool HasPredictedSpicyTrackCursorTier() const { return bHasPredictedSpicyTrackCursorTier; }

	// HUD helper: local prediction when available, authoritative value otherwise.
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|Spice Track|Cursor")
	int32 GetEffectiveSpicyTrackCursorTier() const;

	// Request absolute cursor tier set (client routes to server).
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Cursor")
	void SetSpicyTrackCursorTier(int32 NewCursorTier);

	UFUNCTION(Server, Reliable)
	void ServerSetSpicyTrackCursorTier(int32 NewCursorTier);

	// Request cursor tier delta (+1/-1 etc), with local prediction on owning client.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Cursor")
	void AdjustSpicyTrackCursorTier(int32 DeltaTier);

	// Server-only: snap cursor to current highest selectable tier.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Cursor", meta = (BlueprintAuthorityOnly))
	void SnapSpicyTrackCursorToHighestSelectable();

	// Server-only runtime reset for new invader run/session initialization.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Cursor", meta = (BlueprintAuthorityOnly))
	void ResetSpicyTrackCursor();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Cursor|Prediction")
	void SetPredictedSpicyTrackCursorTier(int32 NewPredictedCursorTier);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|Spice Track|Cursor|Prediction")
	void ClearPredictedSpicyTrackCursorTier();

	// ---- LOADOUT (GameplayTag driven) ----

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Loadout")
	void SetLoadoutTags(const FGameplayTagContainer& NewLoadoutTags);

	// Convenience update: routes through server authority and applies slot-aware replacement/add semantics.
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Loadout")
	void UpdateLoadoutWithTag(FGameplayTag NewTag);

	UFUNCTION(Server, Reliable)
	void ServerUpdateLoadoutWithTag(FGameplayTag NewTag);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Player|Loadout")
	void RemoveTagFromLoadout(FGameplayTag TagToRemove);

	UFUNCTION(Server, Reliable)
	void ServerRemoveTagFromLoadout(FGameplayTag TagToRemove);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Player|Loadout")
	TArray<FGameplayTag> GetTagsInLoadoutSlot(FGameplayTag SlotTag) const;

	UFUNCTION()
	void OnRep_Loadout(const FGameplayTagContainer& OldLoadoutTags);

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnCoreAttributeChangedSignature OnCoreAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnPlayerAttributeChangedSignature OnPlayerAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnSpiceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnMaxSpiceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnMoveSpeedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player|Attributes")
	FAROnScalarAttributeChangedSignature OnStrengthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnCurrentCharacterTagChangedSignature OnCurrentCharacterTagChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnCharacterPickedChangedSignature OnCharacterPickedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnDisplayNameChangedSignature OnDisplayNameChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnReadyStatusChangedSignature OnReadyStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnPlayerSlotIdChangedSignature OnPlayerSlotIdChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnDownedStateChangedSignature OnDownedStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnDeadStateChangedSignature OnDeadStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnSetupStateChangedSignature OnSetupStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnLoadoutTagsChangedSignature OnLoadoutTagsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Player")
	FAROnTravelReadinessChangedSignature OnTravelReadinessChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderPlayerColorChangedSignature OnInvaderPlayerColorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderComboChangedSignature OnInvaderComboChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnInvaderActivatedUpgradesChangedSignature OnInvaderActivatedUpgradesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track")
	FAROnSpiceSharingStateChangedSignature OnSpiceSharingStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|Spice Track|Cursor")
	FAROnSpicyTrackCursorChangedSignature OnSpicyTrackCursorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Dialogue")
	FAROnDialogueAutoAdvancePreferenceChangedSignature OnDialogueAutoAdvancePreferenceChanged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Serialization")
	TObjectPtr<UScriptStruct> ClassStateStruct;

	/**
	 * Seamless-travel carry path from old PlayerState to the new instance.
	 *
	 * Contract:
	 * - copies player-owned identity/runtime fields explicitly (slot, current character pointer, display name, dialogue preference)
	 * - does not implicitly copy character-runtime-owned combat/loadout state
	 * - resets per-run player transients that should not survive mode travel (ready state)
	 * - avoids generic by-name struct overlay for PlayerState handoff to prevent stale/mismatched BP state from reintroducing duplicate identity mirrors
	 */
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual bool ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION()
	void OnRep_PlayerSlotId(int32 OldSlotId);
	UFUNCTION()
	void OnRep_CharacterPicked(EARCharacterChoice OldCharacter);
	UFUNCTION()
	void OnRep_DisplayName(const FString& OldDisplayName);
	UFUNCTION()
	void OnRep_IsReady(bool bOldReady);

	UFUNCTION()
	void OnRep_DialogueAutoAdvanceEnabled(bool bOldEnabled);

	UFUNCTION()
	void OnRep_IsSetup(bool bOldIsSetup);
	UFUNCTION()
	void OnRep_CurrentCharacterTag(FGameplayTag OldCharacterTag);

	UFUNCTION()
	void OnRep_CurrentCharacterRuntime();
	void BindCurrentRuntimeDelegates();
	void UnbindCurrentRuntimeDelegates();

	UFUNCTION()
	void HandleRuntimeLoadoutChanged(
		AARCharacterStateRuntime* SourceRuntime,
		const FGameplayTagContainer& NewLoadoutTags,
		const FGameplayTagContainer& OldLoadoutTags);

	UFUNCTION()
	void HandleRuntimeDownedChanged(
		AARCharacterStateRuntime* SourceRuntime,
		FGameplayTag CharacterTag,
		bool bNewDowned,
		bool bOldDowned);

	UFUNCTION()
	void HandleRuntimeDeadChanged(
		AARCharacterStateRuntime* SourceRuntime,
		FGameplayTag CharacterTag,
		bool bNewDead,
		bool bOldDead);

	UFUNCTION()
	void HandleRuntimeInvaderColorChanged(EARAffinityColor NewColor, EARAffinityColor OldColor);

	UFUNCTION()
	void HandleRuntimeInvaderComboChanged(
		AARCharacterStateRuntime* SourceRuntime,
		FGameplayTag CharacterTag,
		int32 NewCombo,
		int32 OldCombo);

	UFUNCTION()
	void HandleRuntimeActivatedUpgradesChanged(
		AARCharacterStateRuntime* SourceRuntime,
		FGameplayTag CharacterTag,
		const FGameplayTagContainer& NewActivatedTags,
		const FGameplayTagContainer& OldActivatedTags);

	UFUNCTION()
	void HandleRuntimeSpiceSharingChanged(
		AARCharacterStateRuntime* SourceRuntime,
		FGameplayTag CharacterTag,
		bool bNewSharing,
		bool bOldSharing);

	UFUNCTION()
	void HandleRuntimeSpicyTrackCursorChanged(
		AARCharacterStateRuntime* SourceRuntime,
		FGameplayTag CharacterTag,
		int32 NewCursorTier,
		int32 OldCursorTier);
	void SetCharacterPicked_Internal(EARCharacterChoice NewCharacter);
	void SetCurrentCharacterTagWithSwap_Internal(FGameplayTag NewCharacterTag);
	void SetCurrentCharacterTag_Internal(FGameplayTag NewCharacterTag, bool bMarkSaveDirty = true);
	void SetInvaderPlayerColor_Internal(EARAffinityColor NewColor, bool bForceBroadcast = false);
	void SetSpiceSharingActive_Internal(bool bNewIsSharing, bool bForceBroadcast = false);
	void SetSpicyTrackCursorTier_Internal(int32 NewCursorTier, bool bForceBroadcast = false);
	int32 ClampSpicyTrackCursorTier(int32 RequestedCursorTier) const;
	int32 ResolveMaxSelectableSpicyTrackCursorTier() const;
	int32 ResolveSpiceTierFromValue(float SpiceValue) const;
	EARAffinityColor ResolveDefaultInvaderPlayerColorFromCharacter(EARCharacterChoice InCharacterChoice) const;
	static bool DoesInvaderColorMatch(EARAffinityColor PlayerColor, EARAffinityColor EnemyColor);
	void SetDisplayName_Internal(const FString& NewDisplayName);
	void SetReady_Internal(bool bNewReady);
	void SetDowned_Internal(bool bNewDowned);
	void SetDead_Internal(bool bNewDead);
	void SetDialogueAutoAdvanceEnabled_Internal(bool bEnabled);
	void SetPlayerSlotId_Internal(int32 NewSlotId);
	void SetLoadoutTags_Internal(const FGameplayTagContainer& NewLoadoutTags, bool bMarkSaveDirty = true);
	// Character-owned loadout cache for runtime-only flows (for example seamless travel/session state without disk IO).
	void CacheCharacterOwnedLoadout(const FGameplayTag CharacterTag, const FGameplayTagContainer& LoadoutTagsToCache);
	bool TryResolveCharacterOwnedLoadout(const FGameplayTag CharacterTag, FGameplayTagContainer& OutLoadoutTags) const;
	void UpdateLoadoutWithTag_Internal(FGameplayTag NewTag);
	void RemoveTagFromLoadout_Internal(FGameplayTag TagToRemove);
	void NormalizeLoadoutTagsForSlotRules(FGameplayTagContainer& InOutTags) const;
	bool IsSingleSlotLoadoutRootTag(FGameplayTag RootTag) const;
	void EnsureDefaultLoadoutIfEmpty();
	void BindTrackedAttributeDelegates();
	void UnbindTrackedAttributeDelegates();
	void BroadcastTrackedAttributeSnapshot();
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleStrengthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleInvaderColorOverrideTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleSpiceSharingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void EvaluateInvaderColorFromASCOverrideTags();
	void ApplyInvaderColorGameplayTags(EARAffinityColor NewColor);
	EARAffinityColor ResolveInvaderColorFromASCOverrideTags() const;
	void EvaluateLifeStateFromASC();
	void BroadcastCoreAttributeChanged(EARCoreAttributeType AttributeType, float NewValue, float OldValue);
	void BroadcastPlayerAttributeChanged(EARPlayerAttributeType AttributeType, float NewValue, float OldValue);
	void SetSpiceMeter_Internal(float NewSpiceValue);
	void SetStrength_Internal(float NewStrength);
	bool EnsureReadyPrerequisitesForRun();
	void EvaluateTravelReadinessAndBroadcast();

	UPROPERTY(ReplicatedUsing=OnRep_PlayerSlotId, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player", meta = (ToolTip = "Runtime-only controller/profile slot id used for controller-owned systems such as pause voting. Not used for character ownership."))
	int32 PlayerSlotId = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterPicked, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	EARCharacterChoice CharacterPicked = EARCharacterChoice::None;

	// Canonical runtime character identity used for save ownership and new gameplay logic.
	UPROPERTY(ReplicatedUsing=OnRep_CurrentCharacterTag, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player", meta = (ToolTip = "Canonical gameplay-tag identity for the active character. New systems should use this instead of CharacterPicked."))
	FGameplayTag CurrentCharacterTag;

	// Character-owned replicated runtime owner. This actor is authoritative for combat/loadout state.
	UPROPERTY(ReplicatedUsing=OnRep_CurrentCharacterRuntime, Transient, BlueprintReadOnly, Category = "Alien Ramen|Player|Character Runtime", meta = (ToolTip = "Replicated character runtime actor owning combat/loadout state for CurrentCharacterTag."))
	TObjectPtr<AARCharacterStateRuntime> CurrentCharacterRuntime = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayName, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	FString DisplayName;

	UPROPERTY(ReplicatedUsing=OnRep_IsReady, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	bool bIsReady = false;

	UPROPERTY(ReplicatedUsing=OnRep_IsSetup, EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Player")
	bool bIsSetup = false;

	UPROPERTY(ReplicatedUsing=OnRep_DialogueAutoAdvanceEnabled, Transient, BlueprintReadOnly, Category = "Alien Ramen|Dialogue", meta = (ToolTip = "Per-player preference controlling whether dialogue lines auto-advance when possible."))
	bool bDialogueAutoAdvanceEnabled = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Prediction")
	float PredictedSpiceValue = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Prediction")
	bool bHasPredictedSpiceValue = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Cursor|Prediction")
	int32 PredictedSpicyTrackCursorTier = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|Spice Track|Cursor|Prediction")
	bool bHasPredictedSpicyTrackCursorTier = false;

	// Cached travel readiness for change detection.
	UPROPERTY(Transient)
	bool bCachedTravelReady = false;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle SpiceChangedDelegateHandle;
	FDelegateHandle MaxSpiceChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;
	FDelegateHandle StrengthChangedDelegateHandle;
	FDelegateHandle DownedTagChangedDelegateHandle;
	FDelegateHandle DeadTagChangedDelegateHandle;
	FDelegateHandle ColorNoneTagChangedDelegateHandle;
	FDelegateHandle ColorRedTagChangedDelegateHandle;
	FDelegateHandle ColorWhiteTagChangedDelegateHandle;
	FDelegateHandle ColorBlueTagChangedDelegateHandle;
	FDelegateHandle SharingSpiceTagChangedDelegateHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundTrackedASC;
	TWeakObjectPtr<AARCharacterStateRuntime> BoundRuntimeForDelegates;
	bool bUpdatingInvaderColorFromTags = false;
	bool bApplyingInvaderColorTags = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
