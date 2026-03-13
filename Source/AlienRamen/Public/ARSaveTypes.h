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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTag MeatType;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 LegacyId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FString UniqueNetIdString;

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

			return UniqueNetIdString.Equals(Other.UniqueNetIdString, ESearchCase::CaseSensitive);
		}
		return PlayerSlot != EARPlayerSlot::Unknown && PlayerSlot == Other.PlayerSlot;
	}
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARPlayerCharacterSaveData
{
	GENERATED_BODY()

	// Which character this nested row belongs to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Canonical gameplay-tag identity for the character this nested player row belongs to."))
	FGameplayTag CharacterTag;

	// Character-specific loadout that is still owned by the player profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Loadout saved for this specific player-character pairing."))
	FGameplayTagContainer LoadoutTags;
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

	// Compatibility mirror of the active character row's loadout.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Compatibility mirror of the currently active character's loadout."))
	FGameplayTagContainer LoadoutTags;

	// Per-character rows that stay under the owning player profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ToolTip = "Per-character save rows owned by this player profile."))
	TArray<FARPlayerCharacterSaveData> CharacterStates;

	FGameplayTag ResolveCurrentCharacterTag() const
	{
		return CurrentCharacterTag.IsValid()
			? ARPlayer::NormalizeCharacterTag(CurrentCharacterTag, Identity.PlayerSlot)
			: ARPlayer::NormalizeCharacterTag(ARPlayer::GetCharacterTagForChoice(CharacterPicked), Identity.PlayerSlot);
	}

	const struct FARPlayerCharacterSaveData* FindCharacterStateData(const FGameplayTag CharacterTag, int32& OutIndex) const;
	struct FARPlayerCharacterSaveData* FindCharacterStateDataMutable(const FGameplayTag CharacterTag, int32& OutIndex);
	struct FARPlayerCharacterSaveData& FindOrAddCharacterStateData(const FGameplayTag CharacterTag);
	void SyncCompatibilityLoadoutFromCurrentCharacter();
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARCharacterHeldShopItemSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Supported carryable class to restore into the character's hands when loading directly back into the shop."))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Energy-drink item tag used when the held item snapshot is an energy drink."))
	FGameplayTag EnergyDrinkItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ToolTip = "Meat color used when the held item snapshot is meat."))
	EARAffinityColor MeatColor = EARAffinityColor::Red;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName SlotName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 SlotNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 SaveVersion = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 CyclesPlayed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FDateTime LastSavedTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int32 Money = 0;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARShopTransientCarryableSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	FGameplayTag EnergyDrinkItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop")
	EARAffinityColor MeatColor = EARAffinityColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Shop", meta = (ClampMin = "1", UIMin = "1"))
	int32 MeatAmount = 1;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARSaveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 ClampedFieldCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	FName SlotName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	int32 SlotNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Save")
	EARSaveResultCode ResultCode = EARSaveResultCode::Unknown;
};

inline const FARPlayerCharacterSaveData* FARPlayerStateSaveData::FindCharacterStateData(const FGameplayTag CharacterTag, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedTag.IsValid())
	{
		return nullptr;
	}

	for (int32 Index = 0; Index < CharacterStates.Num(); ++Index)
	{
		if (CharacterStates[Index].CharacterTag == NormalizedTag)
		{
			OutIndex = Index;
			return &CharacterStates[Index];
		}
	}

	return nullptr;
}

inline FARPlayerCharacterSaveData* FARPlayerStateSaveData::FindCharacterStateDataMutable(const FGameplayTag CharacterTag, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedTag.IsValid())
	{
		return nullptr;
	}

	for (int32 Index = 0; Index < CharacterStates.Num(); ++Index)
	{
		if (CharacterStates[Index].CharacterTag == NormalizedTag)
		{
			OutIndex = Index;
			return &CharacterStates[Index];
		}
	}

	return nullptr;
}

inline FARPlayerCharacterSaveData& FARPlayerStateSaveData::FindOrAddCharacterStateData(const FGameplayTag CharacterTag)
{
	int32 ExistingIndex = INDEX_NONE;
	if (FARPlayerCharacterSaveData* Existing = FindCharacterStateDataMutable(CharacterTag, ExistingIndex))
	{
		return *Existing;
	}

	FARPlayerCharacterSaveData& Added = CharacterStates.AddDefaulted_GetRef();
	Added.CharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag, Identity.PlayerSlot);
	return Added;
}

inline void FARPlayerStateSaveData::SyncCompatibilityLoadoutFromCurrentCharacter()
{
	LoadoutTags.Reset();

	const FGameplayTag ActiveCharacterTag = ResolveCurrentCharacterTag();
	int32 CharacterIndex = INDEX_NONE;
	if (const FARPlayerCharacterSaveData* CharacterState = FindCharacterStateData(ActiveCharacterTag, CharacterIndex))
	{
		LoadoutTags = CharacterState->LoadoutTags;
	}

	CharacterPicked = ARPlayer::GetCharacterChoiceForTag(ActiveCharacterTag);
	CurrentCharacterTag = ActiveCharacterTag;
}
