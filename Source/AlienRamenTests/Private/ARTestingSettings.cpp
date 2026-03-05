#include "ARTestingSettings.h"

UARTestingSettings::UARTestingSettings()
{
	DefaultTestPlayerSlot = FName(TEXT("Auto"));
	DefaultTestCharacter = EARCharacterChoice::Brother;
	bAllowNetworkedTests = true;
	ReplicationWaitTimeoutSeconds = 5.0f;
	DebugSaveRevision = -1;
}

