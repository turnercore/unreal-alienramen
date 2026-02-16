#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HelperLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API UHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Apply matching struct properties to Target by name (case-insensitive). */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "AlienRamen|Helpers|Reflection",
		meta = (CustomStructureParam = "StructData", DisplayName = "Apply Struct To Object (By Name)"))
	static void ApplyStructToObjectByName(
		UObject* Target,
		const int32& StructData,
		int32& OutPropertiesCopied,
		int32& OutPropertiesSkipped
	);

	DECLARE_FUNCTION(execApplyStructToObjectByName);


	/** Fill OutStructData from Source by matching property name (case-insensitive) + compatible type. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "AlienRamen|Helpers|Reflection",
		meta = (CustomStructureParam = "OutStructData", DisplayName = "Extract Object To Struct (By Name)"))
	static void ExtractObjectToStructByName(
		UObject* Source,
		int32& OutStructData,
		int32& OutPropertiesCopied,
		int32& OutPropertiesSkipped
	);

	DECLARE_FUNCTION(execExtractObjectToStructByName);


private:

	static void ApplyStructToObjectByName_Impl(
		UObject* Target,
		const UStruct* StructType,
		const void* StructPtr,
		int32* OutPropertiesCopied,
		int32* OutPropertiesSkipped,
		TArray<FName>* OutCopiedNames,
		TArray<FName>* OutMissingNames,
		TArray<FName>* OutTypeMismatchNames,
		bool bEnableLog
	);

	static void ExtractObjectToStructByName_Impl(
		UObject* Source,
		const UStruct* StructType,
		void* StructPtr,
		int32* OutPropertiesCopied,
		int32* OutPropertiesSkipped,
		bool bEnableLog,
		bool bResetStructToDefaults
	);
};
