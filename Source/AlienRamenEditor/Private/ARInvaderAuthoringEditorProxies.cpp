#include "ARInvaderAuthoringEditorProxies.h"

void UARInvaderPIESaveLoadedBridge::Configure(FSimpleDelegate InOnLoaded)
{
	OnLoaded = MoveTemp(InOnLoaded);
}

void UARInvaderPIESaveLoadedBridge::HandleSignalOnGameLoaded()
{
	OnLoaded.ExecuteIfBound();
}
