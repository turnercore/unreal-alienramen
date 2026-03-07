#include "ARLobbyGameMode.h"

AARLobbyGameMode::AARLobbyGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Lobby"), false);
	bSaveOnModeExit = false;
	bAllowManualSaveInMode = false;
}
