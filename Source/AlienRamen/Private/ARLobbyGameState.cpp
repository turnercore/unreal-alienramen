#include "ARLobbyGameState.h"
#include "ARGameStateModeStructs.h"

AARLobbyGameState::AARLobbyGameState()
{
	ClassStateStruct = FARLobbyGameStateData::StaticStruct();
}

UScriptStruct* AARLobbyGameState::GetStateStruct_Implementation() const
{
	return ClassStateStruct ? ClassStateStruct.Get() : FARLobbyGameStateData::StaticStruct();
}
