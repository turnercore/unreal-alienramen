#include "ARDebugSaveToolLibrary.h"

#include "ARLog.h"

#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace ARDebugSaveToolInternal
{
	static const TCHAR* DebugIndexClassPath = TEXT("/Game/CodeAlong/Blueprints/SaveSystem/SG_SaveIndex.SG_SaveIndex_C");
	static const TCHAR* DebugSaveClassPath = TEXT("/Game/CodeAlong/Blueprints/SaveSystem/SG_AlienRamenSave.SG_AlienRamenSave_C");
	static const TCHAR* DebugIndexSlot = TEXT("SaveIndexDebug");
	static const TCHAR* DebugSlotSuffix = TEXT("_debug");

	static bool TryGetIntField(const UStruct* StructType, const void* StructData, const FString& FieldPrefix, int32& OutValue)
	{
		OutValue = 0;
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
			{
				OutValue = IntProp->GetPropertyValue_InContainer(StructData);
				return true;
			}
		}

		return false;
	}

	static bool TrySetIntField(const UStruct* StructType, void* StructData, const FString& FieldPrefix, int32 InValue)
	{
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
			{
				IntProp->SetPropertyValue_InContainer(StructData, InValue);
				return true;
			}
		}

		return false;
	}

	static bool TryGetNameField(const UStruct* StructType, const void* StructData, const FString& FieldPrefix, FName& OutValue)
	{
		OutValue = NAME_None;
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				OutValue = NameProp->GetPropertyValue_InContainer(StructData);
				return true;
			}
		}

		return false;
	}

	static bool TrySetNameField(const UStruct* StructType, void* StructData, const FString& FieldPrefix, FName InValue)
	{
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				NameProp->SetPropertyValue_InContainer(StructData, InValue);
				return true;
			}
		}

		return false;
	}

	static bool TryGetDateTimeField(const UStruct* StructType, const void* StructData, const FString& FieldPrefix, FDateTime& OutValue)
	{
		OutValue = FDateTime();
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct == TBaseStructure<FDateTime>::Get())
				{
					const void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(StructData);
					OutValue = *reinterpret_cast<const FDateTime*>(ValuePtr);
					return true;
				}
			}
		}

		return false;
	}

	static bool TrySetDateTimeField(const UStruct* StructType, void* StructData, const FString& FieldPrefix, const FDateTime& InValue)
	{
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct == TBaseStructure<FDateTime>::Get())
				{
					void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(StructData);
					*reinterpret_cast<FDateTime*>(ValuePtr) = InValue;
					return true;
				}
			}
		}

		return false;
	}

	static bool TrySetGameplayTagContainerField(const UStruct* StructType, void* StructData, const FString& FieldPrefix, const FGameplayTagContainer& InValue)
	{
		if (!StructType || !StructData) return false;

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(FieldPrefix)) continue;

			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct == FGameplayTagContainer::StaticStruct())
				{
					void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(StructData);
					*reinterpret_cast<FGameplayTagContainer*>(ValuePtr) = InValue;
					return true;
				}
			}
		}

		return false;
	}

	static bool TrySetIntPropertyOnObject(UObject* Object, const FString& FieldPrefix, int32 InValue)
	{
		if (!Object) return false;
		const UStruct* StructType = Object->GetClass();
		void* StructData = Object;
		return TrySetIntField(StructType, StructData, FieldPrefix, InValue);
	}

	static bool TrySetNamePropertyOnObject(UObject* Object, const FString& FieldPrefix, FName InValue)
	{
		if (!Object) return false;
		const UStruct* StructType = Object->GetClass();
		void* StructData = Object;
		return TrySetNameField(StructType, StructData, FieldPrefix, InValue);
	}

	static bool TrySetDateTimePropertyOnObject(UObject* Object, const FString& FieldPrefix, const FDateTime& InValue)
	{
		if (!Object) return false;
		const UStruct* StructType = Object->GetClass();
		void* StructData = Object;
		return TrySetDateTimeField(StructType, StructData, FieldPrefix, InValue);
	}

	static bool TrySetGameplayTagContainerPropertyOnObject(UObject* Object, const FString& FieldPrefix, const FGameplayTagContainer& InValue)
	{
		if (!Object) return false;
		const UStruct* StructType = Object->GetClass();
		void* StructData = Object;
		return TrySetGameplayTagContainerField(StructType, StructData, FieldPrefix, InValue);
	}

	static bool TryGetIntPropertyOnObject(const UObject* Object, const FString& FieldPrefix, int32& OutValue)
	{
		if (!Object) return false;
		const UStruct* StructType = Object->GetClass();
		const void* StructData = Object;
		return TryGetIntField(StructType, StructData, FieldPrefix, OutValue);
	}

	static bool TryGetDateTimePropertyOnObject(const UObject* Object, const FString& FieldPrefix, FDateTime& OutValue)
	{
		if (!Object) return false;
		const UStruct* StructType = Object->GetClass();
		const void* StructData = Object;
		return TryGetDateTimeField(StructType, StructData, FieldPrefix, OutValue);
	}
}

