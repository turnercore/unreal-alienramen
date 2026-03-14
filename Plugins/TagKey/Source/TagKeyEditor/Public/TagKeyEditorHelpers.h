#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UDataTable;
class UScriptStruct;

class TAGKEYEDITOR_API FTagKeyEditorHelpers
{
public:
	static bool TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError);
	static bool TryResolveDataTableForRowStruct(UScriptStruct* DesiredRowStruct, UDataTable*& OutDataTable, FGameplayTag& OutMatchedRootTag, FString& OutError);
};
