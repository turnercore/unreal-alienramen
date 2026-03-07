#include "ARPickupCollectorComponent.h"

#include "ARAttributeSetCore.h"
#include "ARInvaderCollisionChannels.h"
#include "ARInvaderDropBase.h"
#include "ARPlayerCharacterInvader.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameplayEffectExtension.h"
#include "TimerManager.h"

UARPickupCollectorComponent::UARPickupCollectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UARPickupCollectorComponent::BeginPlay()
{
	Super::BeginPlay();
	SetupDetectorComponent();
	EnsurePickupRadiusBinding();
	RefreshDetectorRadius();
	StartRetryTimer();
}

void UARPickupCollectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRetryTimer();
	UnbindPickupRadiusDelegate();
	OverlappingDrops.Reset();
	LastServerRequestByDrop.Reset();
	Super::EndPlay(EndPlayReason);
}

void UARPickupCollectorComponent::SetupDetectorComponent()
{
	AARPlayerCharacterInvader* OwnerInvader = GetOwnerInvader();
	if (!OwnerInvader || PickupDetector)
	{
		return;
	}

	PickupDetector = NewObject<USphereComponent>(OwnerInvader, TEXT("PickupDetector"));
	if (!PickupDetector)
	{
		return;
	}

	PickupDetector->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupDetector->SetCollisionObjectType(ECC_WorldDynamic);
	PickupDetector->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupDetector->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	PickupDetector->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	PickupDetector->SetCollisionResponseToChannel(ARInvaderCollisionChannels::Drop, ECR_Overlap);
	PickupDetector->SetGenerateOverlapEvents(true);
	PickupDetector->SetSphereRadius(FMath::Max(1.0f, FallbackPickupRadius), true);

	if (USceneComponent* Root = OwnerInvader->GetRootComponent())
	{
		PickupDetector->SetupAttachment(Root);
	}

	OwnerInvader->AddOwnedComponent(PickupDetector);
	PickupDetector->RegisterComponent();
	PickupDetector->OnComponentBeginOverlap.AddDynamic(this, &UARPickupCollectorComponent::HandleDetectorBeginOverlap);
	PickupDetector->OnComponentEndOverlap.AddDynamic(this, &UARPickupCollectorComponent::HandleDetectorEndOverlap);
}

void UARPickupCollectorComponent::EnsurePickupRadiusBinding()
{
	AARPlayerCharacterInvader* OwnerInvader = GetOwnerInvader();
	if (!OwnerInvader)
	{
		return;
	}

	UAbilitySystemComponent* ASC = OwnerInvader->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (BoundASC.Get() == ASC && PickupRadiusChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindPickupRadiusDelegate();
	BoundASC = ASC;
	PickupRadiusChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetPickupRadiusAttribute())
		.AddUObject(this, &UARPickupCollectorComponent::HandlePickupRadiusAttributeChanged);
}

void UARPickupCollectorComponent::UnbindPickupRadiusDelegate()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (ASC && PickupRadiusChangedDelegateHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetPickupRadiusAttribute())
			.Remove(PickupRadiusChangedDelegateHandle);
	}

	PickupRadiusChangedDelegateHandle.Reset();
	BoundASC.Reset();
}

void UARPickupCollectorComponent::RefreshDetectorRadius()
{
	if (!PickupDetector)
	{
		return;
	}

	const float NewRadius = FMath::Max(1.0f, ResolvePickupRadius());
	if (FMath::IsNearlyEqual(CachedPickupRadius, NewRadius, 0.1f))
	{
		return;
	}

	CachedPickupRadius = NewRadius;
	PickupDetector->SetSphereRadius(NewRadius, true);
	ProcessAllOverlappingDrops();
}

void UARPickupCollectorComponent::StartRetryTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(RetryTimerHandle))
	{
		return;
	}

	const float TickInterval = FMath::Max(0.01f, ServerRequestRetrySeconds);
	World->GetTimerManager().SetTimer(RetryTimerHandle, this, &UARPickupCollectorComponent::HandleRetryTimerTick, TickInterval, true);
}

void UARPickupCollectorComponent::StopRetryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimerHandle);
	}
}

AARPlayerCharacterInvader* UARPickupCollectorComponent::GetOwnerInvader() const
{
	return Cast<AARPlayerCharacterInvader>(GetOwner());
}

