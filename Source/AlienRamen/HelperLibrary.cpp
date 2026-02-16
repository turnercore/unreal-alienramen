// HelperLibrary.cpp

#include "HelperLibrary.h"

#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlienRamenHelperLibrary, Log, All);

// -------------------------
// Helpers
// -------------------------

static FString NormalizeTypePath(const UObject* Obj)
{
	if (!Obj) return FString();

	FString Path = Obj->GetPathName();
	Path.ReplaceInline(TEXT("REINST_"), TEXT(""));
	Path.ReplaceInline(TEXT("SKEL_"), TEXT(""));
	Path.ReplaceInline(TEXT("TRASHCLASS_"), TEXT(""));
	return Path;
}

static FString GetAuthoredNameString(const FProperty* P)
{
	if (!P) return FString();

#if ENGINE_MAJOR_VERSION >= 5
	return P->GetAuthoredName();
#else
	return P->GetName();
#endif
}

static FString NormalizeNameKey(const FString& InName)
{
	FString Name = InName;

	// Conservative suffix stripping: _<number>_<hex...> => base
	int32 FirstUnderscore = INDEX_NONE;
	int32 SecondUnderscore = INDEX_NONE;

	if (Name.FindChar(TEXT('_'), FirstUnderscore))
	{
		SecondUnderscore = Name.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstUnderscore + 1);
		if (SecondUnderscore != INDEX_NONE)
		{
			const FString Middle = Name.Mid(FirstUnderscore + 1, SecondUnderscore - (FirstUnderscore + 1));
			const FString Tail = Name.Mid(SecondUnderscore + 1);

			const bool bMiddleIsNumber = Middle.IsNumeric();

			bool bTailLooksHex = (Tail.Len() >= 8);
			if (bTailLooksHex)
			{
				for (TCHAR C : Tail)
				{
					if (!FChar::IsHexDigit(C))
					{
						bTailLooksHex = false;
						break;
					}
				}
			}

			if (bMiddleIsNumber && bTailLooksHex)
			{
				Name = Name.Left(FirstUnderscore);
			}
		}
	}

	return Name.ToLower(); // case-insensitive match
}

static FString NormalizePropNameKey(const FProperty* P)
{
	return NormalizeNameKey(GetAuthoredNameString(P));
}

static FName CleanPropName(const FProperty* P)
{
	return FName(*GetAuthoredNameString(P));
}

static bool ArePropertiesCompatible(const FProperty* Src, const FProperty* Dst)
{
	if (!Src || !Dst) return false;

	// Exact match (fast path)
	if (Src->SameType(Dst))
	{
		return true;
	}

	// Struct compatibility (SKEL/REINST tolerant)
	if (const FStructProperty* SrcStruct = CastField<FStructProperty>(Src))
	{
		if (const FStructProperty* DstStruct = CastField<FStructProperty>(Dst))
		{
			const FString SrcPath = NormalizeTypePath(SrcStruct->Struct);
			const FString DstPath = NormalizeTypePath(DstStruct->Struct);
			return !SrcPath.IsEmpty() && SrcPath == DstPath;
		}
	}

	// Enum compatibility (EnumProperty)
	if (const FEnumProperty* SrcEnumProp = CastField<FEnumProperty>(Src))
	{
		if (const FEnumProperty* DstEnumProp = CastField<FEnumProperty>(Dst))
		{
			const FString SrcEnum = NormalizeTypePath(SrcEnumProp->GetEnum());
			const FString DstEnum = NormalizeTypePath(DstEnumProp->GetEnum());
			return !SrcEnum.IsEmpty() && SrcEnum == DstEnum;
		}
	}

	// Enum compatibility (ByteProperty-with-enum)
	if (const FByteProperty* SrcByte = CastField<FByteProperty>(Src))
	{
		if (const FByteProperty* DstByte = CastField<FByteProperty>(Dst))
		{
			if (SrcByte->Enum || DstByte->Enum)
			{
				const FString SrcEnum = NormalizeTypePath(SrcByte->Enum);
				const FString DstEnum = NormalizeTypePath(DstByte->Enum);
				return !SrcEnum.IsEmpty() && SrcEnum == DstEnum;
			}
		}
	}

	// Object compatibility
	if (const FObjectPropertyBase* SrcObj = CastField<FObjectPropertyBase>(Src))
	{
		if (const FObjectPropertyBase* DstObj = CastField<FObjectPropertyBase>(Dst))
		{
			return SrcObj->PropertyClass == DstObj->PropertyClass;
		}
	}

	// Class compatibility
	if (const FClassProperty* SrcClass = CastField<FClassProperty>(Src))
	{
		if (const FClassProperty* DstClass = CastField<FClassProperty>(Dst))
		{
			return SrcClass->MetaClass == DstClass->MetaClass;
		}
	}

	return false;
}

