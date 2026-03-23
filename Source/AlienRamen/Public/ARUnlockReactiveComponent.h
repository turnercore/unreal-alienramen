/**
 * @file ARUnlockReactiveComponent.h
 * @brief Unlock-driven reactive component for placed actors and Blueprints.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "ARUnlockReactiveComponent.generated.h"

class AARGameStateBase;
class UPrimitiveComponent;

/**
 * Replays lock/unlock + ordered upgrade callbacks from GameState unlock tags.
 *
 * Runtime source of truth remains AARGameStateBase unlock replication. This component only
 * evaluates authored tag requirements and forwards deterministic Blueprint events.
 */
UCLASS(ClassGroup=(AR), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ALIENRAMEN_API UARUnlockReactiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UARUnlockReactiveComponent();

	/** Base gate tags required for unlocked behavior (empty means always unlocked). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Unlock", meta = (ToolTip = "All tags required for this actor to be considered unlocked. Empty means always unlocked."))
	FGameplayTagContainer RequiredUnlockTags;

	/** Ordered upgrade tags to replay when active on GameState unlocks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Unlock", meta = (ToolTip = "Upgrade tags checked independently and replayed in authored order when present in GameState unlocks."))
	TArray<FGameplayTag> OrderedUpgradeTags;

	/**
	 * Optional explicit primitive components for SetOwnerFullyDisabled toggling.
	 * If empty, helper falls back to all primitive components on owner.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alien Ramen|Unlock", meta = (ToolTip = "Optional explicit primitive components to toggle in SetOwnerFullyDisabled. Empty uses all owner primitive components."))
	TArray<TObjectPtr<UPrimitiveComponent>> DisableCollisionComponents;

	/** Runtime unlocked state from latest refresh pass. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Unlock|Runtime")
	bool bIsUnlocked = false;

	/** Runtime active authored upgrade tags present in current GameState unlock set. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Unlock|Runtime")
	FGameplayTagContainer ActiveUpgradeTags;

	/** Refreshes this component state from AARGameStateBase unlock tags. */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Unlock")
	void RefreshFromGameState();

	/** True when latest refresh resolved as unlocked. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Unlock")
	bool IsUnlocked() const { return bIsUnlocked; }

	/** True when a tag is currently active in ActiveUpgradeTags. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Unlock")
	bool HasUpgradeTag(FGameplayTag UpgradeTag) const;

	/** Returns active upgrade tags from latest refresh pass. */
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Unlock")
	const FGameplayTagContainer& GetActiveUpgradeTags() const { return ActiveUpgradeTags; }

	/**
	 * Convenience helper to hide/unhide owner and disable/restore owner primitive collisions.
	 * Disabled actors no longer render or respond to traces.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Unlock")
	void SetOwnerFullyDisabled(bool bDisabled);

	/** Called when base required unlock tags are not satisfied. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Unlock")
	void OnLocked();

	/** Called when base required unlock tags are satisfied. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Unlock")
	void OnUnlocked();

	/** Called for each active authored upgrade tag in authored order after OnUnlocked. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Unlock")
	void OnUpgrade(FGameplayTag UpgradeTag);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindToGameState();
	void UnbindFromGameState();
	AARGameStateBase* ResolveGameState() const;
	void ApplyLockedState();
	void ApplyUnlockedAndUpgradeState(const FGameplayTagContainer& Unlocks);
	void GatherTargetPrimitiveComponents(TArray<UPrimitiveComponent*>& OutComponents) const;

	UFUNCTION()
	void HandleHydratedFromSave();

	UFUNCTION()
	void HandleUnlocksChanged(FGameplayTagContainer NewUnlocks, FGameplayTagContainer OldUnlocks);

	/** Bound/cached game state used for delegate lifetime management. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AARGameStateBase> BoundGameState;

	/** Cached owner collision enable state captured when SetOwnerFullyDisabled(true) is first applied. */
	bool bCachedOwnerCollisionEnabled = true;

	/** True while collision cache for SetOwnerFullyDisabled is active. */
	bool bHasCachedDisableState = false;

	/** Cached primitive collision modes captured when SetOwnerFullyDisabled(true) is first applied. */
	TMap<TObjectPtr<UPrimitiveComponent>, ECollisionEnabled::Type> CachedComponentCollisionModes;
};
