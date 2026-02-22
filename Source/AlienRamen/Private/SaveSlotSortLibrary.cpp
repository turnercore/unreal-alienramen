#include "SaveSlotSortLibrary.h"
#include "ARLog.h"

#include "Algo/StableSort.h"
#include "UObject/UnrealType.h"

static FString NormalizeNameKey(const FString& InName)
{
	return InName.ToLower();
}

static FString NormalizePropNameKey(const FProperty* P)
{
#if ENGINE_MAJOR_VERSION >= 5
	return NormalizeNameKey(P ? P->GetAuthoredName() : FString());
#else
	return NormalizeNameKey(P ? P->GetName() : FString());
#endif
}

DEFINE_FUNCTION(USaveSlotSortLibrary::execSortStructArrayByDateTimeField)
{
	// Step the wildcard array
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(nullptr);

	FArrayProperty* ArrayProp = CastField<FArrayProperty>(Stack.MostRecentProperty);
	void* ArrayAddr = Stack.MostRecentPropertyAddress;

	// Remaining params
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_UBOOL(bNewestFirst);
	P_FINISH;

	if (!ArrayProp || !ArrayAddr)
	{
		UE_LOG(ARLog, Warning, TEXT("[SaveSlotSort] SortStructArrayByDateTimeField failed: TargetArray is invalid."));
		return;
	}

	// Must be array of structs
	FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
	if (!InnerStructProp || !InnerStructProp->Struct)
	{
		UE_LOG(ARLog, Warning,
			TEXT("[SaveSlotSort] SortStructArrayByDateTimeField failed: TargetArray must be an array of structs."));
		return;
	}

	UScriptStruct* StructType = InnerStructProp->Struct;

	// Find a DateTime property matching FieldName (case-insensitive, authored name)
	const FString WantedKey = NormalizeNameKey(FieldName.ToString());

	FStructProperty* DateTimeProp = nullptr;
	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* P = *It;
		if (!P) continue;

		if (NormalizePropNameKey(P) == WantedKey)
		{
			FStructProperty* SP = CastField<FStructProperty>(P);
			if (SP && SP->Struct == TBaseStructure<FDateTime>::Get())
			{
				DateTimeProp = SP;
				break;
			}
		}
	}

	if (!DateTimeProp)
	{
		UE_LOG(ARLog, Warning,
			TEXT("[SaveSlotSort] SortStructArrayByDateTimeField failed: could not find DateTime field '%s' on struct '%s'."),
			*FieldName.ToString(), *GetNameSafe(StructType));
		return;
	}

	FScriptArrayHelper Helper(ArrayProp, ArrayAddr);
	const int32 Num = Helper.Num();
	if (Num <= 1)
	{
		return;
	}

	// Build indices [0..Num-1], stable sort by key
	TArray<int32> Indices;
	Indices.Reserve(Num);
	for (int32 i = 0; i < Num; ++i) Indices.Add(i);

	auto GetKey = [&](int32 Index) -> FDateTime
		{
			void* ElemPtr = Helper.GetRawPtr(Index);
			const void* TimePtr = DateTimeProp->ContainerPtrToValuePtr<void>(ElemPtr);
			return *reinterpret_cast<const FDateTime*>(TimePtr);
		};

	Algo::StableSort(Indices, [&](int32 A, int32 B)
		{
			const FDateTime KA = GetKey(A);
			const FDateTime KB = GetKey(B);
			return bNewestFirst ? (KA > KB) : (KA < KB);
		});

	// Apply permutation in-place using swaps
	TArray<int32> Where;
	Where.SetNumUninitialized(Num);
	for (int32 NewPos = 0; NewPos < Num; ++NewPos)
	{
		Where[Indices[NewPos]] = NewPos; // oldIndex -> newIndex
	}

	for (int32 i = 0; i < Num; ++i)
	{
		while (Where[i] != i)
		{
			const int32 j = Where[i];

			Helper.SwapValues(i, j);

			// keep Where in sync with the swap
			Swap(Where[i], Where[j]);
		}
	}

	UE_LOG(ARLog, Verbose,
		TEXT("[SaveSlotSort] Sorted %d elements of '%s' by DateTime field '%s' (%s)."),
		Num, *GetNameSafe(StructType), *FieldName.ToString(), bNewestFirst ? TEXT("newest first") : TEXT("oldest first"));
}

void USaveSlotSortLibrary::SortStructArrayByDateTimeField(TArray<int32>& TargetArray, FName FieldName, bool bNewestFirst)
{
	// Intentionally empty (CustomThunk)
}

