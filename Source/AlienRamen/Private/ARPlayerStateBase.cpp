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
	DOREPLIFETIME(AARPlayerStateBase, InvaderPlayerColor);
	DOREPLIFETIME(AARPlayerStateBase, InvaderComboCount);
	DOREPLIFETIME(AARPlayerStateBase, ActivatedInvaderUpgradeTags);
	DOREPLIFETIME(AARPlayerStateBase, bIsSharingSpice);
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

	const EARCharacterChoice DefaultCharacter =
		(PlayerSlot == EARPlayerSlot::P2) ? EARCharacterChoice::Sister : EARCharacterChoice::Brother;
	SetCharacterPicked(DefaultCharacter);
	ResetInvaderCombo();
	ClearActivatedInvaderUpgrades();
	SetInvaderPlayerColor_Internal(ResolveDefaultInvaderPlayerColorFromCharacter(DefaultCharacter), true);
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

void AARPlayerStateBase::SetSpiceSharingActive(bool bNewIsSharing)
{
	if (HasAuthority())
	{
		SetSpiceSharingActive_Internal(bNewIsSharing);
		return;
	}

	ServerSetSpiceSharingActive(bNewIsSharing);
}

void AARPlayerStateBase::ServerSetSpiceSharingActive_Implementation(bool bNewIsSharing)
{
	SetSpiceSharingActive_Internal(bNewIsSharing);
}

