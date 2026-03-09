#include "TagContentResolverEditorHelpers.h"

#include "TagContentResolverSubsystem.h"

bool FTagContentResolverEditorHelpers::TryResolveDataTableForRootTag(FGameplayTag RootTag, UDataTable*& OutDataTable, FString& OutError)
{
	return UTagContentResolverSubsystem::TryResolveDataTableForRootTagFromConfiguredRoutes(RootTag, OutDataTable, OutError);
}

bool FTagContentResolverEditorHelpers::TryResolveDataTableForRowStruct(
	UScriptStruct* DesiredRowStruct,
	UDataTable*& OutDataTable,
	FGameplayTag& OutMatchedRootTag,
	FString& OutError)
{
	return UTagContentResolverSubsystem::TryResolveDataTableForRowStructFromConfiguredRoutes(
		DesiredRowStruct,
		OutDataTable,
		OutMatchedRootTag,
		OutError);
}
