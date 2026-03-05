#include "ARPlayerStateBase.h"

#include "ARAttributeSetCore.h"
#include "ARLoadoutSettings.h"
#include "ARLog.h"
#include "AbilitySystemComponent.h"
#include "ARSaveSubsystem.h"
#include "GameplayTagUtilities.h"
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
	DOREPLIFETIME(AARPlayerStateBase, bIsDowned);
	DOREPLIFETIME(AARPlayerStateBase, bIsDeadState);
	DOREPLIFETIME(AARPlayerStateBase, bIsSetup);
}

void AARPlayerStateBase::OnRep_Loadout(const FGameplayTagContainer& OldLoadoutTags)
{
	OnLoadoutTagsChanged.Broadcast(this, PlayerSlot, LoadoutTags, OldLoadoutTags);
}

AARPlayerStateBase::AARPlayerStateBase()
{
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
	DisplayName = GetPlayerName();
	bCachedTravelReady = false;
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
	EvaluateTravelReadinessAndBroadcast();
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

void AARPlayerStateBase::SetIsSetupComplete(bool bNewIsSetup)
{
	if (!HasAuthority() || bIsSetup == bNewIsSetup)
	{
		return;
	}

	const bool bOldIsSetup = bIsSetup;
	bIsSetup = bNewIsSetup;
	OnRep_IsSetup(bOldIsSetup);
	ForceNetUpdate();
}

void AARPlayerStateBase::InitializeForFirstSessionJoin()
{
	if (!HasAuthority())
	{
		return;
	}

	SetCharacterPicked(EARCharacterChoice::None);
	EnsureDefaultLoadoutIfEmpty();
}

void AARPlayerStateBase::SetLoadoutTags(const FGameplayTagContainer& NewLoadoutTags)
{
	if (HasAuthority())
	{
		SetLoadoutTags_Internal(NewLoadoutTags);
		return;
	}

	// Keep loadout authoritative on server; callers should route through server-owned setup/systems.
	UE_LOG(ARLog, Warning, TEXT("[PlayerState] SetLoadoutTags ignored on non-authority for '%s'."), *GetNameSafe(this));
}

void AARPlayerStateBase::SetDownedState(bool bNewDowned)
{
	if (HasAuthority())
	{
		SetDowned_Internal(bNewDowned);
		return;
	}

	ServerUpdateDownedState(bNewDowned);
}

void AARPlayerStateBase::ServerUpdateDownedState_Implementation(bool bNewDowned)
{
	SetDowned_Internal(bNewDowned);
}

void AARPlayerStateBase::SetDeadState(bool bNewDead)
{
	if (HasAuthority())
	{
		SetDead_Internal(bNewDead);
		return;
	}

	ServerUpdateDeadState(bNewDead);
}

void AARPlayerStateBase::ServerUpdateDeadState_Implementation(bool bNewDead)
{
	SetDead_Internal(bNewDead);
}

void AARPlayerStateBase::UpdateLoadoutWithTag(FGameplayTag NewTag)
{
	if (HasAuthority())
	{
		UpdateLoadoutWithTag_Internal(NewTag);
		return;
	}

	ServerUpdateLoadoutWithTag(NewTag);
}

void AARPlayerStateBase::ServerUpdateLoadoutWithTag_Implementation(FGameplayTag NewTag)
{
	UpdateLoadoutWithTag_Internal(NewTag);
}

void AARPlayerStateBase::RemoveTagFromLoadout(FGameplayTag TagToRemove)
{
	if (HasAuthority())
	{
		RemoveTagFromLoadout_Internal(TagToRemove);
		return;
	}

	ServerRemoveTagFromLoadout(TagToRemove);
}

void AARPlayerStateBase::ServerRemoveTagFromLoadout_Implementation(FGameplayTag TagToRemove)
{
	RemoveTagFromLoadout_Internal(TagToRemove);
}

TArray<FGameplayTag> AARPlayerStateBase::GetTagsInLoadoutSlot(FGameplayTag SlotTag) const
{
	TArray<FGameplayTag> Result;
	if (!SlotTag.IsValid())
	{
		return Result;
	}

	TArray<FGameplayTag> ExistingTags;
	LoadoutTags.GetGameplayTagArray(ExistingTags);
	for (const FGameplayTag& ExistingTag : ExistingTags)
	{
		if (!ExistingTag.IsValid())
		{
			continue;
		}

		// Include tags in the slot subtree, excluding the slot root itself.
		if (ExistingTag != SlotTag && ExistingTag.MatchesTag(SlotTag))
		{
			Result.Add(ExistingTag);
		}
	}

	return Result;
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
	EvaluateLifeStateFromASC();
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindTrackedAttributeDelegates();
	Super::EndPlay(EndPlayReason);
}

void AARPlayerStateBase::OnRep_PlayerSlot(EARPlayerSlot OldSlot)
{
	OnPlayerSlotChanged.Broadcast(PlayerSlot, OldSlot);
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::OnRep_CharacterPicked(EARCharacterChoice OldCharacter)
{
	OnCharacterPickedChanged.Broadcast(this, PlayerSlot, CharacterPicked, OldCharacter);
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::OnRep_DisplayName(const FString& OldDisplayName)
{
	OnDisplayNameChanged.Broadcast(this, PlayerSlot, DisplayName, OldDisplayName);
}

void AARPlayerStateBase::OnRep_IsReady(bool bOldReady)
{
	OnReadyStatusChanged.Broadcast(this, PlayerSlot, bIsReady, bOldReady);
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::OnRep_IsDowned(bool bOldDowned)
{
	OnDownedStateChanged.Broadcast(this, PlayerSlot, bIsDowned, bOldDowned);
}

void AARPlayerStateBase::OnRep_IsDeadState(bool bOldDeadState)
{
	OnDeadStateChanged.Broadcast(this, PlayerSlot, bIsDeadState, bOldDeadState);
}

void AARPlayerStateBase::OnRep_IsSetup(bool bOldIsSetup)
{
	OnSetupStateChanged.Broadcast(bIsSetup, bOldIsSetup);
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
	EvaluateTravelReadinessAndBroadcast();
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
	if (!HasAuthority())
	{
		return;
	}

	if (bNewReady)
	{
		if (!EnsureReadyPrerequisitesForRun())
		{
			UE_LOG(ARLog, Warning, TEXT("[PlayerState] SetReadyForRun blocked: prerequisites unresolved for '%s'."), *GetNameSafe(this));
			return;
		}
	}

	if (bIsReady == bNewReady)
	{
		return;
	}

	const bool bOldReady = bIsReady;
	bIsReady = bNewReady;
	OnRep_IsReady(bOldReady);
	ForceNetUpdate();
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::SetDowned_Internal(bool bNewDowned)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bResolvedDowned = bIsDeadState ? false : bNewDowned;
	if (bIsDowned == bResolvedDowned)
	{
		return;
	}

	const bool bOldDowned = bIsDowned;
	bIsDowned = bResolvedDowned;
	OnRep_IsDowned(bOldDowned);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetDead_Internal(bool bNewDead)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDeadState == bNewDead)
	{
		return;
	}

	const bool bOldDead = bIsDeadState;
	bIsDeadState = bNewDead;
	OnRep_IsDeadState(bOldDead);
	ForceNetUpdate();

	if (bIsDeadState && bIsDowned)
	{
		SetDowned_Internal(false);
	}
}

bool AARPlayerStateBase::EnsureReadyPrerequisitesForRun()
{
	if (!HasAuthority())
	{
		return false;
	}

	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return false;
	}

	if (PlayerSlot == EARPlayerSlot::Unknown)
	{
		bool bP1Taken = false;
		bool bP2Taken = false;
		for (APlayerState* PS : GS->PlayerArray)
		{
			const AARPlayerStateBase* OtherPlayer = Cast<AARPlayerStateBase>(PS);
			if (!OtherPlayer || OtherPlayer == this)
			{
				continue;
			}

			if (OtherPlayer->GetPlayerSlot() == EARPlayerSlot::P1)
			{
				bP1Taken = true;
			}
			else if (OtherPlayer->GetPlayerSlot() == EARPlayerSlot::P2)
			{
				bP2Taken = true;
			}
		}

		if (bP1Taken && bP2Taken)
		{
			UE_LOG(ARLog, Warning, TEXT("[PlayerState] Could not auto-assign player slot for '%s': P1 and P2 already taken."), *GetNameSafe(this));
			return false;
		}

		const EARPlayerSlot ResolvedSlot = !bP1Taken ? EARPlayerSlot::P1 : EARPlayerSlot::P2;
		SetPlayerSlot(ResolvedSlot);
	}

	if (CharacterPicked == EARCharacterChoice::None)
	{
		const auto IsCharacterTakenByOther = [this, GS](EARCharacterChoice Choice) -> bool
		{
			if (Choice == EARCharacterChoice::None)
			{
				return false;
			}

			for (APlayerState* PS : GS->PlayerArray)
			{
				const AARPlayerStateBase* OtherPlayer = Cast<AARPlayerStateBase>(PS);
				if (!OtherPlayer || OtherPlayer == this)
				{
					continue;
				}

				if (OtherPlayer->GetCharacterPicked() == Choice)
				{
					return true;
				}
			}

			return false;
		};

		const EARCharacterChoice PreferredChoice =
			(PlayerSlot == EARPlayerSlot::P2) ? EARCharacterChoice::Sister : EARCharacterChoice::Brother;
		const EARCharacterChoice AlternateChoice =
			(PreferredChoice == EARCharacterChoice::Brother) ? EARCharacterChoice::Sister : EARCharacterChoice::Brother;

		if (!IsCharacterTakenByOther(PreferredChoice))
		{
			SetCharacterPicked_Internal(PreferredChoice);
		}
		else if (!IsCharacterTakenByOther(AlternateChoice))
		{
			SetCharacterPicked_Internal(AlternateChoice);
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[PlayerState] Ready prerequisites could not auto-assign character for '%s'."), *GetNameSafe(this));
			return false;
		}
	}

	return true;
}

void AARPlayerStateBase::SetLoadoutTags_Internal(const FGameplayTagContainer& NewLoadoutTags)
{
	if (!HasAuthority())
	{
		return;
	}

	FGameplayTagContainer NormalizedTags = NewLoadoutTags;
	NormalizeLoadoutTagsForSlotRules(NormalizedTags);

	if (LoadoutTags == NormalizedTags)
	{
		return;
	}

	const FGameplayTagContainer OldLoadoutTags = LoadoutTags;
	LoadoutTags = NormalizedTags;
	OnRep_Loadout(OldLoadoutTags);
	ForceNetUpdate();

	// Mark save dirty so autosave can persist new loadout.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			SaveSubsystem->MarkSaveDirty();
		}
	}
}

