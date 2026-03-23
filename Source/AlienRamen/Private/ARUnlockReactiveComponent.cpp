#include "ARUnlockReactiveComponent.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UARUnlockReactiveComponent::UARUnlockReactiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UARUnlockReactiveComponent::BeginPlay()
{
	Super::BeginPlay();

	BindToGameState();
	RefreshFromGameState();
}

void UARUnlockReactiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromGameState();
	Super::EndPlay(EndPlayReason);
}

void UARUnlockReactiveComponent::BindToGameState()
{
	AARGameStateBase* const GameState = ResolveGameState();
	if (!GameState)
	{
		UE_LOG(ARLog, Verbose, TEXT("[UnlockReactive] '%s' could not bind: no AARGameStateBase."), *GetNameSafe(this));
		return;
	}

	if (BoundGameState.Get() == GameState)
	{
		return;
	}

	UnbindFromGameState();

	GameState->OnHydratedFromSave.AddUniqueDynamic(this, &UARUnlockReactiveComponent::HandleHydratedFromSave);
	GameState->OnUnlocksChanged.AddUniqueDynamic(this, &UARUnlockReactiveComponent::HandleUnlocksChanged);
	BoundGameState = GameState;

	UE_LOG(ARLog, Verbose, TEXT("[UnlockReactive] '%s' bound to GameState '%s'."), *GetNameSafe(this), *GetNameSafe(GameState));
}

void UARUnlockReactiveComponent::UnbindFromGameState()
{
	AARGameStateBase* const GameState = BoundGameState.Get();
	if (!GameState)
	{
		BoundGameState = nullptr;
		return;
	}

	GameState->OnHydratedFromSave.RemoveDynamic(this, &UARUnlockReactiveComponent::HandleHydratedFromSave);
	GameState->OnUnlocksChanged.RemoveDynamic(this, &UARUnlockReactiveComponent::HandleUnlocksChanged);

	UE_LOG(ARLog, Verbose, TEXT("[UnlockReactive] '%s' unbound from GameState '%s'."), *GetNameSafe(this), *GetNameSafe(GameState));
	BoundGameState = nullptr;
}

AARGameStateBase* UARUnlockReactiveComponent::ResolveGameState() const
{
	if (AARGameStateBase* const Existing = BoundGameState.Get())
	{
		return Existing;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UARUnlockReactiveComponent* const MutableThis = const_cast<UARUnlockReactiveComponent*>(this);
	AARGameStateBase* const Resolved = World->GetGameState<AARGameStateBase>();
	MutableThis->BoundGameState = Resolved;
	return Resolved;
}

void UARUnlockReactiveComponent::RefreshFromGameState()
{
	AARGameStateBase* const GameState = ResolveGameState();
	if (!GameState)
	{
		if (RequiredUnlockTags.IsEmpty())
		{
			UE_LOG(ARLog, Verbose, TEXT("[UnlockReactive] '%s' refresh with no GameState and no required tags -> unlocked path."), *GetNameSafe(this));
			ApplyUnlockedAndUpgradeState(FGameplayTagContainer::EmptyContainer);
			return;
		}

		UE_LOG(ARLog, Verbose, TEXT("[UnlockReactive] '%s' refresh with no GameState -> locked path."), *GetNameSafe(this));
		ApplyLockedState();
		return;
	}

	const FGameplayTagContainer& Unlocks = GameState->GetUnlocks();
	const bool bNowUnlocked = RequiredUnlockTags.IsEmpty() || Unlocks.HasAll(RequiredUnlockTags);

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[UnlockReactive] '%s' refresh: required=%d unlocks=%d unlocked=%s upgradesAuthored=%d"),
		*GetNameSafe(this),
		RequiredUnlockTags.Num(),
		Unlocks.Num(),
		bNowUnlocked ? TEXT("true") : TEXT("false"),
		OrderedUpgradeTags.Num());

	if (!bNowUnlocked)
	{
		ApplyLockedState();
		return;
	}

	ApplyUnlockedAndUpgradeState(Unlocks);
}

