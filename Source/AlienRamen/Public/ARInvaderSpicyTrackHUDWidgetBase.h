/**
 * @file ARInvaderSpicyTrackHUDWidgetBase.h
 * @brief Reusable HUD widget bridge for Invader spicy-track signals.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "ARInvaderSpicyTrackTypes.h"
#include "ARInvaderSpicyTrackHUDWidgetBase.generated.h"

class AARInvaderGameState;
class AARInvaderHUD;
class AARPlayerStateBase;

/**
 * Per-character spicy-track snapshot used by HUD widgets.
 *
 * This is keyed by canonical `SourceCharacterTag` so UI can render by character
 * identity instead of local controller/player index.
 */
USTRUCT(BlueprintType)
struct FARInvaderSpicyTrackCharacterState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track")
	TObjectPtr<AARPlayerStateBase> SourcePlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track")
	FGameplayTag SourceCharacterTag;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track")
	float CurrentSpiceValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track")
	float MaxSpiceValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track")
	int32 CurrentCursorTier = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnInvaderWidgetCharacterSpiceTrackChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	float,
	NewSpiceValue,
	float,
	OldSpiceValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnInvaderWidgetCharacterMaxSpiceTrackChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	float,
	NewMaxSpiceValue,
	float,
	OldMaxSpiceValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FAROnInvaderWidgetCharacterCursorChangedSignature,
	AARPlayerStateBase*,
	SourcePlayerState,
	FGameplayTag,
	SourceCharacterTag,
	int32,
	NewCursorTier,
	int32,
	OldCursorTier);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAROnInvaderWidgetSharedTrackChangedSignature,
	const TArray<FARInvaderTrackSlotDisplayState>&,
	SharedTrackSlots);

/**
 * Base class for Invader spicy-track HUD elements.
 *
 * Responsibilities:
 * - bind to an owning `AARInvaderHUD` and resolve `AARInvaderGameState`
 * - track all replicated Invader player states, not only the local player
 * - expose per-character spicy/max-spice/cursor change events for HUD visuals
 * - expose shared-track display-state changes for team upgrade lane rendering
 */
