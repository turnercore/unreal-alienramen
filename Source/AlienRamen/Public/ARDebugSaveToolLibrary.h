#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARDebugSaveToolLibrary.generated.h"

class USaveGame;

USTRUCT(BlueprintType)
struct FARDebugSaveSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	FName SlotName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	int32 SlotNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	int32 SaveVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	int32 CyclesPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	FDateTime LastSavedTime;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	int32 Money = 0;
};

USTRUCT(BlueprintType)
struct FARDebugPlayerLoadoutEdit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	int32 PlayerStateIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	FGameplayTagContainer LoadoutTags;
};

USTRUCT(BlueprintType)
struct FARDebugSaveEdits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetSeenDialogue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	FGameplayTagContainer SeenDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetDialogueFlags = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	FGameplayTagContainer DialogueFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetUnlocks = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	FGameplayTagContainer Unlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetChoices = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	FGameplayTagContainer Choices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetMoney = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	int32 Material = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetCycles = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	int32 Cycles = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	bool bSetMeatAmount = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	int32 MeatAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Debug Save")
	TArray<FARDebugPlayerLoadoutEdit> PlayerLoadoutEdits;
};

USTRUCT(BlueprintType)
struct FARDebugSaveEditResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	int32 ChangedFieldCount = 0;
};

USTRUCT(BlueprintType)
struct FARDebugFieldDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	FName FieldName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	FString FieldType;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	FString ValueAsString;

	UPROPERTY(BlueprintReadOnly, Category = "AR|Debug Save")
	bool bSupportedByTypedEdits = false;
};

UCLASS()
class ALIENRAMEN_API UARDebugSaveToolLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	static FName GetDebugIndexSlotName();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	static FString GetDebugSlotSuffix();

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	static FName NormalizeDebugSlotName(FName DesiredSlotBaseOrSlotName);

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	static bool IsDebugSlotName(FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static bool ListDebugSlots(TArray<FARDebugSaveSlotEntry>& OutSlots, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static bool CreateDebugSave(FName DesiredSlotBase, FName& OutDebugSlotName, USaveGame*& OutSaveGame, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static bool LoadDebugSave(FName DebugSlotName, USaveGame*& OutSaveGame, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static bool SaveDebugSave(FName DebugSlotName, USaveGame* SaveGameObject, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static bool DeleteDebugSave(FName DebugSlotName, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static FARDebugSaveEditResult ApplyDebugSaveEdits(USaveGame* SaveGameObject, const FARDebugSaveEdits& Edits);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static void GetDebugSaveDiagnostics(USaveGame* SaveGameObject, TArray<FARDebugFieldDiagnostic>& OutFields);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	static bool SetUnlocksToAllKnownTags(USaveGame* SaveGameObject, bool bIncludeUnlocksRootTag, int32& OutTagCount, FString& OutError);

private:
	static constexpr int32 DefaultUserIndex = 0;

	static USaveGame* LoadOrCreateDebugIndexSave(FString& OutError);
	static bool SaveDebugIndexSave(USaveGame* DebugIndexSave, FString& OutError);

	static UClass* ResolveDebugIndexClass();
	static UClass* ResolveDebugSaveClass();

	static FProperty* FindPropertyByNamePrefix(const UStruct* StructType, const FString& Prefix);
	static bool GetStructFromObjectProperty(UObject* Object, FName PropName, UScriptStruct*& OutStructType, void*& OutValuePtr);

	static bool ReadSlotEntries(USaveGame* DebugIndexSave, TArray<FARDebugSaveSlotEntry>& OutSlots, FString& OutError);
	static bool UpsertSlotEntry(USaveGame* DebugIndexSave, const FARDebugSaveSlotEntry& Entry, FString& OutError);
	static bool RemoveSlotEntry(USaveGame* DebugIndexSave, FName SlotName, FString& OutError);

	static FARDebugSaveSlotEntry BuildSlotEntryFromSave(USaveGame* SaveGameObject, FName SlotName, int32 SlotNumberHint);
};
