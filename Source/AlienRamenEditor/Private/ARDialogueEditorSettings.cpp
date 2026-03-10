#include "ARDialogueEditorSettings.h"

UARDialogueEditorSettings::UARDialogueEditorSettings()
{
	if (ConversationAssetsFolder.Path.IsEmpty())
	{
		ConversationAssetsFolder.Path = TEXT("/Game/Data/Conversations");
	}
}
