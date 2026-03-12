#include "ARInvaderGameMode.h"

AARInvaderGameMode::AARInvaderGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[InvaderGameMode] Required gameplay tag 'Mode.Invader' is missing."));
	bAutosaveOnQuit = false;
	bAllowManualSaveInMode = false;
	bShareLocalPauseAcrossControllersInMode = true;
}
