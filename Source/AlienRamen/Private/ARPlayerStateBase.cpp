#include "ARPlayerStateBase.h"

#include "ARAttributeSetCore.h"
#include "ARLog.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "StructSerializable.h"

void AARPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARPlayerStateBase, LoadoutTags);
	DOREPLIFETIME(AARPlayerStateBase, PlayerSlot);
	DOREPLIFETIME(AARPlayerStateBase, CharacterPicked);
	DOREPLIFETIME(AARPlayerStateBase, DisplayName);
	DOREPLIFETIME(AARPlayerStateBase, bIsReady);
}

void AARPlayerStateBase::OnRep_Loadout()
{
	// Optional: notify UI, fire delegate, etc.
}

AARPlayerStateBase::AARPlayerStateBase()
{
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
	DisplayName = GetPlayerName();
}

UAbilitySystemComponent* AARPlayerStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AARPlayerStateBase::GetCoreAttributeValue(EARCoreAttributeType AttributeType) const
{
	if (!AbilitySystemComponent)
	{
		return 0.f;
	}

	switch (AttributeType)
	{
	case EARCoreAttributeType::Health:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	case EARCoreAttributeType::MaxHealth:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMaxHealthAttribute());
	case EARCoreAttributeType::Spice:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetSpiceAttribute());
	case EARCoreAttributeType::MaxSpice:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMaxSpiceAttribute());
	case EARCoreAttributeType::MoveSpeed:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMoveSpeedAttribute());
	default:
		return 0.f;
	}
}

FARPlayerCoreAttributeSnapshot AARPlayerStateBase::GetCoreAttributeSnapshot() const
{
	FARPlayerCoreAttributeSnapshot Snapshot;
	Snapshot.Health = GetCoreAttributeValue(EARCoreAttributeType::Health);
	Snapshot.MaxHealth = GetCoreAttributeValue(EARCoreAttributeType::MaxHealth);
	Snapshot.Spice = GetCoreAttributeValue(EARCoreAttributeType::Spice);
	Snapshot.MaxSpice = GetCoreAttributeValue(EARCoreAttributeType::MaxSpice);
	Snapshot.MoveSpeed = GetCoreAttributeValue(EARCoreAttributeType::MoveSpeed);
	return Snapshot;
}

float AARPlayerStateBase::GetSpiceNormalized() const
{
	const float MaxSpice = GetCoreAttributeValue(EARCoreAttributeType::MaxSpice);
	if (MaxSpice <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Clamp(GetCoreAttributeValue(EARCoreAttributeType::Spice) / MaxSpice, 0.f, 1.f);
}

int32 AARPlayerStateBase::GetHUDPlayerSlotIndex() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return INDEX_NONE;
	}

	return GS->PlayerArray.IndexOfByKey(this);
}

void AARPlayerStateBase::SetPlayerSlot(EARPlayerSlot NewSlot)
{
	if (!HasAuthority() || PlayerSlot == NewSlot)
	{
		return;
	}

	const EARPlayerSlot OldSlot = PlayerSlot;
	PlayerSlot = NewSlot;
	OnRep_PlayerSlot(OldSlot);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetCharacterPicked(EARCharacterChoice NewCharacter)
{
	if (HasAuthority())
	{
		SetCharacterPicked_Internal(NewCharacter);
		return;
	}

	ServerPickCharacter(NewCharacter);
}

void AARPlayerStateBase::ServerPickCharacter_Implementation(EARCharacterChoice NewCharacter)
{
	SetCharacterPicked_Internal(NewCharacter);
}

void AARPlayerStateBase::SetDisplayNameValue(const FString& NewDisplayName)
{
	if (HasAuthority())
	{
		SetDisplayName_Internal(NewDisplayName);
		return;
	}

	ServerUpdateDisplayName(NewDisplayName);
}

void AARPlayerStateBase::ServerUpdateDisplayName_Implementation(const FString& NewDisplayName)
{
	SetDisplayName_Internal(NewDisplayName);
}

void AARPlayerStateBase::SetReadyForRun(bool bNewReady)
{
	if (HasAuthority())
	{
		SetReady_Internal(bNewReady);
		return;
	}

	ServerUpdateReady(bNewReady);
}

void AARPlayerStateBase::ServerUpdateReady_Implementation(bool bNewReady)
{
	SetReady_Internal(bNewReady);
}

void AARPlayerStateBase::SetSpiceMeter(float NewSpiceValue)
{
	if (HasAuthority())
	{
		SetSpiceMeter_Internal(NewSpiceValue);
		return;
	}

	ServerSetSpiceMeter(NewSpiceValue);
}

void AARPlayerStateBase::ClearSpiceMeter()
{
	SetSpiceMeter(0.f);
}

void AARPlayerStateBase::ServerSetSpiceMeter_Implementation(float NewSpiceValue)
{
	SetSpiceMeter_Internal(NewSpiceValue);
}

void AARPlayerStateBase::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaultLoadoutIfEmpty();
	BindTrackedAttributeDelegates();
	BroadcastTrackedAttributeSnapshot();
}

void AARPlayerStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindTrackedAttributeDelegates();
	Super::EndPlay(EndPlayReason);
}

void AARPlayerStateBase::OnRep_PlayerSlot(EARPlayerSlot OldSlot)
{
	OnPlayerSlotChanged.Broadcast(PlayerSlot, OldSlot);
}

void AARPlayerStateBase::OnRep_CharacterPicked(EARCharacterChoice OldCharacter)
{
	OnCharacterPickedChanged.Broadcast(this, PlayerSlot, CharacterPicked, OldCharacter);
}

void AARPlayerStateBase::OnRep_DisplayName(const FString& OldDisplayName)
{
	OnDisplayNameChanged.Broadcast(this, PlayerSlot, DisplayName, OldDisplayName);
}

void AARPlayerStateBase::OnRep_IsReady(bool bOldReady)
{
	OnReadyStatusChanged.Broadcast(this, PlayerSlot, bIsReady, bOldReady);
}

void AARPlayerStateBase::SetCharacterPicked_Internal(EARCharacterChoice NewCharacter)
{
	if (!HasAuthority() || CharacterPicked == NewCharacter)
	{
		return;
	}

	const EARCharacterChoice OldCharacter = CharacterPicked;
	CharacterPicked = NewCharacter;
	OnRep_CharacterPicked(OldCharacter);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetDisplayName_Internal(const FString& NewDisplayName)
{
	if (!HasAuthority())
	{
		return;
	}

	const FString SanitizedName = NewDisplayName.TrimStartAndEnd();
	if (DisplayName == SanitizedName)
	{
		return;
	}

	const FString OldDisplayName = DisplayName;
	DisplayName = SanitizedName;
	SetPlayerName(DisplayName);
	OnRep_DisplayName(OldDisplayName);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetReady_Internal(bool bNewReady)
{
	if (!HasAuthority() || bIsReady == bNewReady)
	{
		return;
	}

	const bool bOldReady = bIsReady;
	bIsReady = bNewReady;
	OnRep_IsReady(bOldReady);
	ForceNetUpdate();
}

void AARPlayerStateBase::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	AARPlayerStateBase* TargetPS = Cast<AARPlayerStateBase>(PlayerState);
	if (!TargetPS)
	{
		return;
	}

	if (GetClass()->ImplementsInterface(UStructSerializable::StaticClass())
		&& TargetPS->GetClass()->ImplementsInterface(UStructSerializable::StaticClass()))
	{
		FInstancedStruct CurrentState;
		IStructSerializable::Execute_ExtractStateToStruct(this, CurrentState);
		IStructSerializable::Execute_ApplyStateFromStruct(TargetPS, CurrentState);
	}

	TargetPS->SetPlayerSlot(PlayerSlot);
	TargetPS->LoadoutTags = LoadoutTags;
	TargetPS->SetCharacterPicked(CharacterPicked);
	TargetPS->SetDisplayNameValue(DisplayName);

	// Ready status is transient and should reset after travel handoff.
	TargetPS->SetReadyForRun(false);
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

void AARPlayerStateBase::BindTrackedAttributeDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!HealthChangedDelegateHandle.IsValid())
	{
		HealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleHealthAttributeChanged);
	}

	if (!MaxHealthChangedDelegateHandle.IsValid())
	{
		MaxHealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxHealthAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleMaxHealthAttributeChanged);
	}

	if (!SpiceChangedDelegateHandle.IsValid())
	{
		SpiceChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetSpiceAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleSpiceAttributeChanged);
	}

	if (!MaxSpiceChangedDelegateHandle.IsValid())
	{
		MaxSpiceChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxSpiceAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleMaxSpiceAttributeChanged);
	}

	if (!MoveSpeedChangedDelegateHandle.IsValid())
	{
		MoveSpeedChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleMoveSpeedAttributeChanged);
	}
}

