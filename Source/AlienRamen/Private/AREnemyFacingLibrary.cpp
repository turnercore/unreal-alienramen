#include "AREnemyFacingLibrary.h"

#include "ARInvaderDirectorSettings.h"

void UAREnemyFacingLibrary::ReorientEnemyFacingDown(
	AActor* EnemyActor,
	bool bUseDirectorSettingsOffset,
	float AdditionalYawOffset,
	bool bZeroPitchAndRoll)
{
	if (!EnemyActor)
	{
		return;
	}

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const float SettingsOffset = (bUseDirectorSettingsOffset && Settings) ? Settings->SpawnFacingYawOffset : 0.f;
	const float TargetYaw = 180.f + SettingsOffset + AdditionalYawOffset;

	FRotator NewRotation = EnemyActor->GetActorRotation();
	NewRotation.Yaw = TargetYaw;
	if (bZeroPitchAndRoll)
	{
		NewRotation.Pitch = 0.f;
		NewRotation.Roll = 0.f;
	}

	EnemyActor->SetActorRotation(NewRotation);
}