void AARPlayerStateBase::ApplySpiceShareTick(const float DeltaSeconds, AARPlayerStateBase* TargetPlayer, float& OutSourceDrained, float& OutTargetGranted)
{
	OutSourceDrained = 0.0f;
	OutTargetGranted = 0.0f;

	if (!HasAuthority() || !AbilitySystemComponent || !TargetPlayer || TargetPlayer == this || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Mutual sharing cancels transfer entirely (prevents both players draining to nowhere).
	if (bIsSharingSpice && TargetPlayer->IsSpiceSharingActive())
	{
		return;
	}

	if (!bIsSharingSpice)
	{
		return;
	}

	const float SourceSpice = GetCoreAttributeValue(EARCoreAttributeType::Spice);
	const float TargetSpice = TargetPlayer->GetCoreAttributeValue(EARCoreAttributeType::Spice);
	const float TargetMaxSpice = TargetPlayer->GetCoreAttributeValue(EARCoreAttributeType::MaxSpice);
	const float TargetCapacity = FMath::Max(0.0f, TargetMaxSpice - TargetSpice);
	if (SourceSpice <= KINDA_SMALL_NUMBER || TargetCapacity <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float DrainRate = FMath::Max(0.0f, AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetSpiceDrainRateAttribute()));
	const float ShareRatio = FMath::Max(0.0f, AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetSpiceShareRatioAttribute()));
	if (DrainRate <= KINDA_SMALL_NUMBER || ShareRatio <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float DrainBudget = FMath::Min(SourceSpice, DrainRate * DeltaSeconds);
	if (DrainBudget <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float GrantedBeforeCap = DrainBudget * ShareRatio;
	const float GrantedAmount = FMath::Min(GrantedBeforeCap, TargetCapacity);
	if (GrantedAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float DrainedAmount = FMath::Min(DrainBudget, GrantedAmount / ShareRatio);
	if (DrainedAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	SetSpiceMeter_Internal(SourceSpice - DrainedAmount);
	TargetPlayer->SetSpiceMeter(TargetSpice + GrantedAmount);
	OutSourceDrained = DrainedAmount;
	OutTargetGranted = GrantedAmount;
}

void AARPlayerStateBase::SetInvaderPlayerColor(EARAffinityColor NewColor)
{
	if (HasAuthority())
	{
		SetInvaderPlayerColor_Internal(NewColor);
		return;
	}

	ServerSetInvaderPlayerColor(NewColor);
}

void AARPlayerStateBase::ServerSetInvaderPlayerColor_Implementation(EARAffinityColor NewColor)
{
	SetInvaderPlayerColor_Internal(NewColor);
}

void AARPlayerStateBase::ResetInvaderCombo()
{
	if (!HasAuthority())
	{
		return;
	}

	if (InvaderComboCount == 0)
	{
		LastInvaderKillCreditServerTime = -1.0f;
		return;
	}

	const int32 OldComboCount = InvaderComboCount;
	InvaderComboCount = 0;
	LastInvaderKillCreditServerTime = -1.0f;
	OnRep_InvaderComboCount(OldComboCount);
	ForceNetUpdate();
}

void AARPlayerStateBase::ReportInvaderKillCredit(EARAffinityColor EnemyColor, const float ServerTimeSeconds, const float ComboTimeoutSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bHasPriorCredit = LastInvaderKillCreditServerTime >= 0.0f;
	const bool bTimedOut = bHasPriorCredit
		&& ComboTimeoutSeconds > 0.0f
		&& (ServerTimeSeconds - LastInvaderKillCreditServerTime) > ComboTimeoutSeconds;

	const bool bMatchedColor = DoesInvaderColorMatch(InvaderPlayerColor, EnemyColor);
	const int32 OldComboCount = InvaderComboCount;
	const int32 NewComboCount = bMatchedColor ? ((bTimedOut ? 0 : OldComboCount) + 1) : 0;

	InvaderComboCount = FMath::Max(0, NewComboCount);
	LastInvaderKillCreditServerTime = ServerTimeSeconds;
	if (InvaderComboCount != OldComboCount)
	{
		OnRep_InvaderComboCount(OldComboCount);
		ForceNetUpdate();
	}
}

void AARPlayerStateBase::MarkInvaderUpgradeActivated(FGameplayTag UpgradeTag)
{
	if (!HasAuthority() || !UpgradeTag.IsValid())
	{
		return;
	}

	if (ActivatedInvaderUpgradeTags.HasTagExact(UpgradeTag))
	{
		return;
	}

	const FGameplayTagContainer OldTags = ActivatedInvaderUpgradeTags;
	ActivatedInvaderUpgradeTags.AddTag(UpgradeTag);
	OnRep_ActivatedInvaderUpgrades(OldTags);
	ForceNetUpdate();
}

void AARPlayerStateBase::ClearActivatedInvaderUpgrades()
{
	if (!HasAuthority() || ActivatedInvaderUpgradeTags.IsEmpty())
	{
		return;
	}

	const FGameplayTagContainer OldTags = ActivatedInvaderUpgradeTags;
	ActivatedInvaderUpgradeTags.Reset();
	OnRep_ActivatedInvaderUpgrades(OldTags);
	ForceNetUpdate();
}

bool AARPlayerStateBase::HasActivatedInvaderUpgrade(FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() && ActivatedInvaderUpgradeTags.HasTagExact(UpgradeTag);
}

void AARPlayerStateBase::SetPredictedSpiceValue(const float NewPredictedSpice)
{
	PredictedSpiceValue = FMath::Max(0.0f, NewPredictedSpice);
	bHasPredictedSpiceValue = true;
	OnPredictedSpiceChanged.Broadcast(PredictedSpiceValue, GetCoreAttributeValue(EARCoreAttributeType::Spice), bHasPredictedSpiceValue);
}

void AARPlayerStateBase::ClearPredictedSpiceValue()
{
	if (!bHasPredictedSpiceValue)
	{
		return;
	}

	bHasPredictedSpiceValue = false;
	PredictedSpiceValue = 0.0f;
	OnPredictedSpiceChanged.Broadcast(PredictedSpiceValue, GetCoreAttributeValue(EARCoreAttributeType::Spice), bHasPredictedSpiceValue);
}

void AARPlayerStateBase::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaultLoadoutIfEmpty();
	if (HasAuthority() && InvaderPlayerColor == EARAffinityColor::None)
	{
		SetInvaderPlayerColor_Internal(ResolveDefaultInvaderPlayerColorFromCharacter(CharacterPicked), true);
	}
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

void AARPlayerStateBase::OnRep_InvaderPlayerColor(EARAffinityColor OldColor)
{
	OnInvaderPlayerColorChanged.Broadcast(InvaderPlayerColor, OldColor);
}

void AARPlayerStateBase::OnRep_InvaderComboCount(int32 OldComboCount)
{
	OnInvaderComboChanged.Broadcast(this, PlayerSlot, InvaderComboCount, OldComboCount);
}

void AARPlayerStateBase::OnRep_ActivatedInvaderUpgrades(const FGameplayTagContainer& OldActivatedTags)
{
	OnInvaderActivatedUpgradesChanged.Broadcast(this, PlayerSlot, ActivatedInvaderUpgradeTags, OldActivatedTags);
}

void AARPlayerStateBase::OnRep_IsSharingSpice(bool bOldIsSharingSpice)
{
	OnSpiceSharingStateChanged.Broadcast(this, PlayerSlot, bIsSharingSpice, bOldIsSharingSpice);
}

void AARPlayerStateBase::SetCharacterPicked_Internal(EARCharacterChoice NewCharacter)
{
	if (!HasAuthority() || CharacterPicked == NewCharacter)
	{
		return;
	}

	const EARCharacterChoice OldCharacter = CharacterPicked;
	CharacterPicked = NewCharacter;
	SetInvaderPlayerColor_Internal(ResolveDefaultInvaderPlayerColorFromCharacter(NewCharacter));
	OnRep_CharacterPicked(OldCharacter);
	ForceNetUpdate();
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::SetInvaderPlayerColor_Internal(EARAffinityColor NewColor, const bool bForceBroadcast)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewColor == EARAffinityColor::Unknown)
	{
		NewColor = EARAffinityColor::None;
	}

	if (!bUpdatingInvaderColorFromTags)
	{
		ApplyInvaderColorGameplayTags(NewColor);
	}

	if (!bForceBroadcast && InvaderPlayerColor == NewColor)
	{
		return;
	}

	const EARAffinityColor OldColor = InvaderPlayerColor;
	InvaderPlayerColor = NewColor;
	OnRep_InvaderPlayerColor(OldColor);
	ForceNetUpdate();

	if (NewColor == EARAffinityColor::None || NewColor == EARAffinityColor::White)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[InvaderSpice|Color] Player '%s' entered non-baseline color %d (Old=%d Slot=%d Character=%d)."),
			*GetNameSafe(this),
			static_cast<int32>(NewColor),
			static_cast<int32>(OldColor),
			static_cast<int32>(PlayerSlot),
			static_cast<int32>(CharacterPicked));
	}
}

void AARPlayerStateBase::SetSpiceSharingActive_Internal(const bool bNewIsSharing, const bool bForceBroadcast)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bForceBroadcast && bIsSharingSpice == bNewIsSharing)
	{
		return;
	}

	const bool bOldIsSharing = bIsSharingSpice;
	bIsSharingSpice = bNewIsSharing;
	OnRep_IsSharingSpice(bOldIsSharing);
	ForceNetUpdate();
}

