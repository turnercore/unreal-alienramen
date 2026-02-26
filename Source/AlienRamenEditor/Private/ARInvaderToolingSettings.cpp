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

	if (EnemyDataTable.IsNull())
	{
		EnemyDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_InvaderEnemies.DT_InvaderEnemies")));
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
