#include "HelperLibrary.h"
#include "UObject/UnrealType.h" // FProperty, FStructProperty, etc.

DEFINE_FUNCTION(UHelperLibrary::execApplyStructToObjectByName)
{
	P_GET_OBJECT(UObject, Target);

	// Pull the wildcard struct parameter off the stack
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);

	FProperty* PassedProperty = Stack.MostRecentProperty;
	void* PassedPropertyAddress = Stack.MostRecentPropertyAddress;

	P_GET_PROPERTY_REF(FIntProperty, OutPropertiesCopied);
	P_GET_PROPERTY_REF(FIntProperty, OutPropertiesSkipped);
	P_FINISH;

	OutPropertiesCopied = 0;
	OutPropertiesSkipped = 0;

	if (!Target || !PassedProperty || !PassedPropertyAddress)
	{
		return;
	}

	// Ensure the wildcard param is actually a struct
	const FStructProperty* StructProp = CastField<FStructProperty>(PassedProperty);
	if (!StructProp || !StructProp->Struct)
	{
		return;
	}

	ApplyStructToObjectByName_Impl(Target, StructProp->Struct, PassedPropertyAddress, OutPropertiesCopied, OutPropertiesSkipped);
}

void UHelperLibrary::ApplyStructToObjectByName_Impl(
	UObject* Target,
	const UStruct* StructType,
	const void* StructPtr,
	int32& OutPropertiesCopied,
	int32& OutPropertiesSkipped)
{
	if (!Target || !StructType || !StructPtr)
	{
		return;
	}

	UClass* TargetClass = Target->GetClass();

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* SrcProp = *It;
		if (!SrcProp)
		{
			continue;
		}

		// Find destination property with the same name
		FProperty* DstProp = TargetClass->FindPropertyByName(SrcProp->GetFName());
		if (!DstProp)
		{
			++OutPropertiesSkipped;
			continue;
		}

		// Require identical type
		if (!SrcProp->SameType(DstProp))
		{
			++OutPropertiesSkipped;
			continue;
		}

		// Copy value from struct memory to object memory
		const void* SrcValuePtr = SrcProp->ContainerPtrToValuePtr<void>(StructPtr);
		void* DstValuePtr = DstProp->ContainerPtrToValuePtr<void>(Target);

		DstProp->CopyCompleteValue(DstValuePtr, SrcValuePtr);
		++OutPropertiesCopied;
	}
}
