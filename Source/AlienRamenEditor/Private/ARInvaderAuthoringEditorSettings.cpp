#include "ARInvaderAuthoringEditorSettings.h"
#include "Engine/World.h"

UARInvaderAuthoringEditorSettings::UARInvaderAuthoringEditorSettings()
{
	if (DefaultTestMap.IsNull())
	{
		DefaultTestMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Lvl_InvaderDebug.Lvl_InvaderDebug")));
	}
}
