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
 * Runtime systems resolve this row as FARShipDefRow and consume the canonical pawn fields directly.
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

	/** Pawn class used when spawning this ship in Invader mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Pawn", meta = (ToolTip = "Soft pawn class used by Invader mode for this ship."))
	TSoftClassPtr<APawn> InvaderPawnClass;

	/** Pawn class used when a dummy/preview pawn is required for this ship row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Ship|Pawn", meta = (DisplayName = "Dummy Pawn Class", ToolTip = "Soft pawn class used when a dummy/preview ship pawn is needed for this row."))
	TSoftClassPtr<APawn> DummyPawnClass;

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
 * (`UParleySpeakerComponent`, `UARCustomerComponent`).
 *
 * Identity tag contract:
 * - `CharacterTag` (`Shop.Character.*`)
 * - `CustomerTag` (`Shop.Customer.*`)
 * - `SpeakerTag` (`Parley.Speaker.*`)
 * These tags should mirror only by the first segment under the root
 * (for example `Brother` in `Shop.Character.Brother.*`), while deeper
 * subleafs may diverge per-system.
 */
USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopCharacterDefRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Shop-character identity tag used for lookup/spawn selection (for example `Shop.Character.Brother`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Shop Character", meta = (DisplayName = "Shop Character Tag", Categories = "Shop.Character", ToolTip = "Shop.Character identity tag for this row. Mirror only the first segment under the root (for example Brother) with Shop.Customer and Parley.Speaker tags. Deeper subleafs may differ."))
	FGameplayTag CharacterTag;

	/** Shop-customer identity tag mirrored to CharacterTag/SpeakerTag (for example `Shop.Customer.Brother`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Shop Character", meta = (DisplayName = "Shop Customer Tag", Categories = "Shop.Customer", ToolTip = "Shop.Customer identity tag for this row. Mirror only the first segment under the root (for example Brother) with Shop.Character and Parley.Speaker tags. Deeper subleafs may differ."))
	FGameplayTag CustomerTag;

	/** Dialogue speaker identity tag mirrored to CharacterTag/CustomerTag (for example `Parley.Speaker.Brother`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Shop Character", meta = (DisplayName = "Parley Speaker Tag", Categories = "Parley.Speaker", ToolTip = "Parley.Speaker identity tag for this row. Mirror only the first segment under the root (for example Brother) with Shop.Character and Shop.Customer tags. Deeper subleafs may differ."))
	FGameplayTag SpeakerTag;

	/** Spawnable character Blueprint/class for this character row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Loadout|Shop Character", meta = (DisplayName = "Blueprint", ToolTip = "Soft pawn class used when spawning this character from DT_Characters."))
	TSoftClassPtr<APawn> CharacterClass;
};
