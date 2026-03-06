/**
 * @file ARInvaderSpicyTrackTypes.h
 * @brief Shared Invader spicy-track enums/structs for runtime + data authoring.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARInvaderTypes.h"
#include "ARPlayerTypes.h"
#include "ARInvaderSpicyTrackTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class EARInvaderPlayerColor : uint8
{
	Unknown = 0,
	Red,
	White,
	Blue
};

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
};

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

	// Player currently publishing presence for the active offer session.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown;

	// Optional currently hovered offer (can be empty when only cursor is present).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FGameplayTag HoveredUpgradeTag;

	// Optional currently hovered destination slot for placement affordance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	int32 HoveredDestinationSlot = -1;

	// Optional normalized UI cursor position in [0..1]x[0..1].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	FVector2D CursorNormalized = FVector2D::ZeroVector;

	// Whether CursorNormalized should be interpreted by HUD.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offer")
	bool bHasCursor = false;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARInvaderKillCreditFxEvent
{
	GENERATED_BODY()

	// Player slot whose spice meter received the awarded kill credit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	EARPlayerSlot TargetPlayerSlot = EARPlayerSlot::Unknown;

	// Awarded spice amount after multipliers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	float SpiceGained = 0.0f;

	// New combo count for the target player after this award.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	int32 NewComboCount = 0;

	// Enemy color used when resolving combo wildcard/match behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Credit")
	EAREnemyColor EnemyColor = EAREnemyColor::White;

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
	EARPlayerSlot RequestingPlayerSlot = EARPlayerSlot::Unknown;

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
