#include "ARProjectileBase.h"

#include "ARInvaderCollisionChannels.h"
#include "ARInvaderDirectorSettings.h"
#include "ARLog.h"
#include "HelperLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"

AARProjectileBase::AARProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

bool AARProjectileBase::InitializeProjectileFromData(const FInstancedStruct& InitData)
{
	if (!InitData.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[ProjectileBase] InitializeProjectileFromData invalid struct on '%s'."), *GetNameSafe(this));
		return false;
	}

	// Apply shared actor-level fields first.
	UHelperLibrary::ApplyStructToObjectByName(this, InitData);

	// Apply the same payload to movement components for common speed/acceleration fields.
	TArray<UProjectileMovementComponent*> ProjectileMovementComponents;
	GetComponents<UProjectileMovementComponent>(ProjectileMovementComponents);
	for (UProjectileMovementComponent* ProjectileMovement : ProjectileMovementComponents)
	{
		if (!ProjectileMovement)
		{
			continue;
		}

		UHelperLibrary::ApplyStructToObjectByName(ProjectileMovement, InitData);
	}

	return true;
}

void AARProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	UPrimitiveComponent* PreferredCollisionPrimitive = nullptr;

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		PreferredCollisionPrimitive = PrimitiveComponent;
		if (PrimitiveComponent == GetRootComponent())
		{
			break;
		}
	}

	if (bAutoWireCollisionAndMovement)
	{
		if (!GetRootComponent() && PreferredCollisionPrimitive)
		{
			SetRootComponent(PreferredCollisionPrimitive);
			UE_LOG(ARLog, Verbose, TEXT("[ProjectileBase] Auto-wired root component to '%s' for '%s'."),
				*GetNameSafe(PreferredCollisionPrimitive),
				*GetNameSafe(this));
		}

		if (PreferredCollisionPrimitive)
		{
			TArray<UProjectileMovementComponent*> ProjectileMovementComponents;
			GetComponents<UProjectileMovementComponent>(ProjectileMovementComponents);
			for (UProjectileMovementComponent* ProjectileMovement : ProjectileMovementComponents)
			{
				if (!ProjectileMovement || ProjectileMovement->UpdatedComponent)
				{
					continue;
				}

				ProjectileMovement->SetUpdatedComponent(PreferredCollisionPrimitive);
				UE_LOG(ARLog, Verbose, TEXT("[ProjectileBase] Auto-wired ProjectileMovement '%s' UpdatedComponent='%s' for '%s'."),
					*GetNameSafe(ProjectileMovement),
					*GetNameSafe(PreferredCollisionPrimitive),
					*GetNameSafe(this));
			}
		}
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionObjectType(ARInvaderCollisionChannels::Projectile);
		if (bApplyDefaultInvaderCollisionResponses)
		{
			PrimitiveComponent->SetCollisionResponseToChannel(ARInvaderCollisionChannels::Projectile, ECR_Ignore);
			PrimitiveComponent->SetCollisionResponseToChannel(ARInvaderCollisionChannels::Drop, ECR_Ignore);
		}
	}

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
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	EvaluateOffscreenReleaseInternal(DeltaSeconds);
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
