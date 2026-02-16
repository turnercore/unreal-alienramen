#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SaveSlotSortLibrary.generated.h"

UCLASS()
class ALIENRAMEN_API USaveSlotSortLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "AlienRamen|Save",
		meta = (
			DisplayName = "Sort Struct Array By DateTime Field",
			ArrayParm = "TargetArray",
			ArrayTypeDependentParams = "TargetArray",
			AdvancedDisplay = "FieldName",
			AutoCreateRefTerm = "FieldName"
			))
	static void SortStructArrayByDateTimeField(
		UPARAM(ref) TArray<int32>& TargetArray,
		FName FieldName = TEXT("LastSavedTime"),
		bool bNewestFirst = true
	);

	DECLARE_FUNCTION(execSortStructArrayByDateTimeField);
};