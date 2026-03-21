/**
 * @file ARSaveTypes.h
 * @brief ARSaveTypes header for Alien Ramen.
 */
#pragma once

#include "ARColorTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "ARPlayerTypes.h"
#include "ARShopRamenTypes.h"
#include "ARSaveTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EARSaveResultCode : uint8
{
	Success = 0,
	AuthorityRequired,
	NoWorld,
	InProgress,
	Throttled,
	ValidationFailed,
	NotFound,
	Unknown
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARMeatTypeAmount
{
	GENERATED_BODY()

	/** Meat type tag (e.g., Item.Meat.Red). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTag MeatType;

	/** Amount stored for this meat type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Amount = 0;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARMeatState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 RedAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 BlueAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 WhiteAmount = 0;

	// Bucket used when callers only know an aggregate value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 UnspecifiedAmount = 0;

	// Extensible typed buckets for future meat variants without schema churn.
	// Array shape is replication-friendly; entries are normalized/sorted by MeatType.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FARMeatTypeAmount> AdditionalAmountsByType;

	int32 GetTotalAmount() const
	{
		int32 Total = FMath::Max(0, RedAmount) + FMath::Max(0, BlueAmount) + FMath::Max(0, WhiteAmount) + FMath::Max(0, UnspecifiedAmount);
		for (const FARMeatTypeAmount& Entry : AdditionalAmountsByType)
		{
			Total += FMath::Max(0, Entry.Amount);
		}
		return Total;
	}

	void SetTotalAsUnspecified(const int32 InTotalAmount)
	{
		RedAmount = 0;
		BlueAmount = 0;
		WhiteAmount = 0;
		AdditionalAmountsByType.Reset();
		UnspecifiedAmount = FMath::Max(0, InTotalAmount);
	}

	void NormalizeAdditionalAmounts()
	{
		TMap<FGameplayTag, int32> Aggregated;
		for (const FARMeatTypeAmount& Entry : AdditionalAmountsByType)
		{
			if (!Entry.MeatType.IsValid())
			{
				continue;
			}

			const int32 SanitizedAmount = FMath::Max(0, Entry.Amount);
			if (SanitizedAmount <= 0)
			{
				continue;
			}

			Aggregated.FindOrAdd(Entry.MeatType) += SanitizedAmount;
		}

		AdditionalAmountsByType.Reset(Aggregated.Num());
		for (const TPair<FGameplayTag, int32>& Pair : Aggregated)
		{
			FARMeatTypeAmount Entry;
			Entry.MeatType = Pair.Key;
			Entry.Amount = Pair.Value;
			AdditionalAmountsByType.Add(Entry);
		}

		AdditionalAmountsByType.Sort([](const FARMeatTypeAmount& A, const FARMeatTypeAmount& B)
		{
			return A.MeatType.ToString() < B.MeatType.ToString();
		});
	}
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARPlayerIdentity
{
	GENERATED_BODY()

	/** Legacy numeric id used only for older saves; online ids are preferred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 LegacyId = 0;

	/** Display name at time of save (for UI/debug). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FText DisplayName;

	/**
	 * Shared-account disambiguator:
	 * - false = primary profile for this online id
	 * - true  = secondary couch profile for this online id
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSharedOnlineIdSecondaryProfile = false;

	/** Platform-specific unique id string (primary identity key). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FString UniqueNetIdString;

	/** Unique id type (e.g., Steam, EOS); used with UniqueNetIdString. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FString UniqueNetIdType;

	bool HasStrictOnlineIdentity() const
	{
		if (UniqueNetIdString.IsEmpty() || UniqueNetIdType.IsEmpty())
		{
			return false;
		}

		return !UniqueNetIdType.Equals(TEXT("NULL"), ESearchCase::IgnoreCase)
			&& !UniqueNetIdType.Equals(TEXT("INVALID"), ESearchCase::IgnoreCase)
			&& !UniqueNetIdType.Equals(TEXT("UNSET"), ESearchCase::IgnoreCase);
	}

	bool Matches(const FARPlayerIdentity& Other) const
	{
		if (!UniqueNetIdString.IsEmpty() && !Other.UniqueNetIdString.IsEmpty())
		{
			if (!UniqueNetIdType.IsEmpty() && !Other.UniqueNetIdType.IsEmpty()
				&& !UniqueNetIdType.Equals(Other.UniqueNetIdType, ESearchCase::CaseSensitive))
			{
				return false;
			}

			if (!UniqueNetIdString.Equals(Other.UniqueNetIdString, ESearchCase::CaseSensitive))
			{
				return false;
			}

			return bSharedOnlineIdSecondaryProfile == Other.bSharedOnlineIdSecondaryProfile;
		}

		// Legacy fallback path for non-online identities.
		return LegacyId > 0 && Other.LegacyId > 0 && LegacyId == Other.LegacyId;
	}

};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARPlayerStateSaveData
{
	GENERATED_BODY()

	// Identity used to match this save row back to a runtime PlayerState.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Identity used to match this save row to a runtime player."))
	FARPlayerIdentity Identity;

	// Canonical active character for this player row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Canonical gameplay-tag identity for the player's currently active character."))
	FGameplayTag CurrentCharacterTag;

	// Legacy compatibility mirror for existing Blueprints.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Compatibility mirror of CurrentCharacterTag for existing Blueprint logic."))
	EARCharacterChoice CharacterPicked = EARCharacterChoice::None;

	// Player-owned preference, not character-owned state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Whether this player's dialogue should auto-advance when possible."))
	bool bDialogueAutoAdvanceEnabled = false;

	// Player-owned progression flags keyed by player identity rather than by character.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Player-owned progression tags that follow this player identity regardless of active character."))
	FGameplayTagContainer ProgressionTags;

		FGameplayTag ResolveCurrentCharacterTag() const
		{
			return CurrentCharacterTag.IsValid()
				? ARPlayer::NormalizeCharacterTag(CurrentCharacterTag)
				: ARPlayer::NormalizeCharacterTag(ARPlayer::GetCharacterTagForChoice(CharacterPicked));
		}

	void SyncCharacterSelectionFromCurrentTag();
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCharacterHeldShopItemSnapshot
{
	GENERATED_BODY()

	/** Actor class to respawn into hands when re-entering shop (must be a supported carryable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Supported carryable class to restore into the character's hands when loading directly back into the shop."))
	TSoftClassPtr<AActor> ActorClass;

	/** Energy drink identity when the held item is a drink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Energy-drink item tag used when the held item snapshot is an energy drink."))
	FGameplayTag EnergyDrinkItemTag;

	/** Meat color when the held item is meat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Meat color used when the held item snapshot is meat."))
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	/** Meat type tag when the held item is meat (Item.Meat.*). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (Categories = "Item.Meat", ToolTip = "Meat definition tag used when the held item snapshot is meat."))
	FGameplayTag MeatTag;

	/** Meat quality tier when the held item is meat (defaults to Average/Standard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Meat quality tier used for value scaling when this held-item snapshot is meat."))
	EARVendingQualityTier MeatQualityTier = EARVendingQualityTier::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ClampMin = "1", UIMin = "1", ToolTip = "Meat amount used when the held item snapshot is meat."))
	int32 MeatAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Bowl recipe snapshot used when the held item snapshot is a ramen bowl."))
	FARRamenBowlSpec BowlSpec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ClampMin = "0", UIMin = "0", ToolTip = "How far through the bowl fill order the restored bowl has progressed."))
	int32 BowlFillStep = 0;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCharacterShopSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "True when the character transform should be restored on direct load back into the shop."))
	bool bHasCharacterTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Character transform to restore when loading directly back into the saved shop state."))
	FTransform CharacterTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "True when the character should restore a supported held shop item on direct load back into the shop."))
	bool bHasHeldItem = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Snapshot for the held supported shop item restored on direct load back into the shop."))
	FARCharacterHeldShopItemSnapshot HeldItem;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARSaveSlotDescriptor
{
	GENERATED_BODY()

	/** Base slot name (without revision suffix). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName SlotName = NAME_None;

	/** Zero-based save revision number used as the on-disk slot suffix (e.g., \"Base__0\"). Not a human-facing slot index. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Zero-based save revision number used as the on-disk slot suffix (e.g., \"Base__0\"). Not a human-facing slot index."))
	int32 SlotNumber = 0;

	/** Save schema version recorded for this slot revision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 SaveVersion = 0;

	/** Cycles completed at time of save (for UI sorting/filters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 CyclesPlayed = 0;

	/** Timestamp of this revision (UTC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FDateTime LastSavedTime;

	/** Money recorded for preview cards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Money = 0;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopTransientCarryableSnapshot
{
	GENERATED_BODY()

	/** Actor class to respawn as a loose carryable on shop reload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	TSoftClassPtr<AActor> ActorClass;

	/** World transform to spawn the carryable at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	FTransform WorldTransform = FTransform::Identity;

	/** Item identity when this snapshot represents an energy drink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	FGameplayTag EnergyDrinkItemTag;

	/** Meat color when this snapshot represents meat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	/** Meat definition tag when this snapshot represents meat (Item.Meat.*). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (Categories = "Item.Meat"))
	FGameplayTag MeatTag;

	/** Meat quality tier when this snapshot represents meat (defaults to Average/Standard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	EARVendingQualityTier MeatQualityTier = EARVendingQualityTier::Standard;

	/** Meat amount when this snapshot represents meat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmount = 1;

	/** Bowl recipe snapshot when this snapshot represents a ramen bowl. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	FARRamenBowlSpec BowlSpec;

	/** How far through bowl fill order this loose bowl had progressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ClampMin = "0", UIMin = "0"))
	int32 BowlFillStep = 0;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARSaveResult
{
	GENERATED_BODY()

	/** True when the save operation succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	bool bSuccess = false;

	/** Error message when bSuccess is false (or empty on success). */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	FString Error;

	/** Number of fields clamped/sanitized during save/load. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 ClampedFieldCount = 0;

	/** Slot base name involved in this result (if any). */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	FName SlotName = NAME_None;

	/** Slot revision number involved in this result (if any). */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 SlotNumber = 0;

	/** Coded result reason for diagnostics/UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	EARSaveResultCode ResultCode = EARSaveResultCode::Unknown;
};

inline void FARPlayerStateSaveData::SyncCharacterSelectionFromCurrentTag()
{
	const FGameplayTag ActiveCharacterTag = ResolveCurrentCharacterTag();
	CharacterPicked = ARPlayer::GetCharacterChoiceForTag(ActiveCharacterTag);
	CurrentCharacterTag = ActiveCharacterTag;
}