EARAffinityColor AARPlayerStateBase::ResolveDefaultInvaderPlayerColorFromCharacter(const EARCharacterChoice InCharacterChoice) const
{
	switch (InCharacterChoice)
	{
	case EARCharacterChoice::Brother:
		return EARAffinityColor::Blue;
	case EARCharacterChoice::Sister:
		return EARAffinityColor::Red;
	default:
		// Keep a deterministic non-white baseline even when character is not yet assigned.
		return (PlayerSlot == EARPlayerSlot::P2) ? EARAffinityColor::Red : EARAffinityColor::Blue;
	}
}

bool AARPlayerStateBase::DoesInvaderColorMatch(const EARAffinityColor PlayerColor, const EARAffinityColor EnemyColor)
{
	if (PlayerColor == EARAffinityColor::None || EnemyColor == EARAffinityColor::None)
	{
		return false;
	}

	if (PlayerColor == EARAffinityColor::White || EnemyColor == EARAffinityColor::White)
	{
		return true;
	}

	return PlayerColor == EnemyColor;
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
			// Never leave character unset; fall back to slot-biased default even if non-unique.
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[PlayerState] Ready prerequisites found both character choices already taken for '%s'; assigning slot-biased fallback %d."),
				*GetNameSafe(this),
				static_cast<int32>(PreferredChoice));
			SetCharacterPicked_Internal(PreferredChoice);
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
	TargetPS->ResetInvaderCombo();
	TargetPS->ClearActivatedInvaderUpgrades();
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

	if (!ColorNoneTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
		if (ColorNoneTag.IsValid())
		{
			ColorNoneTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorNoneTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!ColorRedTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
		if (ColorRedTag.IsValid())
		{
			ColorRedTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorRedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!ColorWhiteTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
		if (ColorWhiteTag.IsValid())
		{
			ColorWhiteTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorWhiteTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!ColorBlueTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);
		if (ColorBlueTag.IsValid())
		{
			ColorBlueTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(ColorBlueTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!SharingSpiceTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag SharingSpiceTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sharing"), false);
		if (SharingSpiceTag.IsValid())
		{
			SharingSpiceTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(SharingSpiceTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleSpiceSharingTagChanged);
			SetSpiceSharingActive_Internal(AbilitySystemComponent->HasMatchingGameplayTag(SharingSpiceTag), true);
		}
	}

	EvaluateInvaderColorFromASCOverrideTags();
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
		ColorNoneTagChangedDelegateHandle.Reset();
		ColorRedTagChangedDelegateHandle.Reset();
		ColorWhiteTagChangedDelegateHandle.Reset();
		ColorBlueTagChangedDelegateHandle.Reset();
		SharingSpiceTagChangedDelegateHandle.Reset();
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

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	if (ColorNoneTag.IsValid() && ColorNoneTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorNoneTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorNoneTagChangedDelegateHandle);
		ColorNoneTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	if (ColorRedTag.IsValid() && ColorRedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorRedTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorRedTagChangedDelegateHandle);
		ColorRedTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	if (ColorWhiteTag.IsValid() && ColorWhiteTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorWhiteTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorWhiteTagChangedDelegateHandle);
		ColorWhiteTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);
	if (ColorBlueTag.IsValid() && ColorBlueTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(ColorBlueTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorBlueTagChangedDelegateHandle);
		ColorBlueTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag SharingSpiceTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sharing"), false);
	if (SharingSpiceTag.IsValid() && SharingSpiceTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(SharingSpiceTag, EGameplayTagEventType::NewOrRemoved).Remove(SharingSpiceTagChangedDelegateHandle);
		SharingSpiceTagChangedDelegateHandle.Reset();
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
	if (bHasPredictedSpiceValue)
	{
		OnPredictedSpiceChanged.Broadcast(PredictedSpiceValue, Snapshot.Spice, true);
	}
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
	if (bHasPredictedSpiceValue)
	{
		OnPredictedSpiceChanged.Broadcast(PredictedSpiceValue, ChangeData.NewValue, true);
	}
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

void AARPlayerStateBase::HandleInvaderColorOverrideTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	EvaluateInvaderColorFromASCOverrideTags();
}

void AARPlayerStateBase::HandleSpiceSharingTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag SharingSpiceTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sharing"), false);
	const bool bSharingNow = SharingSpiceTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(SharingSpiceTag);
	SetSpiceSharingActive_Internal(bSharingNow);
}

void AARPlayerStateBase::EvaluateInvaderColorFromASCOverrideTags()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const EARAffinityColor ResolvedColor = ResolveInvaderColorFromASCOverrideTags();
	bUpdatingInvaderColorFromTags = true;
	SetInvaderPlayerColor_Internal(ResolvedColor);
	bUpdatingInvaderColorFromTags = false;
}

EARAffinityColor AARPlayerStateBase::ResolveInvaderColorFromASCOverrideTags() const
{
	if (!AbilitySystemComponent)
	{
		return ResolveDefaultInvaderPlayerColorFromCharacter(CharacterPicked);
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);

	// Override precedence: None > White > Red > Blue.
	if (ColorNoneTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorNoneTag))
	{
		return EARAffinityColor::None;
	}

	if (ColorWhiteTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorWhiteTag))
	{
		return EARAffinityColor::White;
	}

	if (ColorRedTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorRedTag))
	{
		return EARAffinityColor::Red;
	}

	if (ColorBlueTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(ColorBlueTag))
	{
		return EARAffinityColor::Blue;
	}

	return ResolveDefaultInvaderPlayerColorFromCharacter(CharacterPicked);
}

