#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARSaveTypes.h"
#include "ARSaveGame.generated.h"

UCLASS(BlueprintType)
class ALIENRAMEN_API UARSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 3;
	static constexpr int32 MinSupportedSchemaVersion = 3;

	UARSaveGame();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static int32 GetCurrentSchemaVersion() { return CurrentSchemaVersion; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static int32 GetMinSupportedSchemaVersion() { return MinSupportedSchemaVersion; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save|Meta")
	static bool IsSchemaVersionSupported(int32 InSchemaVersion)
	{
		return InSchemaVersion >= MinSupportedSchemaVersion && InSchemaVersion <= CurrentSchemaVersion;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 Scrap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Progression")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	FName SaveSlot = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveGameVersion = CurrentSchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveSlotNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	FDateTime LastSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FARPlayerStateSaveData> PlayerStates;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataBySlot(EARPlayerSlot Slot, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	int32 ValidateAndSanitize(TArray<FString>* OutWarnings);
};
