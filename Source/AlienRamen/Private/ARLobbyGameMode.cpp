#include "ARLobbyGameMode.h"

AARLobbyGameMode::AARLobbyGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Lobby"), false);
}