void AARPlayerStateBase::ApplyInvaderColorGameplayTags(const EARAffinityColor NewColor)
{
	if (!HasAuthority() || !AbilitySystemComponent || bApplyingInvaderColorTags)
	{
		return;
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);

	FGameplayTagContainer AllColorTags;
	if (ColorNoneTag.IsValid()) { AllColorTags.AddTag(ColorNoneTag); }
	if (ColorWhiteTag.IsValid()) { AllColorTags.AddTag(ColorWhiteTag); }
	if (ColorRedTag.IsValid()) { AllColorTags.AddTag(ColorRedTag); }
	if (ColorBlueTag.IsValid()) { AllColorTags.AddTag(ColorBlueTag); }

	if (!AllColorTags.IsEmpty())
	{
		bApplyingInvaderColorTags = true;
		AbilitySystemComponent->RemoveLooseGameplayTags(AllColorTags, 1, EGameplayTagReplicationState::TagOnly);

		FGameplayTagContainer ActiveColorTag;
		switch (NewColor)
		{
		case EARAffinityColor::None:
			if (ColorNoneTag.IsValid()) { ActiveColorTag.AddTag(ColorNoneTag); }
			break;
		case EARAffinityColor::White:
			if (ColorWhiteTag.IsValid()) { ActiveColorTag.AddTag(ColorWhiteTag); }
			break;
		case EARAffinityColor::Red:
			if (ColorRedTag.IsValid()) { ActiveColorTag.AddTag(ColorRedTag); }
			break;
		case EARAffinityColor::Blue:
			if (ColorBlueTag.IsValid()) { ActiveColorTag.AddTag(ColorBlueTag); }
			break;
		default:
			break;
		}

		if (!ActiveColorTag.IsEmpty())
		{
			AbilitySystemComponent->AddLooseGameplayTags(ActiveColorTag, 1, EGameplayTagReplicationState::TagOnly);
		}
		bApplyingInvaderColorTags = false;
	}
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
