#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPlayerStateBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ARSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARMeatState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 RedAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 BlueAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 WhiteAmount = 0;

	// Bucket used when callers only know an aggregate value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 UnspecifiedAmount = 0;

	// Extensible typed buckets for future meat variants without schema churn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	TMap<FGameplayTag, int32> AdditionalAmountsByType;

	int32 GetTotalAmount() const
	{
		int32 Total = FMath::Max(0, RedAmount) + FMath::Max(0, BlueAmount) + FMath::Max(0, WhiteAmount) + FMath::Max(0, UnspecifiedAmount);
		for (const TPair<FGameplayTag, int32>& Pair : AdditionalAmountsByType)
		{
			Total += FMath::Max(0, Pair.Value);
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
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARPlayerIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 LegacyId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	EARPlayerSlot PlayerSlot = EARPlayerSlot::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FARPlayerIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	EARCharacterChoice CharacterPicked = EARCharacterChoice::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FGameplayTagContainer LoadoutTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FInstancedStruct PlayerStateData;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARGameStateSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FInstancedStruct GameStateData;
};

USTRUCT(BlueprintType)
struct ALIENRAMEN_API FARSaveSlotDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FName SlotName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 SlotNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 SaveVersion = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	int32 CyclesPlayed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FDateTime LastSavedTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
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
};