bool UARUnlockReactiveComponent::HasUpgradeTag(const FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() && ActiveUpgradeTags.HasTag(UpgradeTag);
}

void UARUnlockReactiveComponent::SetOwnerFullyDisabled(const bool bDisabled)
{
	AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UPrimitiveComponent*> TargetComponents;
	GatherTargetPrimitiveComponents(TargetComponents);

	if (bDisabled)
	{
		if (!bHasCachedDisableState)
		{
			bCachedOwnerCollisionEnabled = Owner->GetActorEnableCollision();
			CachedComponentCollisionModes.Reset();
			for (UPrimitiveComponent* const PrimitiveComponent : TargetComponents)
			{
				if (PrimitiveComponent)
				{
					CachedComponentCollisionModes.Add(PrimitiveComponent, PrimitiveComponent->GetCollisionEnabled());
				}
			}
			bHasCachedDisableState = true;
		}

		Owner->SetActorHiddenInGame(true);
		Owner->SetActorEnableCollision(false);
		for (UPrimitiveComponent* const PrimitiveComponent : TargetComponents)
		{
			if (PrimitiveComponent)
			{
				PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
		return;
	}

	Owner->SetActorHiddenInGame(false);

	if (bHasCachedDisableState)
	{
		Owner->SetActorEnableCollision(bCachedOwnerCollisionEnabled);

		for (UPrimitiveComponent* const PrimitiveComponent : TargetComponents)
		{
			if (!PrimitiveComponent)
			{
				continue;
			}

			if (const ECollisionEnabled::Type* const CachedMode = CachedComponentCollisionModes.Find(PrimitiveComponent))
			{
				PrimitiveComponent->SetCollisionEnabled(*CachedMode);
			}
		}

		CachedComponentCollisionModes.Reset();
		bHasCachedDisableState = false;
		return;
	}

	Owner->SetActorEnableCollision(true);
}

void UARUnlockReactiveComponent::ApplyLockedState()
{
	bIsUnlocked = false;
	ActiveUpgradeTags.Reset();
	OnLocked();
}

void UARUnlockReactiveComponent::ApplyUnlockedAndUpgradeState(const FGameplayTagContainer& Unlocks)
{
	bIsUnlocked = true;
	OnUnlocked();

	ActiveUpgradeTags.Reset();
	for (const FGameplayTag& UpgradeTag : OrderedUpgradeTags)
	{
		if (!UpgradeTag.IsValid() || !Unlocks.HasTag(UpgradeTag))
		{
			continue;
		}

		ActiveUpgradeTags.AddTag(UpgradeTag);
		OnUpgrade(UpgradeTag);
	}
}

void UARUnlockReactiveComponent::GatherTargetPrimitiveComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	if (DisableCollisionComponents.Num() > 0)
	{
		for (UPrimitiveComponent* const PrimitiveComponent : DisableCollisionComponents)
		{
			if (IsValid(PrimitiveComponent))
			{
				OutComponents.Add(PrimitiveComponent);
			}
		}
		return;
	}

	const AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Owner->GetComponents<UPrimitiveComponent>(OutComponents);
}

void UARUnlockReactiveComponent::HandleHydratedFromSave()
{
	UE_LOG(ARLog, Verbose, TEXT("[UnlockReactive] '%s' received OnHydratedFromSave."), *GetNameSafe(this));
	RefreshFromGameState();
}

void UARUnlockReactiveComponent::HandleUnlocksChanged(FGameplayTagContainer NewUnlocks, FGameplayTagContainer OldUnlocks)
{
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[UnlockReactive] '%s' received OnUnlocksChanged old=%d new=%d."),
		*GetNameSafe(this),
		OldUnlocks.Num(),
		NewUnlocks.Num());
	RefreshFromGameState();
}
