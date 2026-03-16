/**
 * @file ARLoadoutTypes.h
 * @brief Shared loadout authoring row types for ship and hat data tables.
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARLoadoutTypes.generated.h"

class APawn;
class UARWeaponDefinition;
class UGameplayAbility;
class UGameplayEffect;

/**
 * Data table row for hat loadout entries (`Unlock.Hat.*`).
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARHatDefRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Localized hat name shown in menus and inspectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Hat", meta = (DisplayName = "Display Name", ToolTip = "Localized hat display name used by UI surfaces."))
	FText DisplayName;

	/** Localized hat flavor/summary text shown in menus and inspectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Hat", meta = (ToolTip = "Localized hat description used by UI surfaces."))
	FText Description;
};

/**
 * Data table row for ship loadout entries (`Unlock.Ship.*`).
 *
 * These field names intentionally match reflective runtime readers in player/game-mode systems.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShipDefRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Localized ship name shown in menus and inspectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Identity", meta = (DisplayName = "Display Name", ToolTip = "Localized ship display name used by UI surfaces."))
	FText DisplayName;

	/** Localized ship flavor/summary text shown in menus and inspectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Identity", meta = (ToolTip = "Localized ship description used by UI surfaces."))
	FText Description;

	/** Baseline gameplay-effect class applied for this ship. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Gameplay", meta = (ToolTip = "Baseline gameplay effect applied from this ship row (for example core ship stats)."))
	TSoftClassPtr<UGameplayEffect> Stats;

	/** Pawn class used when spawning this ship in Scrapyard mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Pawn", meta = (ToolTip = "Soft pawn class used by Scrapyard mode for this ship."))
	TSoftClassPtr<APawn> ScrapyardPawnClass;

	/** Optional mode-agnostic pawn fallback used when a mode-specific class is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Pawn", meta = (ToolTip = "Optional fallback pawn class used when a mode-specific pawn class is unavailable."))
	TSoftClassPtr<APawn> DummyPawnClass;

	/** Pawn class used when spawning this ship in Invader mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Pawn", meta = (ToolTip = "Soft pawn class used by Invader mode for this ship."))
	TSoftClassPtr<APawn> InvaderPawnClass;

	/** Movement behavior tag granted from this ship row (for example `Ship.Movement.*`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Gameplay", meta = (Categories = "Ship.Movement", ToolTip = "Optional movement tag granted by this ship row."))
	FGameplayTag MovementType;

	/** Primary weapon definition used by this ship. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Gameplay", meta = (ToolTip = "Primary weapon definition asset resolved for this ship row."))
	TSoftObjectPtr<UARWeaponDefinition> PrimaryWeapon;

	/** Startup abilities granted when this ship baseline is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Gameplay", meta = (ToolTip = "Gameplay ability classes granted when this ship row is applied."))
	TArray<TSoftClassPtr<UGameplayAbility>> StartupAbilities;

	/** Tags granted as loose runtime tags when this ship row is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Gameplay", meta = (Categories = "Unlock.Ship", ToolTip = "Loose gameplay tags granted when this ship row is applied."))
	FGameplayTagContainer ShipTags;

	/** Startup effects applied when this ship baseline is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Gameplay", meta = (ToolTip = "Gameplay effect classes applied when this ship row is applied."))
	TArray<TSoftClassPtr<UGameplayEffect>> StartupEffects;
};

/**
 * Data table row for shop-playable characters (`DT_Characters` / `DT_ShopCharacters`).
 *
 * Speaker/customer links are intentionally component-driven on the spawned pawn Blueprint
 * (`UParleySpeakerComponent`, `UARCustomerComponent`) and not duplicated on this row.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopCharacterDefRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Canonical character identity tag used for lookup/spawn selection (for example `Parley.Speaker.Brother`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Shop Character", meta = (Categories = "Parley.Speaker", ToolTip = "Character identity tag used to select this row at runtime."))
	FGameplayTag CharacterTag;

	/** Spawnable character Blueprint/class for this character row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Shop Character", meta = (DisplayName = "Blueprint", ToolTip = "Soft pawn class used when spawning this character from DT_Characters."))
	TSoftClassPtr<APawn> CharacterClass;
};
