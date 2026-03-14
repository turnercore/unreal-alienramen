#include "ParleyDialogueEditorSettings.h"

UParleyDialogueEditorSettings::UParleyDialogueEditorSettings()
{
	if (ConversationAssetsFolder.Path.IsEmpty())
	{
		ConversationAssetsFolder.Path = TEXT("/Game/Data/Conversations");
	}
}
