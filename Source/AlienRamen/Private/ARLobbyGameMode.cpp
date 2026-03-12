#include "ARLobbyGameMode.h"

AARLobbyGameMode::AARLobbyGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Lobby"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[LobbyGameMode] Required gameplay tag 'Mode.Lobby' is missing."));
	bSaveOnModeExit = false;
	bAllowManualSaveInMode = false;
}
