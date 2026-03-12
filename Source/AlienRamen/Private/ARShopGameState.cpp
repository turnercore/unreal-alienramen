#include "ARShopGameState.h"
#include "ARGameStateModeStructs.h"

AARShopGameState::AARShopGameState()
{
	ClassStateStruct = FARShopGameStateData::StaticStruct();
}

UScriptStruct* AARShopGameState::GetStateStruct_Implementation() const
{
	return ClassStateStruct ? ClassStateStruct.Get() : FARShopGameStateData::StaticStruct();
}