UCLASS(Abstract, Blueprintable)
class ALIENRAMEN_API UARInvaderSpicyTrackHUDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds this widget to an Invader HUD context and starts delegate tracking. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|UI|Spice Track")
	void InitializeInvaderSpicyTrackHUDWidget(AARInvaderHUD* InInvaderHUD);

	/** Unbinds all delegate wiring and clears cached spicy-track snapshots. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|UI|Spice Track")
	void DeinitializeInvaderSpicyTrackHUDWidget();

	/** Attempts to bind from `GetOwningPlayer()->GetHUD()` if it is an `AARInvaderHUD`. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|UI|Spice Track")
	bool TryBindOwningInvaderHUD();

	/** Rebuilds cached player + shared-track snapshots and optionally broadcasts snapshot events. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Invader|UI|Spice Track")
	void RefreshInvaderSpicyTrackSnapshot(bool bBroadcastSnapshotEvents = true);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|UI|Spice Track")
	AARInvaderHUD* GetBoundInvaderHUD() const { return BoundInvaderHUD.Get(); }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|UI|Spice Track")
	AARInvaderGameState* GetBoundInvaderGameState() const { return BoundInvaderGameState.Get(); }

	/** Returns a cached snapshot for one canonical character tag. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|UI|Spice Track")
	bool GetCharacterStateByTag(FGameplayTag CharacterTag, FARInvaderSpicyTrackCharacterState& OutState) const;

	/** Returns all cached per-character spicy-track snapshots. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|UI|Spice Track")
	const TArray<FARInvaderSpicyTrackCharacterState>& GetCachedCharacterStates() const { return CachedCharacterStates; }

	/** Returns the latest UI-ready shared-track display snapshot cached from `AARInvaderGameState`. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|UI|Spice Track")
	bool GetSharedTrackSlotDisplayStates(TArray<FARInvaderTrackSlotDisplayState>& OutSharedTrackSlots) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Invader|UI|Spice Track")
	bool HasSharedTrackSnapshot() const { return bHasSharedTrackSnapshot; }

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|UI|Spice Track")
	FAROnInvaderWidgetCharacterSpiceTrackChangedSignature OnInvaderWidgetCharacterSpiceTrackChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|UI|Spice Track")
	FAROnInvaderWidgetCharacterMaxSpiceTrackChangedSignature OnInvaderWidgetCharacterMaxSpiceTrackChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|UI|Spice Track")
	FAROnInvaderWidgetCharacterCursorChangedSignature OnInvaderWidgetCharacterCursorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alien Ramen|Invader|UI|Spice Track")
	FAROnInvaderWidgetSharedTrackChangedSignature OnInvaderWidgetSharedTrackChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Fires before the first snapshot broadcast so Blueprint can cache bound HUD/GameState references safely. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|UI|Spice Track")
	void BP_OnInvaderSpicyTrackHUDWidgetInitialized(AARInvaderHUD* InInvaderHUD, AARInvaderGameState* InInvaderGameState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|UI|Spice Track")
	void BP_OnInvaderSpicyTrackHUDWidgetDeinitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|UI|Spice Track")
	void BP_OnInvaderWidgetCharacterSpiceTrackChanged(
		AARPlayerStateBase* SourcePlayerState,
		FGameplayTag SourceCharacterTag,
		float NewSpiceValue,
		float OldSpiceValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|UI|Spice Track")
	void BP_OnInvaderWidgetCharacterMaxSpiceTrackChanged(
		AARPlayerStateBase* SourcePlayerState,
		FGameplayTag SourceCharacterTag,
		float NewMaxSpiceValue,
		float OldMaxSpiceValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|UI|Spice Track")
	void BP_OnInvaderWidgetCharacterCursorChanged(
		AARPlayerStateBase* SourcePlayerState,
		FGameplayTag SourceCharacterTag,
		int32 NewCursorTier,
		int32 OldCursorTier);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Invader|UI|Spice Track")
	void BP_OnInvaderWidgetSharedTrackChanged(const TArray<FARInvaderTrackSlotDisplayState>& SharedTrackSlots);

	/** Attempts owning-HUD binding in `NativeConstruct`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track")
	bool bAutoBindOwningInvaderHUDOnConstruct = true;

private:
	void BindInvaderGameStateDelegates();
	void UnbindInvaderGameStateDelegates();
	void RebindTrackedPlayerStateDelegates();
	void UnbindTrackedPlayerStateDelegates();
	void RefreshCachedCharacterStates();
	void RefreshSharedTrackSnapshot();
	void BuildCharacterStateSnapshot(AARPlayerStateBase* PlayerState, FARInvaderSpicyTrackCharacterState& OutState) const;

	UFUNCTION()
	void HandleTrackedPlayersChanged();

	UFUNCTION()
	void HandleSharedTrackChanged();

	UFUNCTION()
	void HandleTrackedPlayerCurrentCharacterTagChanged(FGameplayTag NewCharacterTag, FGameplayTag OldCharacterTag);

	UFUNCTION()
	void HandleTrackedPlayerSpiceTrackChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, float NewSpiceValue, float OldSpiceValue);

	UFUNCTION()
	void HandleTrackedPlayerMaxSpiceTrackChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, float NewMaxSpiceValue, float OldMaxSpiceValue);

	UFUNCTION()
	void HandleTrackedPlayerCursorChanged(AARPlayerStateBase* SourcePlayerState, FGameplayTag SourceCharacterTag, int32 NewCursorTier, int32 OldCursorTier);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARInvaderHUD> BoundInvaderHUD;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AARInvaderGameState> BoundInvaderGameState;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track", meta = (AllowPrivateAccess = "true"))
	TArray<FARInvaderSpicyTrackCharacterState> CachedCharacterStates;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Alien Ramen|Invader|UI|Spice Track", meta = (AllowPrivateAccess = "true"))
	TArray<FARInvaderTrackSlotDisplayState> CachedSharedTrackSlots;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AARPlayerStateBase>> TrackedPlayerStates;

	UPROPERTY(Transient)
	bool bHasSharedTrackSnapshot = false;

	UPROPERTY(Transient)
	bool bHasInvaderSpicyTrackHUDWidgetInitialized = false;
};
