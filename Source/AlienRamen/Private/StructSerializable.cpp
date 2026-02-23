#include "StructSerializable.h"

#include "ARLog.h"
#include "../HelperLibrary.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

void IStructSerializable::ExtractStateToStruct_Implementation(FInstancedStruct& CurrentState) const
{
	const UObject* SelfObject = _getUObject();
	if (!SelfObject)
	{
		UE_LOG(ARLog, Error, TEXT("[StructSerializable] ExtractStateToStruct failed: self object is null."));
		CurrentState.Reset();
		return;
	}

	UScriptStruct* StateStruct = IStructSerializable::Execute_GetStateStruct(const_cast<UObject*>(SelfObject));
	if (!StateStruct)
	{
		CurrentState.Reset();
		return;
	}

	CurrentState = UHelperLibrary::ExtractObjectToStructByName(const_cast<UObject*>(SelfObject), StateStruct);
}

bool IStructSerializable::ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	UObject* SelfObject = _getUObject();
	if (!SelfObject)
	{
		UE_LOG(ARLog, Error, TEXT("[StructSerializable] ApplyStateFromStruct failed: self object is null."));
		return false;
	}

	if (const AActor* SelfActor = Cast<AActor>(SelfObject))
	{
		if (!SelfActor->HasAuthority())
		{
			UE_LOG(ARLog, Warning,
				TEXT("[StructSerializable] ApplyStateFromStruct blocked on non-authority for '%s'."),
				*GetNameSafe(SelfObject));
			return false;
		}
	}

	if (!SavedState.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[StructSerializable] ApplyStateFromStruct failed for '%s': SavedState is invalid."),
			*GetNameSafe(SelfObject));
		return false;
	}

	UScriptStruct* ExpectedStruct = IStructSerializable::Execute_GetStateStruct(SelfObject);
	if (!ExpectedStruct)
	{
		return false;
	}

	const UScriptStruct* IncomingStruct = SavedState.GetScriptStruct();
	if (IncomingStruct != ExpectedStruct)
	{
		UE_LOG(ARLog, Warning,
			TEXT("[StructSerializable] ApplyStateFromStruct failed for '%s': struct mismatch (Expected=%s, Incoming=%s)."),
			*GetNameSafe(SelfObject),
			*GetNameSafe(ExpectedStruct),
			*GetNameSafe(IncomingStruct));
		return false;
	}

	UHelperLibrary::ApplyStructToObjectByName(SelfObject, SavedState);
	return true;
}

UScriptStruct* IStructSerializable::GetStateStruct_Implementation() const
{
	const UObject* SelfObject = _getUObject();
	if (!SelfObject)
	{
		UE_LOG(ARLog, Error, TEXT("[StructSerializable] GetStateStruct failed: self object is null."));
		return nullptr;
	}

	const FObjectProperty* StructProperty = FindFProperty<FObjectProperty>(SelfObject->GetClass(), TEXT("ClassStateStruct"));
	if (!StructProperty)
	{
		UE_LOG(ARLog, Error,
			TEXT("[StructSerializable] GetStateStruct failed for '%s': required property 'ClassStateStruct' not found."),
			*GetNameSafe(SelfObject));
		return nullptr;
	}

	if (!StructProperty->PropertyClass->IsChildOf(UScriptStruct::StaticClass()))
	{
		UE_LOG(ARLog, Error,
			TEXT("[StructSerializable] GetStateStruct failed for '%s': 'ClassStateStruct' is not a UScriptStruct object property."),
			*GetNameSafe(SelfObject));
		return nullptr;
	}

	UScriptStruct* StateStruct = Cast<UScriptStruct>(StructProperty->GetObjectPropertyValue_InContainer(SelfObject));
	if (!StateStruct)
	{
		UE_LOG(ARLog, Error,
			TEXT("[StructSerializable] GetStateStruct failed for '%s': 'ClassStateStruct' is null."),
			*GetNameSafe(SelfObject));
		return nullptr;
	}

	return StateStruct;
}
