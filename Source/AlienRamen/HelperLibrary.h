#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HelperLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API UHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Copies matching properties from a struct into a target object by name.
	 * Only copies when:
	 *  - Target has a UPROPERTY with the same name
	 *  - Property types match (SameType)
	 *
	 * Blueprint usage: pass ANY struct into StructData (wildcard), and a Target (Actor/UObject).
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "AlienRamen|Helpers|Reflection",
		meta = (CustomStructureParam = "StructData", DisplayName = "Apply Struct To Object (By Name)"))
	static void ApplyStructToObjectByName(UObject* Target, const int32& StructData,
		int32& OutPropertiesCopied, int32& OutPropertiesSkipped);

	// Custom thunk declaration (required for wildcard struct pin)
	DECLARE_FUNCTION(execApplyStructToObjectByName);

private:
	static void ApplyStructToObjectByName_Impl(UObject* Target, const UStruct* StructType, const void* StructPtr,
		int32& OutPropertiesCopied, int32& OutPropertiesSkipped);
};