void AARPlayerStateBase::UnbindTrackedAttributeDelegates()
{
	if (!AbilitySystemComponent)
	{
		HealthChangedDelegateHandle.Reset();
		MaxHealthChangedDelegateHandle.Reset();
		SpiceChangedDelegateHandle.Reset();
		MaxSpiceChangedDelegateHandle.Reset();
		MoveSpeedChangedDelegateHandle.Reset();
		return;
	}

	if (HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}

	if (MaxHealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
		MaxHealthChangedDelegateHandle.Reset();
	}

	if (SpiceChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetSpiceAttribute()).Remove(SpiceChangedDelegateHandle);
		SpiceChangedDelegateHandle.Reset();
	}

	if (MaxSpiceChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxSpiceAttribute()).Remove(MaxSpiceChangedDelegateHandle);
		MaxSpiceChangedDelegateHandle.Reset();
	}

	if (MoveSpeedChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute()).Remove(MoveSpeedChangedDelegateHandle);
		MoveSpeedChangedDelegateHandle.Reset();
	}
}

void AARPlayerStateBase::BroadcastTrackedAttributeSnapshot()
{
	const FARPlayerCoreAttributeSnapshot Snapshot = GetCoreAttributeSnapshot();
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Health, Snapshot.Health, Snapshot.Health);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxHealth, Snapshot.MaxHealth, Snapshot.MaxHealth);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Spice, Snapshot.Spice, Snapshot.Spice);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxSpice, Snapshot.MaxSpice, Snapshot.MaxSpice);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MoveSpeed, Snapshot.MoveSpeed, Snapshot.MoveSpeed);

	OnHealthChanged.Broadcast(this, PlayerSlot, Snapshot.Health, Snapshot.Health);
	OnMaxHealthChanged.Broadcast(this, PlayerSlot, Snapshot.MaxHealth, Snapshot.MaxHealth);
	OnSpiceChanged.Broadcast(this, PlayerSlot, Snapshot.Spice, Snapshot.Spice);
	OnMaxSpiceChanged.Broadcast(this, PlayerSlot, Snapshot.MaxSpice, Snapshot.MaxSpice);
	OnMoveSpeedChanged.Broadcast(this, PlayerSlot, Snapshot.MoveSpeed, Snapshot.MoveSpeed);
}

void AARPlayerStateBase::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Health, ChangeData.NewValue, ChangeData.OldValue);
	OnHealthChanged.Broadcast(this, PlayerSlot, ChangeData.NewValue, ChangeData.OldValue);
}

void AARPlayerStateBase::HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxHealth, ChangeData.NewValue, ChangeData.OldValue);
	OnMaxHealthChanged.Broadcast(this, PlayerSlot, ChangeData.NewValue, ChangeData.OldValue);
}

void AARPlayerStateBase::HandleSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Spice, ChangeData.NewValue, ChangeData.OldValue);
	OnSpiceChanged.Broadcast(this, PlayerSlot, ChangeData.NewValue, ChangeData.OldValue);
}

void AARPlayerStateBase::HandleMaxSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxSpice, ChangeData.NewValue, ChangeData.OldValue);
	OnMaxSpiceChanged.Broadcast(this, PlayerSlot, ChangeData.NewValue, ChangeData.OldValue);
}

void AARPlayerStateBase::HandleMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MoveSpeed, ChangeData.NewValue, ChangeData.OldValue);
	OnMoveSpeedChanged.Broadcast(this, PlayerSlot, ChangeData.NewValue, ChangeData.OldValue);
}

void AARPlayerStateBase::BroadcastCoreAttributeChanged(EARCoreAttributeType AttributeType, float NewValue, float OldValue)
{
	OnCoreAttributeChanged.Broadcast(this, PlayerSlot, AttributeType, NewValue, OldValue);
}

void AARPlayerStateBase::SetSpiceMeter_Internal(float NewSpiceValue)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const float MaxSpice = AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMaxSpiceAttribute());
	const float ClampedValue = FMath::Clamp(NewSpiceValue, 0.f, FMath::Max(0.f, MaxSpice));
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetSpiceAttribute(), ClampedValue);
}