FName UARDebugSaveToolLibrary::GetDebugIndexSlotName()
{
	return FName(ARDebugSaveToolInternal::DebugIndexSlot);
}

FString UARDebugSaveToolLibrary::GetDebugSlotSuffix()
{
	return FString(ARDebugSaveToolInternal::DebugSlotSuffix);
}

FName UARDebugSaveToolLibrary::NormalizeDebugSlotName(FName DesiredSlotBaseOrSlotName)
{
	FString Slot = DesiredSlotBaseOrSlotName.ToString().TrimStartAndEnd();
	const FString Suffix = GetDebugSlotSuffix();

	if (Slot.IsEmpty())
	{
		Slot = FString::Printf(TEXT("Debug_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}

	if (!Slot.EndsWith(Suffix, ESearchCase::IgnoreCase))
	{
		Slot += Suffix;
	}

	return FName(*Slot);
}

bool UARDebugSaveToolLibrary::IsDebugSlotName(FName SlotName)
{
	const FString NameStr = SlotName.ToString();
	return NameStr.EndsWith(GetDebugSlotSuffix(), ESearchCase::IgnoreCase);
}

UClass* UARDebugSaveToolLibrary::ResolveDebugIndexClass()
{
	static TWeakObjectPtr<UClass> CachedClass;
	if (CachedClass.IsValid()) return CachedClass.Get();

	const FSoftClassPath Path(ARDebugSaveToolInternal::DebugIndexClassPath);
	UClass* Loaded = Path.TryLoadClass<USaveGame>();
	if (!Loaded)
	{
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|Validation] Could not load debug index class '%s'."), ARDebugSaveToolInternal::DebugIndexClassPath);
		return nullptr;
	}

	CachedClass = Loaded;
	return Loaded;
}

UClass* UARDebugSaveToolLibrary::ResolveDebugSaveClass()
{
	static TWeakObjectPtr<UClass> CachedClass;
	if (CachedClass.IsValid()) return CachedClass.Get();

	const FSoftClassPath Path(ARDebugSaveToolInternal::DebugSaveClassPath);
	UClass* Loaded = Path.TryLoadClass<USaveGame>();
	if (!Loaded)
	{
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|Validation] Could not load debug save class '%s'."), ARDebugSaveToolInternal::DebugSaveClassPath);
		return nullptr;
	}

	CachedClass = Loaded;
	return Loaded;
}

USaveGame* UARDebugSaveToolLibrary::LoadOrCreateDebugIndexSave(FString& OutError)
{
	OutError.Reset();
	const FName IndexSlot = GetDebugIndexSlotName();

	if (UGameplayStatics::DoesSaveGameExist(IndexSlot.ToString(), DefaultUserIndex))
	{
		USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(IndexSlot.ToString(), DefaultUserIndex);
		if (Loaded)
		{
			return Loaded;
		}

		OutError = FString::Printf(TEXT("Could not load existing debug index slot '%s'."), *IndexSlot.ToString());
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return nullptr;
	}

	UClass* IndexClass = ResolveDebugIndexClass();
	if (!IndexClass)
	{
		OutError = TEXT("Debug index class is not available.");
		return nullptr;
	}

	USaveGame* NewIndex = UGameplayStatics::CreateSaveGameObject(IndexClass);
	if (!NewIndex)
	{
		OutError = TEXT("Failed to create debug index save object.");
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return nullptr;
	}

	if (!UGameplayStatics::SaveGameToSlot(NewIndex, IndexSlot.ToString(), DefaultUserIndex))
	{
		OutError = FString::Printf(TEXT("Failed to persist new debug index slot '%s'."), *IndexSlot.ToString());
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return nullptr;
	}

	UE_LOG(ARLog, Log, TEXT("[DebugSaveTool] Created new debug index slot '%s'."), *IndexSlot.ToString());
	return NewIndex;
}

bool UARDebugSaveToolLibrary::SaveDebugIndexSave(USaveGame* DebugIndexSave, FString& OutError)
{
	OutError.Reset();
	if (!DebugIndexSave)
	{
		OutError = TEXT("DebugIndexSave is null.");
		return false;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(DebugIndexSave, GetDebugIndexSlotName().ToString(), DefaultUserIndex);
	if (!bSaved)
	{
		OutError = FString::Printf(TEXT("Failed to save debug index slot '%s'."), *GetDebugIndexSlotName().ToString());
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
	}
	return bSaved;
}

FProperty* UARDebugSaveToolLibrary::FindPropertyByNamePrefix(const UStruct* StructType, const FString& Prefix)
{
	if (!StructType) return nullptr;

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop && Prop->GetName().StartsWith(Prefix))
		{
			return Prop;
		}
	}
	return nullptr;
}

