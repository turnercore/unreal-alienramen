#include "ARUnlockReactiveComponent.h"

#include "ARGameStateBase.h"
#include "ARLog.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace ARUnlockReactiveComponentLog
{
	FString DescribeTagContainer(const FGameplayTagContainer& Tags)
	{
		return Tags.IsEmpty() ? TEXT("<empty>") : Tags.ToStringSimple();
	}

	FString DescribeMissingRequiredTags(const FGameplayTagContainer& RequiredTags, const FGameplayTagContainer& Unlocks)
	{
		TArray<FString> MissingTags;
		MissingTags.Reserve(RequiredTags.Num());

		for (const FGameplayTag& RequiredTag : RequiredTags)
		{
			if (!RequiredTag.IsValid() || Unlocks.HasTag(RequiredTag))
			{
				continue;
			}

			MissingTags.Add(RequiredTag.ToString());
		}

		return MissingTags.Num() > 0 ? FString::Join(MissingTags, TEXT(", ")) : TEXT("<none>");
	}
}

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
		UE_LOG(
			ARLog,
			Log,
			TEXT("[UnlockReactive] Component='%s' Owner='%s' could not bind because no AARGameStateBase was available."),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()));
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

	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' bound to GameState='%s'."),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(GameState));
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

	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' unbound from GameState='%s'."),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(GameState));
	BoundGameState = nullptr;
}

AARGameStateBase* UARUnlockReactiveComponent::ResolveGameState() const
{
	const UWorld* const World = GetWorld();
	if (World)
	{
		if (AARGameStateBase* const WorldGameState = World->GetGameState<AARGameStateBase>())
		{
			return WorldGameState;
		}
	}

	return BoundGameState.Get();
}

void UARUnlockReactiveComponent::RefreshFromGameState()
{
	BindToGameState();

	AARGameStateBase* const GameState = ResolveGameState();
	if (!GameState)
	{
		if (RequiredUnlockTags.IsEmpty())
		{
			UE_LOG(
				ARLog,
				Log,
				TEXT("[UnlockReactive] Component='%s' Owner='%s' refresh found no GameState. RequiredTags=%s -> unlocked path."),
				*GetNameSafe(this),
				*GetNameSafe(GetOwner()),
				*ARUnlockReactiveComponentLog::DescribeTagContainer(RequiredUnlockTags));
			ApplyUnlockedAndUpgradeState(FGameplayTagContainer::EmptyContainer);
			return;
		}

		UE_LOG(
			ARLog,
			Log,
			TEXT("[UnlockReactive] Component='%s' Owner='%s' refresh found no GameState. RequiredTags=%s -> locked path."),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*ARUnlockReactiveComponentLog::DescribeTagContainer(RequiredUnlockTags));
		ApplyLockedState();
		return;
	}

	const FGameplayTagContainer& Unlocks = GameState->GetUnlocks();
	if (!GameState->HasHydratedFromSave() && !RequiredUnlockTags.IsEmpty())
	{
		UE_LOG(
			ARLog,
			Log,
			TEXT("[UnlockReactive] Component='%s' Owner='%s' waiting for hydration. RequiredTags=%s CurrentUnlocks=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*ARUnlockReactiveComponentLog::DescribeTagContainer(RequiredUnlockTags),
			*ARUnlockReactiveComponentLog::DescribeTagContainer(Unlocks));
		return;
	}

	const bool bNowUnlocked = RequiredUnlockTags.IsEmpty() || Unlocks.HasAll(RequiredUnlockTags);

	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' refresh evaluated Hydrated=%s RequiredTags=%s Unlocks=%s MissingRequired=%s Unlocked=%s AuthoredUpgrades=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		GameState->HasHydratedFromSave() ? TEXT("true") : TEXT("false"),
		*ARUnlockReactiveComponentLog::DescribeTagContainer(RequiredUnlockTags),
		*ARUnlockReactiveComponentLog::DescribeTagContainer(Unlocks),
		*ARUnlockReactiveComponentLog::DescribeMissingRequiredTags(RequiredUnlockTags, Unlocks),
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

	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' applied LOCKED state."),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()));
	OnLocked.Broadcast();
}

void UARUnlockReactiveComponent::ApplyUnlockedAndUpgradeState(const FGameplayTagContainer& Unlocks)
{
	bIsUnlocked = true;

	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' applied UNLOCKED state. Unlocks=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*ARUnlockReactiveComponentLog::DescribeTagContainer(Unlocks));
	OnUnlocked.Broadcast();

	ActiveUpgradeTags.Reset();
	for (const FGameplayTag& UpgradeTag : OrderedUpgradeTags)
	{
		if (!UpgradeTag.IsValid() || !Unlocks.HasTag(UpgradeTag))
		{
			continue;
		}

		ActiveUpgradeTags.AddTag(UpgradeTag);
		UE_LOG(
			ARLog,
			Log,
			TEXT("[UnlockReactive] Component='%s' Owner='%s' replaying upgrade '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*UpgradeTag.ToString());
		OnUpgrade.Broadcast(UpgradeTag);
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
	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' received OnHydratedFromSave."),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()));
	RefreshFromGameState();
}

void UARUnlockReactiveComponent::HandleUnlocksChanged(FGameplayTagContainer NewUnlocks, FGameplayTagContainer OldUnlocks)
{
	UE_LOG(
		ARLog,
		Log,
		TEXT("[UnlockReactive] Component='%s' Owner='%s' received OnUnlocksChanged Old=%s New=%s."),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*ARUnlockReactiveComponentLog::DescribeTagContainer(OldUnlocks),
		*ARUnlockReactiveComponentLog::DescribeTagContainer(NewUnlocks));
	RefreshFromGameState();
}
