#include "ARInvaderGameMode.h"

AARInvaderGameMode::AARInvaderGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
	bAutosaveOnQuit = false;
	bAllowManualSaveInMode = false;
}