void AARPlayerStateBase::UpdateLoadoutWithTag_Internal(FGameplayTag NewTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!NewTag.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[PlayerState] UpdateLoadoutWithTag ignored invalid tag for '%s'."), *GetNameSafe(this));
		return;
	}

	FGameplayTag SlotRootTag;
	const bool bHasSlotRoot = UGameplayTagUtilities::TryGetTagAtDepth(NewTag, 2, SlotRootTag);

	FGameplayTagContainer NewLoadout = LoadoutTags;
	if (bHasSlotRoot && IsSingleSlotLoadoutRootTag(SlotRootTag))
	{
		if (!UGameplayTagUtilities::ReplaceTagInSlot(NewLoadout, NewTag))
		{
			UE_LOG(ARLog, Warning, TEXT("[PlayerState] UpdateLoadoutWithTag failed to replace single-slot tag '%s' for '%s'."), *NewTag.ToString(), *GetNameSafe(this));
			return;
		}
	}
	else
	{
		NewLoadout.AddTag(NewTag);
	}

	SetLoadoutTags_Internal(NewLoadout);
}

void AARPlayerStateBase::RemoveTagFromLoadout_Internal(FGameplayTag TagToRemove)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!TagToRemove.IsValid())
	{
		UE_LOG(ARLog, Warning, TEXT("[PlayerState] RemoveTagFromLoadout ignored invalid tag for '%s'."), *GetNameSafe(this));
		return;
	}

	if (!LoadoutTags.HasTagExact(TagToRemove))
	{
		return;
	}

	FGameplayTagContainer NewLoadout = LoadoutTags;
	NewLoadout.RemoveTag(TagToRemove);
	SetLoadoutTags_Internal(NewLoadout);
}

