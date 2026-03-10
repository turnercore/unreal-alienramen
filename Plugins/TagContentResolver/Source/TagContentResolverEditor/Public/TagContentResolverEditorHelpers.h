#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UDataTable;
class UScriptStruct;

class TAGCONTENTRESOLVEREDITOR_API FTagContentResolverEditorHelpers
{
public:
	static bool TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError);
	static bool TryResolveDataTableForRowStruct(UScriptStruct* DesiredRowStruct, UDataTable*& OutDataTable, FGameplayTag& OutMatchedRootTag, FString& OutError);
};