bool UARDebugSaveToolLibrary::GetStructFromObjectProperty(UObject* Object, FName PropName, UScriptStruct*& OutStructType, void*& OutValuePtr)
{
	OutStructType = nullptr;
	OutValuePtr = nullptr;
	if (!Object) return false;

	FProperty* Prop = FindPropertyByNamePrefix(Object->GetClass(), PropName.ToString());
	FStructProperty* StructProp = CastField<FStructProperty>(Prop);
	if (!StructProp) return false;

	OutStructType = StructProp->Struct;
	OutValuePtr = StructProp->ContainerPtrToValuePtr<void>(Object);
	return OutStructType && OutValuePtr;
}

bool UARDebugSaveToolLibrary::ReadSlotEntries(USaveGame* DebugIndexSave, TArray<FARDebugSaveSlotEntry>& OutSlots, FString& OutError)
{
	OutSlots.Reset();
	OutError.Reset();

	if (!DebugIndexSave)
	{
		OutError = TEXT("DebugIndexSave is null.");
		return false;
	}

	FProperty* SlotNamesPropRaw = FindPropertyByNamePrefix(DebugIndexSave->GetClass(), TEXT("SlotNames"));
	FArrayProperty* SlotNamesProp = CastField<FArrayProperty>(SlotNamesPropRaw);
	if (!SlotNamesProp)
	{
		OutError = TEXT("SlotNames array property is missing on debug index save.");
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] %s"), *OutError);
		return false;
	}

	FStructProperty* InnerStructProp = CastField<FStructProperty>(SlotNamesProp->Inner);
	if (!InnerStructProp || !InnerStructProp->Struct)
	{
		OutError = TEXT("SlotNames is not an array of structs.");
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] %s"), *OutError);
		return false;
	}

	FScriptArrayHelper Helper(SlotNamesProp, SlotNamesProp->ContainerPtrToValuePtr<void>(DebugIndexSave));
	for (int32 i = 0; i < Helper.Num(); ++i)
	{
		void* Elem = Helper.GetRawPtr(i);
		FARDebugSaveSlotEntry Entry;

		ARDebugSaveToolInternal::TryGetNameField(InnerStructProp->Struct, Elem, TEXT("SlotName"), Entry.SlotName);
		ARDebugSaveToolInternal::TryGetIntField(InnerStructProp->Struct, Elem, TEXT("SlotNumber"), Entry.SlotNumber);
		ARDebugSaveToolInternal::TryGetIntField(InnerStructProp->Struct, Elem, TEXT("SaveVersion"), Entry.SaveVersion);
		ARDebugSaveToolInternal::TryGetIntField(InnerStructProp->Struct, Elem, TEXT("CyclesPlayed"), Entry.CyclesPlayed);
		ARDebugSaveToolInternal::TryGetDateTimeField(InnerStructProp->Struct, Elem, TEXT("LastSavedTime"), Entry.LastSavedTime);
		ARDebugSaveToolInternal::TryGetIntField(InnerStructProp->Struct, Elem, TEXT("Money"), Entry.Money);

		if (Entry.SlotName.IsNone()) continue;
		if (!IsDebugSlotName(Entry.SlotName)) continue;

		OutSlots.Add(Entry);
	}

	return true;
}

