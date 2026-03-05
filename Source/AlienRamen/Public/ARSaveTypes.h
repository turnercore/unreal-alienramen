#pragma once
/**
 * @file ARSaveTypes.h
 * @brief ARSaveTypes header for Alien Ramen.
 */

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPlayerStateBase.h"
#include "ARSaveTypes.generated.h"

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

	bool Matches(const FARPlayerIdentity& Other) const
	{
		if (!UniqueNetIdString.IsEmpty() && !Other.UniqueNetIdString.IsEmpty())
		{
			return UniqueNetIdString.Equals(Other.UniqueNetIdString, ESearchCase::CaseSensitive);
		}
		return PlayerSlot != EARPlayerSlot::Unknown && PlayerSlot == Other.PlayerSlot;
	}
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARPlayerStateSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FARPlayerIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EARCharacterChoice CharacterPicked = EARCharacterChoice::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FGameplayTagContainer LoadoutTags;
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