void AARPlayerStateBase::NormalizeLoadoutTagsForSlotRules(FGameplayTagContainer& InOutTags) const
{
	TArray<FGameplayTag> ExistingTags;
	InOutTags.GetGameplayTagArray(ExistingTags);

	TMap<FGameplayTag, FGameplayTag> LastTagBySingleSlotRoot;
	for (const FGameplayTag& ExistingTag : ExistingTags)
	{
		FGameplayTag SlotRootTag;
		if (!UGameplayTagUtilities::TryGetTagAtDepth(ExistingTag, 2, SlotRootTag))
		{
			continue;
		}

		if (!IsSingleSlotLoadoutRootTag(SlotRootTag))
		{
			continue;
		}

		LastTagBySingleSlotRoot.FindOrAdd(SlotRootTag) = ExistingTag;
	}

	for (const TPair<FGameplayTag, FGameplayTag>& Entry : LastTagBySingleSlotRoot)
	{
		UGameplayTagUtilities::ReplaceTagInSlot(InOutTags, Entry.Value);
	}
}

bool AARPlayerStateBase::IsSingleSlotLoadoutRootTag(FGameplayTag RootTag) const
{
	if (!RootTag.IsValid())
	{
		return false;
	}

	const UARLoadoutSettings* Settings = GetDefault<UARLoadoutSettings>();
	if (!Settings)
	{
		// Fail-safe to preserve deterministic behavior if settings are unavailable.
		return true;
	}

	// Single-slot by default; explicitly configured roots are multi-slot.
	return !Settings->MultiSlotLoadoutRoots.HasTagExact(RootTag);
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
	TargetPS->SetLoadoutTags(LoadoutTags);
	TargetPS->SetCharacterPicked(CharacterPicked);
	TargetPS->SetDisplayNameValue(DisplayName);

	// Mark copied/traveled state as setup-complete; ready remains transient.
	TargetPS->SetIsSetupComplete(true);
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
		UE_LOG(ARLog, Warning, TEXT("[PlayerState] ApplyStateFromStruct rejected on non-authority '%s'."), *GetNameSafe(this));
		return false;
	}

	const bool bApplied = IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
	if (bApplied)
	{
		EnsureDefaultLoadoutIfEmpty();
	}
	return bApplied;
}

