/**
 * @file ARInvaderSpicyTrackTypes.h
 * @brief Shared Invader spicy-track enums/structs for runtime + data authoring.
 */
#pragma once

#include "CoreMinimal.h"
#include "ARColorTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARInvaderTypes.h"
#include "ARPlayerTypes.h"
#include "ARInvaderSpicyTrackTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class EARInvaderUpgradeClaimPolicy : uint8
{
	// Once any player activates this upgrade, it is no longer eligible this run.
	SingleTeamClaim = 0,

	// Each player may activate this upgrade once this run.
	PerPlayerClaim = 1,

	// No run-level claim lock.
	Repeatable = 2
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderUpgradeDefRow : public FTableRowBase
{
	GENERATED_BODY()

	// Stable runtime identity for this upgrade.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	FGameplayTag UpgradeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	TSoftObjectPtr<UTexture2D> Icon;

	// Applied to the activating player when this upgrade is used from the track.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TSoftClassPtr<UGameplayEffect> OnActivateGameplayEffect;

	// Optional effect applied while this upgrade remains slotted on the shared track.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TSoftClassPtr<UGameplayEffect> WhileSlottedGameplayEffect;

	// If the current full-blast tier is listed here, this upgrade is not offer-eligible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eligibility")
	TArray<int32> LockedOfferTiers;

	// Required save/unlock tags that must exist on GameState unlocks for this upgrade to be offered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eligibility")
	FGameplayTagContainer RequiredUnlockTags;

	// Team-claim policy after upgrade activation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eligibility")
	EARInvaderUpgradeClaimPolicy ClaimPolicy = EARInvaderUpgradeClaimPolicy::SingleTeamClaim;

	// Team-level prerequisites for offer eligibility (satisfied if either player has activated each tag).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eligibility")
	FGameplayTagContainer RequiredActivatedUpgradesForOffer;

	// Player-level prerequisites for activation eligibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eligibility")
	FGameplayTagContainer RequiredActivatedUpgradesForActivation;

	// Finite number of times this slotted upgrade can be activated before it is consumed.
	// Ignored when bInfiniteActivationUses is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activation", meta = (ClampMin = "1"))
	int32 MaxActivationUses = 1;

	// When true, slot activations never consume/remove this upgrade from the track.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activation")
	bool bInfiniteActivationUses = false;
};

/**
 * Authoritative shared-track slot state replicated by `AARInvaderGameState`.
 *
 * This is the runtime/gameplay representation. UI should usually consume the
 * derived display snapshot instead of resolving presentation data itself.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderTrackSlotState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	int32 SlotIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	FGameplayTag UpgradeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	int32 UpgradeLevel = 1;

	// True once a player has actually activated this slotted upgrade.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bHasBeenActivated = false;

	// Remaining finite activations for this slot. Ignored when bInfiniteUses is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	int32 RemainingActivationUses = 1;

	// If true, this slot never consumes activations and never auto-removes from use count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bInfiniteUses = false;
};

/**
 * UI-facing shared-track slot snapshot derived from `FARInvaderTrackSlotState`.
 *
 * This bundles runtime slot information with resolved presentation data such as
 * the localized upgrade display name, so HUD widgets do not need to duplicate
 * lookup logic.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderTrackSlotDisplayState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	int32 SlotIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	FGameplayTag UpgradeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	int32 UpgradeLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bHasUpgrade = false;

	// Remaining finite activations for this slot. Ignored when bInfiniteUses is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	int32 RemainingActivationUses = 0;

	// If true, this slot can be activated infinitely and RemainingActivationUses is not meaningful.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bInfiniteUses = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderUpgradeOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FGameplayTag UpgradeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	int32 OfferedLevel = 1;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderOfferPresenceState
{
	GENERATED_BODY()

	// Character currently publishing presence for the active offer session.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FGameplayTag PlayerCharacterTag;

	// Optional currently hovered offer (can be empty when only cursor is present).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FGameplayTag HoveredUpgradeTag;

	// Optional currently hovered destination slot for placement affordance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	int32 HoveredDestinationSlot = -1;

	// Optional currently selected offer (for teammate "locked-in" selection preview).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FGameplayTag SelectedUpgradeTag;

	// Optional selected destination slot for placement affordance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	int32 SelectedDestinationSlot = -1;

	// Whether selected fields should be interpreted by HUD.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	bool bHasSelection = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderKillCreditFxEvent
{
	GENERATED_BODY()

	// Character whose spice meter received the awarded kill credit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	FGameplayTag TargetCharacterTag;

	// Awarded spice amount after multipliers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	float SpiceGained = 0.0f;

	// New combo count for the target player after this award.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	int32 NewComboCount = 0;

	// Enemy color used when resolving combo wildcard/match behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	EARAffinityColor EnemyColor = EARAffinityColor::White;

	// Optional enemy identifier for data-driven VFX selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	FGameplayTag EnemyIdentifierTag;

	// Optional world-space FX origin (typically enemy death location).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	FVector EffectOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	bool bHasEffectOrigin = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderFullBlastSessionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	bool bIsActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FGameplayTag RequestingCharacterTag;

	// Full-blast tier at activation time (used for offer-level rolls and top-tier rules).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	int32 ActivationTier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	TArray<FARInvaderUpgradeOffer> Offers;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderLevelOffsetWeight
{
	GENERATED_BODY()

	// Added to base level when rolling offer level (for example -3..+3).
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Spice Track")
	int32 Offset = 0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Spice Track", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};