bool UARDebugSaveToolLibrary::UpsertSlotEntry(USaveGame* DebugIndexSave, const FARDebugSaveSlotEntry& Entry, FString& OutError)
{
	OutError.Reset();
	if (!DebugIndexSave)
	{
		OutError = TEXT("DebugIndexSave is null.");
		return false;
	}

	FProperty* SlotNamesPropRaw = FindPropertyByNamePrefix(DebugIndexSave->GetClass(), TEXT("SlotNames"));
	FArrayProperty* SlotNamesProp = CastField<FArrayProperty>(SlotNamesPropRaw);
	if (!SlotNamesProp)
	{
		OutError = TEXT("SlotNames array property is missing on debug index save.");
		return false;
	}

	FStructProperty* InnerStructProp = CastField<FStructProperty>(SlotNamesProp->Inner);
	if (!InnerStructProp || !InnerStructProp->Struct)
	{
		OutError = TEXT("SlotNames is not an array of structs.");
		return false;
	}

	FScriptArrayHelper Helper(SlotNamesProp, SlotNamesProp->ContainerPtrToValuePtr<void>(DebugIndexSave));

	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Helper.Num(); ++i)
	{
		void* Elem = Helper.GetRawPtr(i);
		FName ExistingName;
		if (ARDebugSaveToolInternal::TryGetNameField(InnerStructProp->Struct, Elem, TEXT("SlotName"), ExistingName) && ExistingName == Entry.SlotName)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		FoundIndex = Helper.AddValue();
	}

	void* TargetElem = Helper.GetRawPtr(FoundIndex);
	ARDebugSaveToolInternal::TrySetNameField(InnerStructProp->Struct, TargetElem, TEXT("SlotName"), Entry.SlotName);
	ARDebugSaveToolInternal::TrySetIntField(InnerStructProp->Struct, TargetElem, TEXT("SlotNumber"), Entry.SlotNumber);
	ARDebugSaveToolInternal::TrySetIntField(InnerStructProp->Struct, TargetElem, TEXT("SaveVersion"), Entry.SaveVersion);
	ARDebugSaveToolInternal::TrySetIntField(InnerStructProp->Struct, TargetElem, TEXT("CyclesPlayed"), Entry.CyclesPlayed);
	ARDebugSaveToolInternal::TrySetDateTimeField(InnerStructProp->Struct, TargetElem, TEXT("LastSavedTime"), Entry.LastSavedTime);
	ARDebugSaveToolInternal::TrySetIntField(InnerStructProp->Struct, TargetElem, TEXT("Money"), Entry.Money);

	return true;
}

