#include "ARInvaderGameState.h"
#include "ARGameStateModeStructs.h"

AARInvaderGameState::AARInvaderGameState()
{
	ClassStateStruct = FARInvaderGameStateData::StaticStruct();
}

UScriptStruct* AARInvaderGameState::GetStateStruct_Implementation() const
{
	return FARInvaderGameStateData::StaticStruct();
}
