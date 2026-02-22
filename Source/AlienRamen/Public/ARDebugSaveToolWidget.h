#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARDebugSaveToolLibrary.h"
#include "ARDebugSaveToolWidget.generated.h"

class USaveGame;

UCLASS(Abstract, BlueprintType, Blueprintable)
class ALIENRAMEN_API UARDebugSaveToolWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	bool RefreshSlots();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	bool CreateAndLoadSlot(FName DesiredSlotBase);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	bool LoadSlot(FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	FARDebugSaveEditResult ApplyEditsToCurrentSave(const FARDebugSaveEdits& Edits, bool bSaveAfterApply = true);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	bool SaveCurrentSlot();

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	bool DeleteSlot(FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "Alien Ramen|Debug Save")
	void GetCurrentDiagnostics(TArray<FARDebugFieldDiagnostic>& OutFields) const;

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	FName GetCurrentSlotName() const { return CurrentSlotName; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	USaveGame* GetCurrentSaveObject() const { return CurrentSaveObject; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	TArray<FARDebugSaveSlotEntry> GetAvailableSlots() const { return AvailableSlots; }

	UFUNCTION(BlueprintPure, Category = "Alien Ramen|Debug Save")
	FString GetLastError() const { return LastError; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Debug Save")
	void OnDebugSlotsRefreshed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Debug Save")
	void OnDebugSaveLoaded(FName LoadedSlotName);

	UFUNCTION(BlueprintImplementableEvent, Category = "Alien Ramen|Debug Save")
	void OnDebugSaveError(const FString& ErrorText);

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Debug Save")
	TArray<FARDebugSaveSlotEntry> AvailableSlots;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Debug Save")
	FName CurrentSlotName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Debug Save")
	TObjectPtr<USaveGame> CurrentSaveObject;

	UPROPERTY(BlueprintReadOnly, Category = "Alien Ramen|Debug Save")
	FString LastError;
};
