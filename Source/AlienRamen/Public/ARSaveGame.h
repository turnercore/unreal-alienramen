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
	static constexpr int32 CurrentSchemaVersion = 1;
	static constexpr int32 MinSupportedSchemaVersion = 1;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Dialogue")
	FGameplayTagContainer SeenDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Dialogue")
	FGameplayTagContainer DialogueFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Progression")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Progression")
	FGameplayTagContainer Choices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Progression")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Progression")
	FARMeatState Meat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Progression")
	int32 Material = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Progression")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Meta")
	FName SaveSlot = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Meta")
	int32 SaveGameVersion = CurrentSchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Meta")
	int32 SaveSlotNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save|Meta")
	FDateTime LastSaved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	FARGameStateSaveData GameStateStruct;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alien Ramen|Save")
	TArray<FARPlayerStateSaveData> PlayerStates;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	FInstancedStruct GetGameStateDataInstancedStruct() const { return GameStateStruct.GameStateData; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	FInstancedStruct GetPlayerStateDataInstancedStructByIndex(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataBySlot(EARPlayerSlot Slot, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Save")
	bool FindPlayerStateDataByIdentity(const FARPlayerIdentity& Identity, FARPlayerStateSaveData& OutData, int32& OutIndex) const;

	int32 ValidateAndSanitize(TArray<FString>* OutWarnings);
};