bool UARDebugSaveToolLibrary::RemoveSlotEntry(USaveGame* DebugIndexSave, FName SlotName, FString& OutError)
{
	OutError.Reset();
	if (!DebugIndexSave)
	{
		OutError = TEXT("DebugIndexSave is null.");
		return false;
	}

	FProperty* SlotNamesPropRaw = FindPropertyByNamePrefix(DebugIndexSave->GetClass(), TEXT("SlotNames"));
	FArrayProperty* SlotNamesProp = CastField<FArrayProperty>(SlotNamesPropRaw);
	if (!SlotNamesProp)
	{
		OutError = TEXT("SlotNames array property is missing on debug index save.");
		return false;
	}

	FStructProperty* InnerStructProp = CastField<FStructProperty>(SlotNamesProp->Inner);
	if (!InnerStructProp || !InnerStructProp->Struct)
	{
		OutError = TEXT("SlotNames is not an array of structs.");
		return false;
	}

	FScriptArrayHelper Helper(SlotNamesProp, SlotNamesProp->ContainerPtrToValuePtr<void>(DebugIndexSave));
	for (int32 i = 0; i < Helper.Num(); ++i)
	{
		void* Elem = Helper.GetRawPtr(i);
		FName ExistingName;
		if (ARDebugSaveToolInternal::TryGetNameField(InnerStructProp->Struct, Elem, TEXT("SlotName"), ExistingName) && ExistingName == SlotName)
		{
			Helper.RemoveValues(i, 1);
			return true;
		}
	}

	return true;
}

FARDebugSaveSlotEntry UARDebugSaveToolLibrary::BuildSlotEntryFromSave(USaveGame* SaveGameObject, FName SlotName, int32 SlotNumberHint)
{
	FARDebugSaveSlotEntry Entry;
	Entry.SlotName = SlotName;
	Entry.SlotNumber = SlotNumberHint;
	Entry.LastSavedTime = FDateTime::UtcNow();

	ARDebugSaveToolInternal::TryGetIntPropertyOnObject(SaveGameObject, TEXT("SaveSlotNumber"), Entry.SlotNumber);
	ARDebugSaveToolInternal::TryGetIntPropertyOnObject(SaveGameObject, TEXT("SaveGameVersion"), Entry.SaveVersion);
	ARDebugSaveToolInternal::TryGetIntPropertyOnObject(SaveGameObject, TEXT("Cycles"), Entry.CyclesPlayed);
	ARDebugSaveToolInternal::TryGetIntPropertyOnObject(SaveGameObject, TEXT("Money"), Entry.Money);
	ARDebugSaveToolInternal::TryGetDateTimePropertyOnObject(SaveGameObject, TEXT("LastSaved"), Entry.LastSavedTime);

	return Entry;
}

bool UARDebugSaveToolLibrary::ListDebugSlots(TArray<FARDebugSaveSlotEntry>& OutSlots, FString& OutError)
{
	USaveGame* DebugIndex = LoadOrCreateDebugIndexSave(OutError);
	if (!DebugIndex)
	{
		return false;
	}

	if (!ReadSlotEntries(DebugIndex, OutSlots, OutError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|IO] Failed reading debug slots: %s"), *OutError);
		return false;
	}

	return true;
}

