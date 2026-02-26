#include "ARPlayerStateBase.h"

#include "ARLog.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "ARAttributeSetCore.h"

void AARPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARPlayerStateBase, LoadoutTags);
}

void AARPlayerStateBase::OnRep_Loadout()
{
	// Optional: notify UI, fire delegate, etc.
}
AARPlayerStateBase::AARPlayerStateBase()
{
	// PlayerState replicates by default; still safe to be explicit
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Good default for players; you can revisit later
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
}

UAbilitySystemComponent* AARPlayerStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AARPlayerStateBase::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaultLoadoutIfEmpty();
}

bool AARPlayerStateBase::ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	if (!SavedState.IsValid())
	{
		return false;
	}

	if (!HasAuthority())
	{
		ServerApplyStateFromStruct(SavedState);
		return true;
	}

	return IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARPlayerStateBase::ServerApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
	EnsureDefaultLoadoutIfEmpty();
}

void AARPlayerStateBase::EnsureDefaultLoadoutIfEmpty()
{
	if (!HasAuthority() || !LoadoutTags.IsEmpty())
	{
		return;
	}

	auto TryAddTag = [this](const TCHAR* TagName)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		if (Tag.IsValid())
		{
			LoadoutTags.AddTag(Tag);
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[ShipGAS] Default loadout tag is missing: %s"), TagName);
		}
	};

	TryAddTag(TEXT("Unlock.Ship.Sammy"));
	TryAddTag(TEXT("Unlock.Gadget.Vac"));
	TryAddTag(TEXT("Unlock.Secondary.Mine"));

	if (!LoadoutTags.IsEmpty())
	{
		ForceNetUpdate();
		UE_LOG(ARLog, Log, TEXT("[ShipGAS] Applied default loadout tags: %s"), *LoadoutTags.ToStringSimple());
	}
}
