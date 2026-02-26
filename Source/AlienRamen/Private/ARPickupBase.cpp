#include "ARPickupBase.h"

#include "ARInvaderDirectorSettings.h"
#include "ARLog.h"

AARPickupBase::AARPickupBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AARPickupBase::BeginPlay()
{
	Super::BeginPlay();
	EvaluateOffscreenRelease(0.f);
}

void AARPickupBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EvaluateOffscreenRelease(DeltaSeconds);
}

void AARPickupBase::EvaluateOffscreenRelease(float DeltaSeconds)
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
	UE_LOG(ARLog, Verbose, TEXT("[PickupBase] Offscreen release for '%s' after %.2fs at (%.1f, %.1f, %.1f)."),
		*GetNameSafe(this),
		OffscreenSeconds,
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z);

	BP_OnPickupPreRelease();
	ReleasePickup();
}

bool AARPickupBase::IsOutsideGameplayBounds() const
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

void AARPickupBase::ReleasePickup_Implementation()
{
	Destroy();
}