float UARPickupCollectorComponent::ResolvePickupRadius() const
{
	const AARPlayerCharacterInvader* OwnerInvader = GetOwnerInvader();
	if (!OwnerInvader)
	{
		return 0.0f;
	}

	const UAbilitySystemComponent* ASC = OwnerInvader->GetAbilitySystemComponent();
	if (!ASC)
	{
		return FMath::Max(0.0f, FallbackPickupRadius);
	}

	const float AttrRadius = FMath::Max(0.0f, ASC->GetNumericAttribute(UARAttributeSetCore::GetPickupRadiusAttribute()));
	if (AttrRadius > 0.0f)
	{
		return AttrRadius;
	}

	return FMath::Max(0.0f, FallbackPickupRadius);
}

void UARPickupCollectorComponent::HandleRetryTimerTick()
{
	EnsurePickupRadiusBinding();
	PrunePendingRequests();
	ProcessAllOverlappingDrops();
}

void UARPickupCollectorComponent::ProcessAllOverlappingDrops()
{
	AARPlayerCharacterInvader* OwnerInvader = GetOwnerInvader();
	if (!OwnerInvader || !OwnerInvader->IsLocallyControlled() && !OwnerInvader->HasAuthority())
	{
		return;
	}

	for (auto It = OverlappingDrops.CreateIterator(); It; ++It)
	{
		AARInvaderDropBase* Drop = It->Get();
		if (!Drop || Drop->IsPendingKillPending())
		{
			It.RemoveCurrent();
			continue;
		}

		TryProcessDrop(Drop);
	}
}

void UARPickupCollectorComponent::TryProcessDrop(AARInvaderDropBase* Drop)
{
	AARPlayerCharacterInvader* OwnerInvader = GetOwnerInvader();
	UWorld* World = GetWorld();
	if (!OwnerInvader || !World || !Drop || Drop->IsPendingKillPending() || !Drop->IsAvailableForCollection())
	{
		return;
	}

	if (!Drop->IsPlayerWithinPickupRange2D(OwnerInvader))
	{
		return;
	}

	const bool bCanDriveAuthorityCollection = OwnerInvader->HasAuthority();
	const bool bCanDriveClientPrediction = OwnerInvader->IsLocallyControlled();
	if (!bCanDriveAuthorityCollection && !bCanDriveClientPrediction)
	{
		return;
	}

	if (bCanDriveClientPrediction && bEnableClientPrediction)
	{
		Drop->BeginPredictedCollectionLocal(OwnerInvader);
	}

	if (bCanDriveAuthorityCollection)
	{
		Drop->TryCollectByPlayer(OwnerInvader);
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (ShouldSendServerRequestForDrop(Drop, CurrentTime))
	{
		OwnerInvader->ServerRequestCollectInvaderDrop(Drop);
		LastServerRequestByDrop.FindOrAdd(Drop) = CurrentTime;
	}
}

void UARPickupCollectorComponent::PrunePendingRequests()
{
	for (auto It = OverlappingDrops.CreateIterator(); It; ++It)
	{
		AARInvaderDropBase* Drop = It->Get();
		if (!Drop || Drop->IsPendingKillPending())
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = LastServerRequestByDrop.CreateIterator(); It; ++It)
	{
		const AARInvaderDropBase* Drop = It.Key().Get();
		if (!Drop || Drop->IsPendingKillPending() || !Drop->IsAvailableForCollection())
		{
			It.RemoveCurrent();
		}
	}
}

bool UARPickupCollectorComponent::ShouldSendServerRequestForDrop(const AARInvaderDropBase* Drop, const float WorldTimeSeconds) const
{
	if (!Drop)
	{
		return false;
	}

	const float* LastTime = LastServerRequestByDrop.Find(Drop);
	if (!LastTime)
	{
		return true;
	}

	return (WorldTimeSeconds - *LastTime) >= FMath::Max(0.01f, ServerRequestRetrySeconds);
}

void UARPickupCollectorComponent::HandlePickupRadiusAttributeChanged(const FOnAttributeChangeData& /*ChangeData*/)
{
	RefreshDetectorRadius();
}

void UARPickupCollectorComponent::HandleDetectorBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	AARInvaderDropBase* Drop = Cast<AARInvaderDropBase>(OtherActor);
	if (!Drop)
	{
		return;
	}

	OverlappingDrops.Add(Drop);
	TryProcessDrop(Drop);
}

void UARPickupCollectorComponent::HandleDetectorEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	AARInvaderDropBase* Drop = Cast<AARInvaderDropBase>(OtherActor);
	if (!Drop)
	{
		return;
	}

	OverlappingDrops.Remove(Drop);
	LastServerRequestByDrop.Remove(Drop);
}
