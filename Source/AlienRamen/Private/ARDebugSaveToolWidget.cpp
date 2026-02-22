#include "ARDebugSaveToolWidget.h"

#include "ARLog.h"

bool UARDebugSaveToolWidget::RefreshSlots()
{
	LastError.Reset();
	const bool bOk = UARDebugSaveToolLibrary::ListDebugSlots(AvailableSlots, LastError);
	if (!bOk)
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool] RefreshSlots failed: %s"), *LastError);
		OnDebugSaveError(LastError);
		return false;
	}

	OnDebugSlotsRefreshed();
	return true;
}

bool UARDebugSaveToolWidget::CreateAndLoadSlot(FName DesiredSlotBase)
{
	LastError.Reset();
	USaveGame* NewSave = nullptr;
	FName NewSlot = NAME_None;
	if (!UARDebugSaveToolLibrary::CreateDebugSave(DesiredSlotBase, NewSlot, NewSave, LastError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool] CreateAndLoadSlot failed: %s"), *LastError);
		OnDebugSaveError(LastError);
		return false;
	}

	CurrentSlotName = NewSlot;
	CurrentSaveObject = NewSave;
	RefreshSlots();
	OnDebugSaveLoaded(CurrentSlotName);
	return true;
}

bool UARDebugSaveToolWidget::LoadSlot(FName SlotName)
{
	LastError.Reset();
	USaveGame* Loaded = nullptr;
	if (!UARDebugSaveToolLibrary::LoadDebugSave(SlotName, Loaded, LastError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool] LoadSlot failed: %s"), *LastError);
		OnDebugSaveError(LastError);
		return false;
	}

	CurrentSlotName = UARDebugSaveToolLibrary::NormalizeDebugSlotName(SlotName);
	CurrentSaveObject = Loaded;
	OnDebugSaveLoaded(CurrentSlotName);
	return true;
}

FARDebugSaveEditResult UARDebugSaveToolWidget::ApplyEditsToCurrentSave(const FARDebugSaveEdits& Edits, bool bSaveAfterApply)
{
	FARDebugSaveEditResult Result = UARDebugSaveToolLibrary::ApplyDebugSaveEdits(CurrentSaveObject, Edits);
	if (!Result.bSuccess)
	{
		LastError = Result.Error;
		OnDebugSaveError(LastError);
		return Result;
	}

	if (bSaveAfterApply)
	{
		if (!SaveCurrentSlot())
		{
			Result.bSuccess = false;
			Result.Error = LastError;
		}
	}

	return Result;
}

bool UARDebugSaveToolWidget::SaveCurrentSlot()
{
	LastError.Reset();
	if (!CurrentSaveObject)
	{
		LastError = TEXT("No current save object is loaded.");
		OnDebugSaveError(LastError);
		return false;
	}

	if (CurrentSlotName.IsNone())
	{
		LastError = TEXT("No current slot name is set.");
		OnDebugSaveError(LastError);
		return false;
	}

	if (!UARDebugSaveToolLibrary::SaveDebugSave(CurrentSlotName, CurrentSaveObject, LastError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool] SaveCurrentSlot failed: %s"), *LastError);
		OnDebugSaveError(LastError);
		return false;
	}

	RefreshSlots();
	return true;
}

bool UARDebugSaveToolWidget::DeleteSlot(FName SlotName)
{
	LastError.Reset();
	if (!UARDebugSaveToolLibrary::DeleteDebugSave(SlotName, LastError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool] DeleteSlot failed: %s"), *LastError);
		OnDebugSaveError(LastError);
		return false;
	}

	const FName Normalized = UARDebugSaveToolLibrary::NormalizeDebugSlotName(SlotName);
	if (CurrentSlotName == Normalized)
	{
		CurrentSlotName = NAME_None;
		CurrentSaveObject = nullptr;
	}

	RefreshSlots();
	return true;
}

void UARDebugSaveToolWidget::GetCurrentDiagnostics(TArray<FARDebugFieldDiagnostic>& OutFields) const
{
	UARDebugSaveToolLibrary::GetDebugSaveDiagnostics(CurrentSaveObject, OutFields);
}
