#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructUtils/InstancedStruct.h"
#include "HelperLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API UHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Apply matching struct properties to Target by name (case-insensitive). */
	UFUNCTION(BlueprintCallable, Category = "AlienRamen|Helpers|Reflection",
		meta = (DisplayName = "Apply Struct To Object (By Name)"))
	static void ApplyStructToObjectByName(
		UObject* Target,
		const FInstancedStruct& StructData
	);

	/** Create an instance of StructType, fill it from Source by matching property name (case-insensitive). */
	UFUNCTION(BlueprintCallable, Category = "AlienRamen|Helpers|Reflection",
		meta = (DisplayName = "Extract Object To Struct (By Name)"))
	static FInstancedStruct ExtractObjectToStructByName(
		UObject* Source,
		UScriptStruct* StructType
	);

private:
	static void ApplyStructToObjectByName_Impl(
		UObject* Target,
		const UStruct* StructType,
		const void* StructPtr,
		bool bEnableLog
	);

	static void ExtractObjectToStructByName_Impl(
		UObject* Source,
		const UStruct* StructType,
		void* StructPtr,
		bool bEnableLog,
		bool bResetStructToDefaults
	);
};