bool UARDebugSaveToolLibrary::CreateDebugSave(FName DesiredSlotBase, FName& OutDebugSlotName, USaveGame*& OutSaveGame, FString& OutError)
{
	OutDebugSlotName = NormalizeDebugSlotName(DesiredSlotBase);
	OutSaveGame = nullptr;
	OutError.Reset();

	UClass* SaveClass = ResolveDebugSaveClass();
	if (!SaveClass)
	{
		OutError = TEXT("Debug save class is unavailable.");
		return false;
	}

	FString IndexError;
	USaveGame* DebugIndex = LoadOrCreateDebugIndexSave(IndexError);
	if (!DebugIndex)
	{
		OutError = IndexError;
		return false;
	}

	TArray<FARDebugSaveSlotEntry> ExistingSlots;
	ReadSlotEntries(DebugIndex, ExistingSlots, IndexError);

	int32 MaxSlotNumber = 0;
	int32 ExistingSlotNumber = INDEX_NONE;
	for (const FARDebugSaveSlotEntry& Existing : ExistingSlots)
	{
		MaxSlotNumber = FMath::Max(MaxSlotNumber, Existing.SlotNumber);
		if (Existing.SlotName == OutDebugSlotName)
		{
			ExistingSlotNumber = Existing.SlotNumber;
		}
	}

	const int32 NewSlotNumber = (ExistingSlotNumber != INDEX_NONE) ? ExistingSlotNumber : (MaxSlotNumber + 1);

	USaveGame* NewSave = UGameplayStatics::CreateSaveGameObject(SaveClass);
	if (!NewSave)
	{
		OutError = TEXT("Failed to create debug save object.");
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return false;
	}

	ARDebugSaveToolInternal::TrySetNamePropertyOnObject(NewSave, TEXT("SaveSlot"), OutDebugSlotName);
	ARDebugSaveToolInternal::TrySetIntPropertyOnObject(NewSave, TEXT("SaveSlotNumber"), NewSlotNumber);
	ARDebugSaveToolInternal::TrySetDateTimePropertyOnObject(NewSave, TEXT("LastSaved"), FDateTime::UtcNow());

	if (!UGameplayStatics::SaveGameToSlot(NewSave, OutDebugSlotName.ToString(), DefaultUserIndex))
	{
		OutError = FString::Printf(TEXT("Failed to save debug slot '%s'."), *OutDebugSlotName.ToString());
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return false;
	}

	const FARDebugSaveSlotEntry Entry = BuildSlotEntryFromSave(NewSave, OutDebugSlotName, NewSlotNumber);
	if (!UpsertSlotEntry(DebugIndex, Entry, OutError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|IO] Could not update debug index entry for '%s': %s"), *OutDebugSlotName.ToString(), *OutError);
		return false;
	}

	if (!SaveDebugIndexSave(DebugIndex, OutError))
	{
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[DebugSaveTool] Created debug save slot '%s' (SlotNumber=%d)."), *OutDebugSlotName.ToString(), NewSlotNumber);
	OutSaveGame = NewSave;
	return true;
}

bool UARDebugSaveToolLibrary::LoadDebugSave(FName DebugSlotName, USaveGame*& OutSaveGame, FString& OutError)
{
	const FName NormalizedSlot = NormalizeDebugSlotName(DebugSlotName);
	OutSaveGame = nullptr;
	OutError.Reset();

	if (!UGameplayStatics::DoesSaveGameExist(NormalizedSlot.ToString(), DefaultUserIndex))
	{
		OutError = FString::Printf(TEXT("Debug slot '%s' does not exist."), *NormalizedSlot.ToString());
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return false;
	}

	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(NormalizedSlot.ToString(), DefaultUserIndex);
	if (!Loaded)
	{
		OutError = FString::Printf(TEXT("Failed to load debug slot '%s'."), *NormalizedSlot.ToString());
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return false;
	}

	OutSaveGame = Loaded;
	return true;
}

bool UARDebugSaveToolLibrary::SaveDebugSave(FName DebugSlotName, USaveGame* SaveGameObject, FString& OutError)
{
	OutError.Reset();
	const FName NormalizedSlot = NormalizeDebugSlotName(DebugSlotName);
	if (!SaveGameObject)
	{
		OutError = TEXT("SaveGameObject is null.");
		return false;
	}

	ARDebugSaveToolInternal::TrySetNamePropertyOnObject(SaveGameObject, TEXT("SaveSlot"), NormalizedSlot);
	ARDebugSaveToolInternal::TrySetDateTimePropertyOnObject(SaveGameObject, TEXT("LastSaved"), FDateTime::UtcNow());

	if (!UGameplayStatics::SaveGameToSlot(SaveGameObject, NormalizedSlot.ToString(), DefaultUserIndex))
	{
		OutError = FString::Printf(TEXT("Failed to save debug slot '%s'."), *NormalizedSlot.ToString());
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return false;
	}

	FString IndexError;
	USaveGame* DebugIndex = LoadOrCreateDebugIndexSave(IndexError);
	if (!DebugIndex)
	{
		OutError = IndexError;
		return false;
	}

	TArray<FARDebugSaveSlotEntry> ExistingSlots;
	ReadSlotEntries(DebugIndex, ExistingSlots, IndexError);
	int32 SlotNumberHint = 1;
	for (const FARDebugSaveSlotEntry& Existing : ExistingSlots)
	{
		if (Existing.SlotName == NormalizedSlot)
		{
			SlotNumberHint = Existing.SlotNumber;
			break;
		}
		SlotNumberHint = FMath::Max(SlotNumberHint, Existing.SlotNumber + 1);
	}

	const FARDebugSaveSlotEntry Entry = BuildSlotEntryFromSave(SaveGameObject, NormalizedSlot, SlotNumberHint);
	if (!UpsertSlotEntry(DebugIndex, Entry, OutError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|IO] Could not upsert debug index for '%s': %s"), *NormalizedSlot.ToString(), *OutError);
		return false;
	}

	if (!SaveDebugIndexSave(DebugIndex, OutError))
	{
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[DebugSaveTool] Saved debug slot '%s'."), *NormalizedSlot.ToString());
	return true;
}

bool UARDebugSaveToolLibrary::DeleteDebugSave(FName DebugSlotName, FString& OutError)
{
	OutError.Reset();
	const FName NormalizedSlot = NormalizeDebugSlotName(DebugSlotName);

	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(NormalizedSlot.ToString(), DefaultUserIndex);
	if (!bDeleted && UGameplayStatics::DoesSaveGameExist(NormalizedSlot.ToString(), DefaultUserIndex))
	{
		OutError = FString::Printf(TEXT("Failed to delete debug slot '%s'."), *NormalizedSlot.ToString());
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|IO] %s"), *OutError);
		return false;
	}

	FString IndexError;
	USaveGame* DebugIndex = LoadOrCreateDebugIndexSave(IndexError);
	if (!DebugIndex)
	{
		OutError = IndexError;
		return false;
	}

	if (!RemoveSlotEntry(DebugIndex, NormalizedSlot, OutError))
	{
		UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|IO] Could not remove debug index entry for '%s': %s"), *NormalizedSlot.ToString(), *OutError);
		return false;
	}

	if (!SaveDebugIndexSave(DebugIndex, OutError))
	{
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[DebugSaveTool] Deleted debug slot '%s'."), *NormalizedSlot.ToString());
	return true;
}

FARDebugSaveEditResult UARDebugSaveToolLibrary::ApplyDebugSaveEdits(USaveGame* SaveGameObject, const FARDebugSaveEdits& Edits)
{
	FARDebugSaveEditResult Result;
	Result.bSuccess = false;

	if (!SaveGameObject)
	{
		Result.Error = TEXT("SaveGameObject is null.");
		UE_LOG(ARLog, Error, TEXT("[DebugSaveTool|Validation] %s"), *Result.Error);
		return Result;
	}

	int32 Changes = 0;

	if (Edits.bSetSeenDialogue && ARDebugSaveToolInternal::TrySetGameplayTagContainerPropertyOnObject(SaveGameObject, TEXT("SeenDialogue"), Edits.SeenDialogue))
	{
		++Changes;
	}
	if (Edits.bSetDialogueFlags && ARDebugSaveToolInternal::TrySetGameplayTagContainerPropertyOnObject(SaveGameObject, TEXT("DialogueFlags"), Edits.DialogueFlags))
	{
		++Changes;
	}
	if (Edits.bSetUnlocks && ARDebugSaveToolInternal::TrySetGameplayTagContainerPropertyOnObject(SaveGameObject, TEXT("Unlocks"), Edits.Unlocks))
	{
		++Changes;
	}
	if (Edits.bSetChoices && ARDebugSaveToolInternal::TrySetGameplayTagContainerPropertyOnObject(SaveGameObject, TEXT("Choices"), Edits.Choices))
	{
		++Changes;
	}
	if (Edits.bSetMoney && ARDebugSaveToolInternal::TrySetIntPropertyOnObject(SaveGameObject, TEXT("Money"), Edits.Money))
	{
		++Changes;
	}
	if (Edits.bSetMaterial && ARDebugSaveToolInternal::TrySetIntPropertyOnObject(SaveGameObject, TEXT("Material"), Edits.Material))
	{
		++Changes;
	}
	if (Edits.bSetCycles && ARDebugSaveToolInternal::TrySetIntPropertyOnObject(SaveGameObject, TEXT("Cycles"), Edits.Cycles))
	{
		++Changes;
	}

	if (Edits.bSetMeatAmount)
	{
		UScriptStruct* MeatStructType = nullptr;
		void* MeatValuePtr = nullptr;
		if (GetStructFromObjectProperty(SaveGameObject, TEXT("Meat"), MeatStructType, MeatValuePtr))
		{
			if (ARDebugSaveToolInternal::TrySetIntField(MeatStructType, MeatValuePtr, TEXT("Amount"), Edits.MeatAmount))
			{
				++Changes;
			}
			else
			{
				UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] Could not set Meat.Amount on save object '%s'."), *GetNameSafe(SaveGameObject));
			}
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] Could not find Meat struct on save object '%s'."), *GetNameSafe(SaveGameObject));
		}
	}

	if (!Edits.PlayerLoadoutEdits.IsEmpty())
	{
		FProperty* PlayerStatesPropRaw = FindPropertyByNamePrefix(SaveGameObject->GetClass(), TEXT("PlayerStates"));
		FArrayProperty* PlayerStatesProp = CastField<FArrayProperty>(PlayerStatesPropRaw);
		FStructProperty* PlayerStateInnerStruct = PlayerStatesProp ? CastField<FStructProperty>(PlayerStatesProp->Inner) : nullptr;

		if (PlayerStatesProp && PlayerStateInnerStruct && PlayerStateInnerStruct->Struct)
		{
			FScriptArrayHelper PlayerStatesHelper(PlayerStatesProp, PlayerStatesProp->ContainerPtrToValuePtr<void>(SaveGameObject));

			for (const FARDebugPlayerLoadoutEdit& Edit : Edits.PlayerLoadoutEdits)
			{
				if (Edit.PlayerStateIndex < 0 || Edit.PlayerStateIndex >= PlayerStatesHelper.Num())
				{
					UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] PlayerState index %d out of range (count=%d)."),
						Edit.PlayerStateIndex, PlayerStatesHelper.Num());
					continue;
				}

				void* PlayerStateElem = PlayerStatesHelper.GetRawPtr(Edit.PlayerStateIndex);
				if (ARDebugSaveToolInternal::TrySetGameplayTagContainerField(PlayerStateInnerStruct->Struct, PlayerStateElem, TEXT("LoadoutTags"), Edit.LoadoutTags))
				{
					++Changes;
				}
				else
				{
					UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] Could not set PlayerStates[%d].LoadoutTags."),
						Edit.PlayerStateIndex);
				}
			}
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[DebugSaveTool|Validation] PlayerStates array struct is missing or invalid."));
		}
	}

	Result.bSuccess = true;
	Result.ChangedFieldCount = Changes;
	return Result;
}