static void DebugLog(bool bEnableLog, const FString& Msg)
{
	if (!bEnableLog) return;
	UE_LOG(LogAlienRamenHelperLibrary, Log, TEXT("%s"), *Msg);
}

// -------------------------
// Public API
// -------------------------

void UHelperLibrary::ApplyStructToObjectByName(UObject* Target, const FInstancedStruct& StructData)
{
	if (!Target)
	{
		UE_LOG(LogAlienRamenHelperLibrary, Warning,
			TEXT("ApplyStructToObjectByName: invalid input (Target=null)"));
		return;
	}

	if (!StructData.IsValid())
	{
		UE_LOG(LogAlienRamenHelperLibrary, Warning,
			TEXT("ApplyStructToObjectByName: invalid input (StructData is not valid) Target=%s"),
			*GetNameSafe(Target));
		return;
	}

	const UScriptStruct* StructType = StructData.GetScriptStruct();
	const void* StructPtr = StructData.GetMemory();

	ApplyStructToObjectByName_Impl(Target, StructType, StructPtr, /*bEnableLog*/ true);
}

FInstancedStruct UHelperLibrary::ExtractObjectToStructByName(UObject* Source, UScriptStruct* StructType)
{
	FInstancedStruct Out;

	if (!Source || !StructType)
	{
		UE_LOG(LogAlienRamenHelperLibrary, Warning,
			TEXT("ExtractObjectToStructByName: invalid input (Source=%s, StructType=%s)"),
			*GetNameSafe(Source), *GetNameSafe(StructType));
		return Out;
	}

	Out.InitializeAs(StructType);

	void* StructPtr = Out.GetMutableMemory();
	ExtractObjectToStructByName_Impl(Source, StructType, StructPtr, /*bEnableLog*/ true, /*bResetStructToDefaults*/ false);

	return Out;
}

// -------------------------
// Implementation
// -------------------------

