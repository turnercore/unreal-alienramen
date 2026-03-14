#include "TagKeyEditorHelpers.h"

#include "TagKeySubsystem.h"

bool FTagKeyEditorHelpers::TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError)
{
	return UTagKeySubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(RootTag, OutDataTable, OutError);
}

bool FTagKeyEditorHelpers::TryResolveDataTableForRowStruct(
	UScriptStruct* DesiredRowStruct,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRootTag,
	FString& OutError)
{
	return UTagKeySubsystem::TryResolveDataTableForRowStructFromConfiguredRoutes(
		DesiredRowStruct,
		OutDataTable,
		OutMatchedRootTag,
		OutError);
}