void UARDebugSaveToolLibrary::GetDebugSaveDiagnostics(USaveGame* SaveGameObject, TArray<FARDebugFieldDiagnostic>& OutFields)
{
	OutFields.Reset();
	if (!SaveGameObject) return;

	static const TArray<FString> SupportedPrefixes = {
		TEXT("SeenDialogue"),
		TEXT("DialogueFlags"),
		TEXT("Unlocks"),
		TEXT("Choices"),
		TEXT("Money"),
		TEXT("Material"),
		TEXT("Cycles"),
		TEXT("Meat"),
		TEXT("PlayerStates"),
		TEXT("SaveSlot"),
		TEXT("SaveGameVersion"),
		TEXT("SaveSlotNumber"),
		TEXT("LastSaved")
	};

	for (TFieldIterator<FProperty> It(SaveGameObject->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		FARDebugFieldDiagnostic Row;
		Row.FieldName = Prop->GetFName();
		Row.FieldType = Prop->GetClass()->GetName();

		const FString Name = Prop->GetName();
		for (const FString& Prefix : SupportedPrefixes)
		{
			if (Name.StartsWith(Prefix))
			{
				Row.bSupportedByTypedEdits = true;
				break;
			}
		}

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(SaveGameObject);
		Prop->ExportTextItem_Direct(Row.ValueAsString, ValuePtr, nullptr, SaveGameObject, PPF_None);
		OutFields.Add(Row);
	}
}