void UHelperLibrary::ExtractObjectToStructByName_Impl(
	UObject* Source,
	const UStruct* StructType,
	void* StructPtr,
	bool bEnableLog,
	bool bResetStructToDefaults)
{
	if (!Source || !StructType || !StructPtr)
	{
		return;
	}

	UClass* SourceClass = Source->GetClass();

	DebugLog(bEnableLog, FString::Printf(TEXT("ExtractObjectToStructByName: Source=%s Class=%s Struct=%s"),
		*GetNameSafe(Source),
		*GetNameSafe(SourceClass),
		*GetNameSafe(StructType)));

	// Only needed when caller passes in an already-live struct buffer they want reset.
	// For FInstancedStruct::InitializeAs, the memory is already constructed.
	if (bResetStructToDefaults)
	{
		StructType->DestroyStruct(StructPtr);
		StructType->InitializeStruct(StructPtr);
	}

	// Map normalized name -> source candidates
	TMultiMap<FString, FProperty*> SrcByName;
	for (TFieldIterator<FProperty> It(SourceClass); It; ++It)
	{
		FProperty* P = *It;
		if (!P) continue;
		SrcByName.Add(NormalizePropNameKey(P), P);
	}

	// Iterate DEST (struct) properties, find matching SOURCE (object) properties.
	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* DstProp = *It;
		if (!DstProp) continue;

		const FString DstKey = NormalizePropNameKey(DstProp);
		const FName CleanName = CleanPropName(DstProp);

		TArray<FProperty*> Candidates;
		SrcByName.MultiFind(DstKey, Candidates);

		if (Candidates.Num() == 0)
		{
			DebugLog(bEnableLog, FString::Printf(TEXT("  MISSING ON SOURCE: %s"), *CleanName.ToString()));
			continue;
		}

		FProperty* BestSrc = nullptr;
		for (FProperty* Cand : Candidates)
		{
			if (ArePropertiesCompatible(Cand, DstProp))
			{
				BestSrc = Cand;
				break;
			}
		}

		if (!BestSrc)
		{
			const FString SrcTypeName = Candidates[0] ? Candidates[0]->GetClass()->GetName() : TEXT("Unknown");
			const FString DstTypeName = DstProp->GetClass()->GetName();

			DebugLog(bEnableLog, FString::Printf(TEXT("  TYPE MISMATCH: %s (Src=%s Dst=%s)"),
				*CleanName.ToString(), *SrcTypeName, *DstTypeName));
			continue;
		}

		const void* SrcValuePtr = BestSrc->ContainerPtrToValuePtr<void>(Source);
		void* DstValuePtr = DstProp->ContainerPtrToValuePtr<void>(StructPtr);

		DstProp->CopyCompleteValue(DstValuePtr, SrcValuePtr);

		DebugLog(bEnableLog, FString::Printf(TEXT("  COPIED: %s"), *CleanName.ToString()));
	}
}

void UHelperLibrary::ApplyStructToObjectByName_Impl(
	UObject* Target,
	const UStruct* StructType,
	const void* StructPtr,
	bool bEnableLog)
{
	if (!Target || !StructType || !StructPtr)
	{
		return;
	}

	UClass* TargetClass = Target->GetClass();

	DebugLog(bEnableLog, FString::Printf(TEXT("ApplyStructToObjectByName: Target=%s Class=%s Struct=%s"),
		*GetNameSafe(Target),
		*GetNameSafe(TargetClass),
		*GetNameSafe(StructType)));

	// Map normalized name -> destination candidates
	TMultiMap<FString, FProperty*> DstByName;
	for (TFieldIterator<FProperty> It(TargetClass); It; ++It)
	{
		FProperty* P = *It;
		if (!P) continue;
		DstByName.Add(NormalizePropNameKey(P), P);
	}

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* SrcProp = *It;
		if (!SrcProp) continue;

		const FString SrcKey = NormalizePropNameKey(SrcProp);
		const FName CleanName = CleanPropName(SrcProp);

		TArray<FProperty*> Candidates;
		DstByName.MultiFind(SrcKey, Candidates);

		if (Candidates.Num() == 0)
		{
			DebugLog(bEnableLog, FString::Printf(TEXT("  MISSING: %s"), *CleanName.ToString()));
			continue;
		}

		FProperty* BestDst = nullptr;
		for (FProperty* Cand : Candidates)
		{
			if (ArePropertiesCompatible(SrcProp, Cand))
			{
				BestDst = Cand;
				break;
			}
		}

		if (!BestDst)
		{
			const FString SrcTypeName = SrcProp->GetClass()->GetName();
			const FString DstTypeName = Candidates[0] ? Candidates[0]->GetClass()->GetName() : TEXT("Unknown");

			DebugLog(bEnableLog, FString::Printf(TEXT("  TYPE MISMATCH: %s (Src=%s Dst=%s)"),
				*CleanName.ToString(), *SrcTypeName, *DstTypeName));
			continue;
		}

		const void* SrcValuePtr = SrcProp->ContainerPtrToValuePtr<void>(StructPtr);
		void* DstValuePtr = BestDst->ContainerPtrToValuePtr<void>(Target);

		BestDst->CopyCompleteValue(DstValuePtr, SrcValuePtr);

		DebugLog(bEnableLog, FString::Printf(TEXT("  COPIED: %s"), *CleanName.ToString()));
	}
}
