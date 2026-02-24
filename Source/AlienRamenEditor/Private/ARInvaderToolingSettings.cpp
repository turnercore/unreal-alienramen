#include "ARInvaderToolingSettings.h"

UARInvaderToolingSettings::UARInvaderToolingSettings()
{
	if (WaveDataTable.IsNull())
	{
		WaveDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Waves.DT_Waves")));
	}

	if (StageDataTable.IsNull())
	{
		StageDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Stages.DT_Stages")));
	}

	if (EnemiesFolder.Path.IsEmpty())
	{
		EnemiesFolder.Path = TEXT("/Game/CodeAlong/Blueprints/Enemies");
	}

	if (BackupsFolder.Path.IsEmpty())
	{
		BackupsFolder.Path = TEXT("/Game/Data/Backups");
	}

	if (PIELoadSlotName.IsNone())
	{
		PIELoadSlotName = TEXT("Debug");
	}
}
