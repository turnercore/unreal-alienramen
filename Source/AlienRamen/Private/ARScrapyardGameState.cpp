#include "ARScrapyardGameState.h"
#include "ARGameStateModeStructs.h"

AARScrapyardGameState::AARScrapyardGameState()
{
	ClassStateStruct = FARScrapyardGameStateData::StaticStruct();
}

UScriptStruct* AARScrapyardGameState::GetStateStruct_Implementation() const
{
	return FARScrapyardGameStateData::StaticStruct();
}
