#include "ARProjectileBase.h"

#include "ARInvaderDirectorSettings.h"
#include "ARLog.h"

AARProjectileBase::AARProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AARProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	if (bUseProjectSettingsOffscreenCullSeconds)
	{
		const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
		if (Settings)
		{
			OffscreenReleaseDelay = FMath::Max(0.f, Settings->ProjectileOffscreenCullSeconds);
		}
	}

	EvaluateOffscreenReleaseInternal(0.f);
}

void AARProjectileBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EvaluateOffscreenReleaseInternal(DeltaSeconds);
}

void AARProjectileBase::EvaluateOffscreenRelease()
{
	EvaluateOffscreenReleaseInternal(0.f);
}

void AARProjectileBase::EvaluateOffscreenReleaseInternal(float DeltaSeconds)
{
	if (bReleased || !bReleaseWhenOutsideGameplayBounds)
	{
		return;
	}

	if (bOffscreenCheckAuthorityOnly && !HasAuthority())
	{
		return;
	}

	const bool bOffscreen = IsOutsideGameplayBounds();
	if (!bOffscreen)
	{
		OffscreenSeconds = 0.f;
		return;
	}

	OffscreenSeconds += FMath::Max(0.f, DeltaSeconds);
	if (OffscreenSeconds < FMath::Max(0.f, OffscreenReleaseDelay))
	{
		return;
	}

	bReleased = true;
	UE_LOG(ARLog, Verbose, TEXT("[ProjectileBase] Offscreen release for '%s' after %.2fs at (%.1f, %.1f, %.1f)."),
		*GetNameSafe(this),
		OffscreenSeconds,
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z);

	BP_OnProjectilePreRelease();
	ReleaseProjectile();
}

bool AARProjectileBase::IsOutsideGameplayBounds() const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!Settings)
	{
		return false;
	}

	const FVector Location = GetActorLocation();
	const float Margin = FMath::Max(0.f, OffscreenReleaseMargin);
	const float MinX = Settings->GameplayBoundsMin.X - Margin;
	const float MaxX = Settings->GameplayBoundsMax.X + Margin;
	const float MinY = Settings->GameplayBoundsMin.Y - Margin;
	const float MaxY = Settings->GameplayBoundsMax.Y + Margin;

	return Location.X < MinX
		|| Location.X > MaxX
		|| Location.Y < MinY
		|| Location.Y > MaxY;
}

void AARProjectileBase::ReleaseProjectile_Implementation()
{
	Destroy();
}