void AARPlayerStateBase::EnsureDefaultLoadoutIfEmpty()
{
	if (!HasAuthority() || !LoadoutTags.IsEmpty())
	{
		return;
	}

	const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
	const FGameplayTagContainer NewTags = LoadoutSettings ? LoadoutSettings->DefaultPlayerLoadoutTags : FGameplayTagContainer();

	if (!NewTags.IsEmpty())
	{
		SetLoadoutTags_Internal(NewTags);
		UE_LOG(ARLog, Log, TEXT("[ShipGAS] Applied default loadout tags: %s"), *NewTags.ToStringSimple());
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[ShipGAS] Default loadout is empty in project settings (Alien Ramen Loadout -> Default Player Loadout Tags)."));
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

	if (!DownedTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false);
		if (DownedTag.IsValid())
		{
			DownedTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(DownedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleDownedTagChanged);
		}
	}

	if (!DeadTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
		if (DeadTag.IsValid())
		{
			DeadTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleDeadTagChanged);
		}
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
		DownedTagChangedDelegateHandle.Reset();
		DeadTagChangedDelegateHandle.Reset();
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

	const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false);
	if (DownedTag.IsValid() && DownedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(DownedTag, EGameplayTagEventType::NewOrRemoved).Remove(DownedTagChangedDelegateHandle);
		DownedTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
	if (DeadTag.IsValid() && DeadTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagChangedDelegateHandle);
		DeadTagChangedDelegateHandle.Reset();
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
	EvaluateLifeStateFromASC();
}

void AARPlayerStateBase::HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxHealth, ChangeData.NewValue, ChangeData.OldValue);
	OnMaxHealthChanged.Broadcast(this, PlayerSlot, ChangeData.NewValue, ChangeData.OldValue);
	EvaluateLifeStateFromASC();
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

void AARPlayerStateBase::HandleDownedTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	EvaluateLifeStateFromASC();
}

void AARPlayerStateBase::HandleDeadTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	EvaluateLifeStateFromASC();
}

void AARPlayerStateBase::EvaluateLifeStateFromASC()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
	const bool bDeadFromTag = DeadTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DeadTag);
	const bool bDownedFromTag = DownedTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DownedTag);

	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMaxHealthAttribute());
	const float Health = AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	const bool bDownedFromHealth = (MaxHealth > 0.f && Health <= 0.f);

	SetDead_Internal(bDeadFromTag);
	SetDowned_Internal(!bDeadFromTag && (bDownedFromTag || bDownedFromHealth));
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

bool AARPlayerStateBase::IsTravelReady() const
{
	return PlayerSlot != EARPlayerSlot::Unknown && CharacterPicked != EARCharacterChoice::None && bIsReady;
}

void AARPlayerStateBase::EvaluateTravelReadinessAndBroadcast()
{
	const bool bNowReady = IsTravelReady();
	if (bCachedTravelReady != bNowReady)
	{
		bCachedTravelReady = bNowReady;
		OnTravelReadinessChanged.Broadcast(bCachedTravelReady);
	}
}
