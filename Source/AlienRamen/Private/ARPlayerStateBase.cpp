#include "ARPlayerStateBase.h"

#include "ARAttributeSetCore.h"
#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "ARInvaderGameState.h"
#include "ARInvaderSpicyTrackSettings.h"
#include "ARLoadoutSettings.h"
#include "ARLog.h"
#include "ARSaveGame.h"
#include "AbilitySystemComponent.h"
#include "ARSaveSubsystem.h"
#include "GameplayTagUtilities.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "StructSerializable.h"

namespace
{
	static FARPlayerIdentity BuildPlayerIdentityForSave(const AARPlayerStateBase* PlayerState)
	{
		FARPlayerIdentity Identity;
		if (!PlayerState)
		{
			return Identity;
		}

		UGameInstance* GameInstance = PlayerState->GetGameInstance();
		UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
		if (SaveSubsystem)
		{
			Identity = SaveSubsystem->BuildRuntimePlayerIdentity(PlayerState);
		}
		else
		{
			Identity.DisplayName = FText::FromString(PlayerState->GetDisplayNameValue());
			if (PlayerState->GetUniqueId().IsValid())
			{
				Identity.UniqueNetIdString = PlayerState->GetUniqueId()->ToString();
				Identity.UniqueNetIdType = PlayerState->GetUniqueId()->GetType().ToString();
			}
		}

		return Identity;
	}

	static FARPlayerStateSaveData* FindOrAddCurrentSavePlayerData(AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return nullptr;
		}

		UGameInstance* GameInstance = PlayerState->GetGameInstance();
		UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
		UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
		if (!SaveGame)
		{
			return nullptr;
		}

		const FARPlayerIdentity Identity = BuildPlayerIdentityForSave(PlayerState);
		FARPlayerStateSaveData ExistingData;
		int32 ExistingIndex = INDEX_NONE;
		if (SaveGame->FindPlayerStateDataByIdentity(Identity, ExistingData, ExistingIndex) && SaveGame->PlayerStates.IsValidIndex(ExistingIndex))
		{
			return &SaveGame->PlayerStates[ExistingIndex];
		}

		FARPlayerStateSaveData& Added = SaveGame->PlayerStates.AddDefaulted_GetRef();
		Added.Identity = Identity;
		return &Added;
	}

	static UARSaveGame* GetCurrentSaveGame(const AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return nullptr;
		}

		UGameInstance* GameInstance = PlayerState->GetGameInstance();
		UARSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UARSaveSubsystem>() : nullptr;
		return SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	}

	static FGameplayTag ResolveSignalCharacterTag(const AARPlayerStateBase* PlayerState)
	{
		if (!PlayerState)
		{
			return FGameplayTag();
		}

		return ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
	}

	static FGameplayTag ResolveAlternateCanonicalCharacterTag(const FGameplayTag CharacterTag)
	{
		const EARCharacterChoice Choice = ARPlayer::GetCharacterChoiceForTag(CharacterTag);
		switch (Choice)
		{
		case EARCharacterChoice::Brother:
			return ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Sister);
		case EARCharacterChoice::Sister:
			return ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Brother);
		default:
			return FGameplayTag();
		}
	}
}

void AARPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARPlayerStateBase, PlayerSlotId);
	DOREPLIFETIME(AARPlayerStateBase, CharacterPicked);
	DOREPLIFETIME(AARPlayerStateBase, CurrentCharacterTag);
	DOREPLIFETIME(AARPlayerStateBase, CurrentCharacterRuntime);
	DOREPLIFETIME(AARPlayerStateBase, DisplayName);
	DOREPLIFETIME(AARPlayerStateBase, bIsReady);
	DOREPLIFETIME(AARPlayerStateBase, bIsSetup);
	DOREPLIFETIME(AARPlayerStateBase, bDialogueAutoAdvanceEnabled);
}

void AARPlayerStateBase::OnRep_Loadout(const FGameplayTagContainer& OldLoadoutTags)
{
	OnLoadoutTagsChanged.Broadcast(this, ResolveSignalCharacterTag(this), GetCurrentCharacterLoadoutTags(), OldLoadoutTags);
}

AARPlayerStateBase::AARPlayerStateBase()
{
	bReplicates = true;
	DisplayName = GetPlayerName();
	bCachedTravelReady = false;
}

UAbilitySystemComponent* AARPlayerStateBase::GetAbilitySystemComponent() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetAbilitySystemComponent() : nullptr;
}

UAbilitySystemComponent* AARPlayerStateBase::GetASC() const
{
	return GetAbilitySystemComponent();
}

APawn* AARPlayerStateBase::GetCurrentCharacterPawn() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetCurrentPawn() : nullptr;
}

FGameplayTagContainer AARPlayerStateBase::GetCurrentCharacterLoadoutTags() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetLoadoutTags() : FGameplayTagContainer();
}

void AARPlayerStateBase::SetCurrentCharacterRuntime(AARCharacterStateRuntime* NewRuntime)
{
	if (!HasAuthority() || CurrentCharacterRuntime == NewRuntime)
	{
		return;
	}

	if (NewRuntime)
	{
		NewRuntime->SetOwningPlayerState(this);
		if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
		{
			CharacterSubsystem->RegisterRuntime(NewRuntime);
			CharacterSubsystem->BindRuntimePawn(NewRuntime, GetPawn());
		}
	}

	CurrentCharacterRuntime = NewRuntime;
	OnRep_CurrentCharacterRuntime();

	// Invader spicy meter is clamped by MaxSpice on the character runtime ASC.
	// Runtime rebinds can occur after InvaderGameState performed its global sync pass,
	// so push the current shared cap immediately when this player gets a runtime.
	if (CurrentCharacterRuntime)
	{
		if (const AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
		{
			if (UAbilitySystemComponent* ActiveASC = GetASC())
			{
				ActiveASC->SetNumericAttributeBase(
					UARAttributeSetCore::GetMaxSpiceAttribute(),
					static_cast<float>(InvaderGameState->GetSharedMaxSpice()));
				SetSpiceMeter_Internal(GetCoreAttributeValue(EARCoreAttributeType::Spice));
			}
		}
	}

	ForceNetUpdate();
}

float AARPlayerStateBase::GetCoreAttributeValue(EARCoreAttributeType AttributeType) const
{
	const UAbilitySystemComponent* ActiveASC = GetASC();
	if (!ActiveASC)
	{
		return 0.f;
	}

	switch (AttributeType)
	{
	case EARCoreAttributeType::Health:
		return ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	case EARCoreAttributeType::MaxHealth:
		return ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetMaxHealthAttribute());
	case EARCoreAttributeType::Spice:
		return ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceAttribute());
	case EARCoreAttributeType::MaxSpice:
		return ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetMaxSpiceAttribute());
	case EARCoreAttributeType::MoveSpeed:
		return ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetMoveSpeedAttribute());
	case EARCoreAttributeType::Strength:
		return ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetStrengthAttribute());
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
	Snapshot.Strength = GetCoreAttributeValue(EARCoreAttributeType::Strength);
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

void AARPlayerStateBase::SetPlayerSlotId(int32 NewSlotId)
{
	SetPlayerSlotId_Internal(NewSlotId);
}

void AARPlayerStateBase::SetPlayerSlotId_Internal(const int32 NewSlotId)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 SanitizedSlotId = FMath::Max(0, NewSlotId);
	if (PlayerSlotId == SanitizedSlotId)
	{
		return;
	}

	const int32 OldSlotId = PlayerSlotId;
	PlayerSlotId = SanitizedSlotId;
	OnRep_PlayerSlotId(OldSlotId);
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

void AARPlayerStateBase::SetCurrentCharacterTag(FGameplayTag NewCharacterTag)
{
	if (HasAuthority())
	{
		SetCurrentCharacterTagWithSwap_Internal(NewCharacterTag);
		return;
	}

	ServerSetCurrentCharacterTag(NewCharacterTag);
}

void AARPlayerStateBase::SetCurrentCharacterTagDirect(FGameplayTag NewCharacterTag)
{
	if (!HasAuthority())
	{
		return;
	}

	SetCurrentCharacterTag_Internal(NewCharacterTag);
}

FGameplayTag AARPlayerStateBase::GetPlayerSlotTag() const
{
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CurrentCharacterTag);
	const EARPlayerSlot CharacterSlot = ARPlayer::GetPlayerSlotForCharacterTag(NormalizedCharacterTag);
	return ARPlayer::GetPlayerSlotTag(CharacterSlot);
}

void AARPlayerStateBase::ServerSetCurrentCharacterTag_Implementation(FGameplayTag NewCharacterTag)
{
	SetCurrentCharacterTagWithSwap_Internal(NewCharacterTag);
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

void AARPlayerStateBase::ApplyPlayerSaveData(const FARPlayerStateSaveData& PlayerData)
{
	if (!HasAuthority())
	{
		return;
	}

	SetDisplayName_Internal(PlayerData.Identity.DisplayName.ToString());
	SetDialogueAutoAdvanceEnabled_Internal(PlayerData.bDialogueAutoAdvanceEnabled);
	SetCurrentCharacterTag_Internal(PlayerData.ResolveCurrentCharacterTag(), false);

	FGameplayTagContainer ProjectedLoadout;
	if (TryResolveCharacterOwnedLoadout(CurrentCharacterTag, ProjectedLoadout))
	{
		SetLoadoutTags_Internal(ProjectedLoadout, false);
	}
	else
	{
		SetLoadoutTags_Internal(FGameplayTagContainer(), false);
	}

	// Hydration may legitimately resolve an empty character-owned loadout (missing/legacy rows).
	// Keep editor raw-map startup and runtime join behavior deterministic by seeding defaults.
	EnsureDefaultLoadoutIfEmpty();

	if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		bool bCreatedRuntime = false;
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(this, CurrentCharacterTag, bCreatedRuntime))
		{
			if (UARSaveGame* SaveGame = GetCurrentSaveGame(this))
			{
				FARCharacterSaveData CharacterState;
				int32 CharacterStateIndex = INDEX_NONE;
				if (SaveGame->FindCharacterStateDataByTag(CurrentCharacterTag, CharacterState, CharacterStateIndex))
				{
					Runtime->ApplySaveData(CharacterState);
				}
			}

			SetCurrentCharacterRuntime(Runtime);
			Runtime->SetCurrentPawn(GetPawn());
		}
	}
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
		(ARPlayer::GetCharacterChoiceForTag(CurrentCharacterTag) == EARCharacterChoice::Sister) ? EARCharacterChoice::Sister : EARCharacterChoice::Brother;
	SetCurrentCharacterTag_Internal(ARPlayer::GetCharacterTagForChoice(DefaultCharacter), false);
	ResetInvaderCombo();
	ClearActivatedInvaderUpgrades();
	ResetSpicyTrackCursor();
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

void AARPlayerStateBase::SetDialogueAutoAdvanceEnabled(bool bEnabled)
{
	if (HasAuthority())
	{
		SetDialogueAutoAdvanceEnabled_Internal(bEnabled);
		return;
	}

	ServerSetDialogueAutoAdvanceEnabled(bEnabled);
}

void AARPlayerStateBase::ServerSetDialogueAutoAdvanceEnabled_Implementation(bool bEnabled)
{
	SetDialogueAutoAdvanceEnabled_Internal(bEnabled);
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
	GetCurrentCharacterLoadoutTags().GetGameplayTagArray(ExistingTags);
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

float AARPlayerStateBase::GetStrength() const
{
	return GetCoreAttributeValue(EARCoreAttributeType::Strength);
}

bool AARPlayerStateBase::IsDowned() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->IsDowned() : false;
}

bool AARPlayerStateBase::IsDeadState() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->IsDeadState() : false;
}

EARAffinityColor AARPlayerStateBase::GetInvaderPlayerColor() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetInvaderPlayerColor() : EARAffinityColor::None;
}

int32 AARPlayerStateBase::GetInvaderComboCount() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetInvaderComboCount() : 0;
}

float AARPlayerStateBase::GetInvaderLastKillCreditServerTime() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetInvaderLastKillCreditServerTime() : -1.0f;
}

const FGameplayTagContainer& AARPlayerStateBase::GetActivatedInvaderUpgrades() const
{
	static const FGameplayTagContainer EmptyTags;
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetActivatedInvaderUpgrades() : EmptyTags;
}

bool AARPlayerStateBase::IsSpiceSharingActive() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->IsSpiceSharingActive() : false;
}

int32 AARPlayerStateBase::GetSpicyTrackCursorTier() const
{
	return CurrentCharacterRuntime ? CurrentCharacterRuntime->GetSpicyTrackCursorTier() : 0;
}

void AARPlayerStateBase::SetStrength(const float NewStrength)
{
	if (HasAuthority())
	{
		SetStrength_Internal(NewStrength);
		return;
	}

	ServerSetStrength(NewStrength);
}

void AARPlayerStateBase::ServerSetStrength_Implementation(const float NewStrength)
{
	SetStrength_Internal(NewStrength);
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

	UAbilitySystemComponent* SourceASC = GetASC();
	if (!HasAuthority() || !SourceASC || !TargetPlayer || TargetPlayer == this || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Mutual sharing cancels transfer entirely (prevents both players draining to nowhere).
	if (IsSpiceSharingActive() && TargetPlayer->IsSpiceSharingActive())
	{
		return;
	}

	if (!IsSpiceSharingActive())
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

	const float DrainRate = FMath::Max(0.0f, SourceASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceDrainRateAttribute()));
	const float ShareRatio = FMath::Max(0.0f, SourceASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceShareRatioAttribute()));
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

	if (!CurrentCharacterRuntime)
	{
		return;
	}

	CurrentCharacterRuntime->ResetInvaderCombo();
	ForceNetUpdate();
}

void AARPlayerStateBase::ReportInvaderKillCredit(EARAffinityColor EnemyColor, const float ServerTimeSeconds, const float ComboTimeoutSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentCharacterRuntime)
	{
		CurrentCharacterRuntime->ReportInvaderKillCredit(EnemyColor, ServerTimeSeconds, ComboTimeoutSeconds);
		ForceNetUpdate();
	}
}

void AARPlayerStateBase::MarkInvaderUpgradeActivated(FGameplayTag UpgradeTag)
{
	if (!HasAuthority() || !UpgradeTag.IsValid())
	{
		return;
	}

	if (!CurrentCharacterRuntime)
	{
		return;
	}

	CurrentCharacterRuntime->MarkInvaderUpgradeActivated(UpgradeTag);
	ForceNetUpdate();
}

void AARPlayerStateBase::ClearActivatedInvaderUpgrades()
{
	if (!HasAuthority() || !CurrentCharacterRuntime)
	{
		return;
	}

	CurrentCharacterRuntime->ClearActivatedInvaderUpgrades();
	ForceNetUpdate();
}

bool AARPlayerStateBase::HasActivatedInvaderUpgrade(FGameplayTag UpgradeTag) const
{
	if (!UpgradeTag.IsValid())
	{
		return false;
	}

	return GetActivatedInvaderUpgrades().HasTagExact(UpgradeTag);
}

void AARPlayerStateBase::SetPredictedSpiceValue(const float NewPredictedSpice)
{
	const float OldDisplayed = bHasPredictedSpiceValue ? PredictedSpiceValue : GetCoreAttributeValue(EARCoreAttributeType::Spice);
	PredictedSpiceValue = FMath::Max(0.0f, NewPredictedSpice);
	bHasPredictedSpiceValue = true;
	OnSpiceChanged.Broadcast(this, ResolveSignalCharacterTag(this), PredictedSpiceValue, OldDisplayed);
}

void AARPlayerStateBase::ClearPredictedSpiceValue()
{
	if (!bHasPredictedSpiceValue)
	{
		return;
	}

	const float OldPredicted = PredictedSpiceValue;
	const float AuthoritativeSpice = GetCoreAttributeValue(EARCoreAttributeType::Spice);
	bHasPredictedSpiceValue = false;
	PredictedSpiceValue = AuthoritativeSpice;
	OnSpiceChanged.Broadcast(this, ResolveSignalCharacterTag(this), AuthoritativeSpice, OldPredicted);
}

int32 AARPlayerStateBase::GetEffectiveSpicyTrackCursorTier() const
{
	return bHasPredictedSpicyTrackCursorTier ? PredictedSpicyTrackCursorTier : GetSpicyTrackCursorTier();
}

void AARPlayerStateBase::SetSpicyTrackCursorTier(int32 NewCursorTier)
{
	if (HasAuthority())
	{
		SetSpicyTrackCursorTier_Internal(NewCursorTier);
		return;
	}

	SetPredictedSpicyTrackCursorTier(NewCursorTier);
	ServerSetSpicyTrackCursorTier(NewCursorTier);
}

void AARPlayerStateBase::ServerSetSpicyTrackCursorTier_Implementation(int32 NewCursorTier)
{
	SetSpicyTrackCursorTier_Internal(NewCursorTier);
}

void AARPlayerStateBase::AdjustSpicyTrackCursorTier(const int32 DeltaTier)
{
	if (DeltaTier == 0)
	{
		return;
	}

	const int32 MaxSelectableTier = ResolveMaxSelectableSpicyTrackCursorTier();
	if (MaxSelectableTier <= 0)
	{
		SetSpicyTrackCursorTier(0);
		return;
	}

	// Track input should loop across valid selectable tiers only (1..MaxSelectableTier)
	// and never land on empty tier 0 when at least one tier is selectable.
	const int32 CurrentTier = FMath::Clamp(GetEffectiveSpicyTrackCursorTier(), 1, MaxSelectableTier);
	const int32 MinTier = 1;
	const int32 RangeSize = MaxSelectableTier - MinTier + 1;
	const int32 ZeroBasedCurrent = CurrentTier - MinTier;
	const int32 ZeroBasedWrapped = ((ZeroBasedCurrent + DeltaTier) % RangeSize + RangeSize) % RangeSize;
	const int32 WrappedTier = ZeroBasedWrapped + MinTier;

	SetSpicyTrackCursorTier(WrappedTier);
}

void AARPlayerStateBase::SnapSpicyTrackCursorToHighestSelectable()
{
	if (!HasAuthority())
	{
		return;
	}

	SetSpicyTrackCursorTier_Internal(ResolveMaxSelectableSpicyTrackCursorTier());
}

void AARPlayerStateBase::ResetSpicyTrackCursor()
{
	if (!HasAuthority())
	{
		return;
	}

	SetSpicyTrackCursorTier_Internal(0);
	ClearPredictedSpicyTrackCursorTier();
}

void AARPlayerStateBase::SetPredictedSpicyTrackCursorTier(const int32 NewPredictedCursorTier)
{
	const int32 OldDisplayedCursorTier = bHasPredictedSpicyTrackCursorTier ? PredictedSpicyTrackCursorTier : GetSpicyTrackCursorTier();
	PredictedSpicyTrackCursorTier = ClampSpicyTrackCursorTier(NewPredictedCursorTier);
	bHasPredictedSpicyTrackCursorTier = true;
	OnSpicyTrackCursorChanged.Broadcast(this, ResolveSignalCharacterTag(this), PredictedSpicyTrackCursorTier, OldDisplayedCursorTier);
}

void AARPlayerStateBase::ClearPredictedSpicyTrackCursorTier()
{
	if (!bHasPredictedSpicyTrackCursorTier)
	{
		return;
	}

	bHasPredictedSpicyTrackCursorTier = false;
	PredictedSpicyTrackCursorTier = GetSpicyTrackCursorTier();
}

void AARPlayerStateBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (CurrentCharacterTag.IsValid())
		{
			CharacterPicked = ARPlayer::GetCharacterChoiceForTag(CurrentCharacterTag);
		}
		else if (CharacterPicked != EARCharacterChoice::None)
		{
			CurrentCharacterTag = ARPlayer::GetCharacterTagForChoice(CharacterPicked);
		}

		if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
		{
			bool bCreatedRuntime = false;
			AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(this, CurrentCharacterTag, bCreatedRuntime);
			if (Runtime)
			{
				if (UARSaveGame* SaveGame = GetCurrentSaveGame(this))
				{
					FARCharacterSaveData CharacterState;
					int32 CharacterStateIndex = INDEX_NONE;
					if (SaveGame->FindCharacterStateDataByTag(CurrentCharacterTag, CharacterState, CharacterStateIndex))
					{
						Runtime->ApplySaveData(CharacterState);
					}
				}

				SetCurrentCharacterRuntime(Runtime);
				Runtime->SetCurrentPawn(GetPawn());
			}
		}
	}

	EnsureDefaultLoadoutIfEmpty();
	if (HasAuthority() && CurrentCharacterRuntime && CurrentCharacterRuntime->GetInvaderPlayerColor() == EARAffinityColor::None)
	{
		SetInvaderPlayerColor_Internal(ResolveDefaultInvaderPlayerColorFromCharacter(CharacterPicked), true);
	}
	BindTrackedAttributeDelegates();
	BindCurrentRuntimeDelegates();
	BroadcastTrackedAttributeSnapshot();
	EvaluateLifeStateFromASC();
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCurrentRuntimeDelegates();
	UnbindTrackedAttributeDelegates();
	Super::EndPlay(EndPlayReason);
}

void AARPlayerStateBase::OnRep_PlayerSlotId(const int32 OldSlotId)
{
	OnPlayerSlotIdChanged.Broadcast(this, PlayerSlotId, OldSlotId);
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::OnRep_CharacterPicked(EARCharacterChoice OldCharacter)
{
	OnCharacterPickedChanged.Broadcast(this, ResolveSignalCharacterTag(this), CharacterPicked, OldCharacter);
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::OnRep_CurrentCharacterTag(FGameplayTag OldCharacterTag)
{
	const FGameplayTag NormalizedOldCharacterTag = ARPlayer::NormalizeCharacterTag(OldCharacterTag);
	const FGameplayTag NormalizedNewCharacterTag = ResolveSignalCharacterTag(this);
	if (!NormalizedOldCharacterTag.MatchesTagExact(NormalizedNewCharacterTag))
	{
		OnCurrentCharacterTagChanged.Broadcast(NormalizedNewCharacterTag, NormalizedOldCharacterTag);
	}

	const EARCharacterChoice ResolvedChoice = ARPlayer::GetCharacterChoiceForTag(CurrentCharacterTag);
	if (ResolvedChoice != EARCharacterChoice::None && CharacterPicked != ResolvedChoice)
	{
		const EARCharacterChoice OldCharacter = CharacterPicked;
		CharacterPicked = ResolvedChoice;
		OnRep_CharacterPicked(OldCharacter);
	}
}

void AARPlayerStateBase::OnRep_CurrentCharacterRuntime()
{
	BindCurrentRuntimeDelegates();
	UnbindTrackedAttributeDelegates();
	BindTrackedAttributeDelegates();
	BroadcastTrackedAttributeSnapshot();
}

void AARPlayerStateBase::OnRep_DisplayName(const FString& OldDisplayName)
{
	OnDisplayNameChanged.Broadcast(this, ResolveSignalCharacterTag(this), DisplayName, OldDisplayName);
}

void AARPlayerStateBase::OnRep_IsReady(bool bOldReady)
{
	OnReadyStatusChanged.Broadcast(this, ResolveSignalCharacterTag(this), bIsReady, bOldReady);
	EvaluateTravelReadinessAndBroadcast();
}

void AARPlayerStateBase::OnRep_DialogueAutoAdvanceEnabled(bool bOldEnabled)
{
	OnDialogueAutoAdvancePreferenceChanged.Broadcast(this, bDialogueAutoAdvanceEnabled, bOldEnabled);
}

void AARPlayerStateBase::OnRep_IsSetup(bool bOldIsSetup)
{
	OnSetupStateChanged.Broadcast(bIsSetup, bOldIsSetup);
}

void AARPlayerStateBase::BindCurrentRuntimeDelegates()
{
	UnbindCurrentRuntimeDelegates();
	if (!CurrentCharacterRuntime)
	{
		return;
	}

	CurrentCharacterRuntime->OnLoadoutChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeLoadoutChanged);
	CurrentCharacterRuntime->OnDownedStateChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeDownedChanged);
	CurrentCharacterRuntime->OnDeadStateChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeDeadChanged);
	CurrentCharacterRuntime->OnInvaderPlayerColorChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeInvaderColorChanged);
	CurrentCharacterRuntime->OnInvaderComboChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeInvaderComboChanged);
	CurrentCharacterRuntime->OnActivatedInvaderUpgradesChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeActivatedUpgradesChanged);
	CurrentCharacterRuntime->OnSpiceSharingStateChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeSpiceSharingChanged);
	CurrentCharacterRuntime->OnSpicyTrackCursorChanged.AddDynamic(this, &AARPlayerStateBase::HandleRuntimeSpicyTrackCursorChanged);
	BoundRuntimeForDelegates = CurrentCharacterRuntime;
}

void AARPlayerStateBase::UnbindCurrentRuntimeDelegates()
{
	AARCharacterStateRuntime* Runtime = BoundRuntimeForDelegates.Get();
	if (!Runtime)
	{
		return;
	}

	Runtime->OnLoadoutChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeLoadoutChanged);
	Runtime->OnDownedStateChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeDownedChanged);
	Runtime->OnDeadStateChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeDeadChanged);
	Runtime->OnInvaderPlayerColorChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeInvaderColorChanged);
	Runtime->OnInvaderComboChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeInvaderComboChanged);
	Runtime->OnActivatedInvaderUpgradesChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeActivatedUpgradesChanged);
	Runtime->OnSpiceSharingStateChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeSpiceSharingChanged);
	Runtime->OnSpicyTrackCursorChanged.RemoveDynamic(this, &AARPlayerStateBase::HandleRuntimeSpicyTrackCursorChanged);
	BoundRuntimeForDelegates = nullptr;
}

void AARPlayerStateBase::HandleRuntimeLoadoutChanged(
	AARCharacterStateRuntime* SourceRuntime,
	const FGameplayTagContainer& NewLoadoutTags,
	const FGameplayTagContainer& OldLoadoutTags)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	OnLoadoutTagsChanged.Broadcast(this, ResolveSignalCharacterTag(this), NewLoadoutTags, OldLoadoutTags);
}

void AARPlayerStateBase::HandleRuntimeDownedChanged(
	AARCharacterStateRuntime* SourceRuntime,
	FGameplayTag CharacterTag,
	bool bNewDowned,
	bool bOldDowned)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	OnDownedStateChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), bNewDowned, bOldDowned);
}

void AARPlayerStateBase::HandleRuntimeDeadChanged(
	AARCharacterStateRuntime* SourceRuntime,
	FGameplayTag CharacterTag,
	bool bNewDead,
	bool bOldDead)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	OnDeadStateChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), bNewDead, bOldDead);
}

void AARPlayerStateBase::HandleRuntimeInvaderColorChanged(EARAffinityColor NewColor, EARAffinityColor OldColor)
{
	OnInvaderPlayerColorChanged.Broadcast(NewColor, OldColor);
}

void AARPlayerStateBase::HandleRuntimeInvaderComboChanged(
	AARCharacterStateRuntime* SourceRuntime,
	FGameplayTag CharacterTag,
	int32 NewCombo,
	int32 OldCombo)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	OnInvaderComboChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), NewCombo, OldCombo);
}

void AARPlayerStateBase::HandleRuntimeActivatedUpgradesChanged(
	AARCharacterStateRuntime* SourceRuntime,
	FGameplayTag CharacterTag,
	const FGameplayTagContainer& NewActivatedTags,
	const FGameplayTagContainer& OldActivatedTags)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	OnInvaderActivatedUpgradesChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), NewActivatedTags, OldActivatedTags);
}

void AARPlayerStateBase::HandleRuntimeSpiceSharingChanged(
	AARCharacterStateRuntime* SourceRuntime,
	FGameplayTag CharacterTag,
	bool bNewSharing,
	bool bOldSharing)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	OnSpiceSharingStateChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), bNewSharing, bOldSharing);
}

void AARPlayerStateBase::HandleRuntimeSpicyTrackCursorChanged(
	AARCharacterStateRuntime* SourceRuntime,
	FGameplayTag CharacterTag,
	int32 NewCursorTier,
	int32 OldCursorTier)
{
	if (SourceRuntime != CurrentCharacterRuntime)
	{
		return;
	}

	if (bHasPredictedSpicyTrackCursorTier)
	{
		const int32 OldPredictedCursorTier = PredictedSpicyTrackCursorTier;
		ClearPredictedSpicyTrackCursorTier();
		if (OldPredictedCursorTier != NewCursorTier)
		{
			OnSpicyTrackCursorChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), NewCursorTier, OldPredictedCursorTier);
		}
		return;
	}

	OnSpicyTrackCursorChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), NewCursorTier, OldCursorTier);
}

void AARPlayerStateBase::SetCharacterPicked_Internal(EARCharacterChoice NewCharacter)
{
	if (!HasAuthority())
	{
		return;
	}

	SetCurrentCharacterTagWithSwap_Internal(ARPlayer::GetCharacterTagForChoice(NewCharacter));
}

void AARPlayerStateBase::SetCurrentCharacterTagWithSwap_Internal(FGameplayTag NewCharacterTag)
{
	if (!HasAuthority())
	{
		return;
	}

	const FGameplayTag NormalizedNewTag = ARPlayer::NormalizeCharacterTag(NewCharacterTag);
	if (!NormalizedNewTag.IsValid())
	{
		SetCurrentCharacterTag_Internal(NewCharacterTag);
		return;
	}

	APlayerState* OccupyingPlayerState = nullptr;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GameState)
	{
		for (APlayerState* CandidatePlayerState : GameState->PlayerArray)
		{
			AARPlayerStateBase* Candidate = Cast<AARPlayerStateBase>(CandidatePlayerState);
			if (!Candidate || Candidate == this)
			{
				continue;
			}

			const FGameplayTag CandidateCharacterTag = ARPlayer::NormalizeCharacterTag(Candidate->GetCurrentCharacterTag());
			if (CandidateCharacterTag.IsValid() && CandidateCharacterTag.MatchesTagExact(NormalizedNewTag))
			{
				OccupyingPlayerState = CandidatePlayerState;
				break;
			}
		}
	}

	AARPlayerStateBase* OccupyingARPlayerState = Cast<AARPlayerStateBase>(OccupyingPlayerState);
	if (OccupyingARPlayerState)
	{
		FGameplayTag RequesterOldCharacterTag = ARPlayer::NormalizeCharacterTag(CurrentCharacterTag);
		if (!RequesterOldCharacterTag.IsValid() || RequesterOldCharacterTag.MatchesTagExact(NormalizedNewTag))
		{
			RequesterOldCharacterTag = ResolveAlternateCanonicalCharacterTag(NormalizedNewTag);
		}

		if (RequesterOldCharacterTag.IsValid() && !RequesterOldCharacterTag.MatchesTagExact(NormalizedNewTag))
		{
			OccupyingARPlayerState->SetCurrentCharacterTag_Internal(RequesterOldCharacterTag);
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[PlayerState] Character swap aborted for '%s': no safe alternate tag was available for occupying player '%s'."),
				*GetNameSafe(this),
				*GetNameSafe(OccupyingARPlayerState));
			return;
		}
	}

	SetCurrentCharacterTag_Internal(NormalizedNewTag);
}

void AARPlayerStateBase::SetCurrentCharacterTag_Internal(FGameplayTag NewCharacterTag, const bool bMarkSaveDirty)
{
	if (!HasAuthority())
	{
		return;
	}

	const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(NewCharacterTag);
	const EARCharacterChoice NewCharacter = ARPlayer::GetCharacterChoiceForTag(NormalizedTag);
	if (CurrentCharacterTag == NormalizedTag && CharacterPicked == NewCharacter)
	{
		return;
	}

	FGameplayTagContainer NextProjectedLoadout;
	bool bHasProjectedLoadout = false;

	const FGameplayTagContainer PreviousLoadoutTags = GetCurrentCharacterLoadoutTags();
	CacheCharacterOwnedLoadout(CurrentCharacterTag, PreviousLoadoutTags);

	UARSaveGame* SaveGame = GetCurrentSaveGame(this);
	if (SaveGame && CurrentCharacterTag.IsValid())
	{
		FARCharacterSaveData& CurrentCharacterState = SaveGame->FindOrAddCharacterStateData(CurrentCharacterTag);
		if (CurrentCharacterRuntime
			&& ARPlayer::NormalizeCharacterTag(CurrentCharacterRuntime->GetCharacterTag()).MatchesTagExact(ARPlayer::NormalizeCharacterTag(CurrentCharacterTag)))
		{
			CurrentCharacterRuntime->WriteSaveData(CurrentCharacterState);
		}
		else
		{
			CurrentCharacterState.LoadoutTags = PreviousLoadoutTags;
		}
	}

	if (FARPlayerStateSaveData* PlayerSaveData = FindOrAddCurrentSavePlayerData(this))
	{
		PlayerSaveData->Identity = BuildPlayerIdentityForSave(this);
		PlayerSaveData->bDialogueAutoAdvanceEnabled = bDialogueAutoAdvanceEnabled;

		PlayerSaveData->CurrentCharacterTag = NormalizedTag;
		PlayerSaveData->SyncCharacterSelectionFromCurrentTag();
	}

	bHasProjectedLoadout = TryResolveCharacterOwnedLoadout(NormalizedTag, NextProjectedLoadout);
	if (!bHasProjectedLoadout)
	{
		NextProjectedLoadout.Reset();
	}

	const EARCharacterChoice OldCharacter = CharacterPicked;
	const FGameplayTag OldCharacterTag = CurrentCharacterTag;
	CurrentCharacterTag = NormalizedTag;
	CharacterPicked = NewCharacter;
	const FGameplayTag NormalizedOldCharacterTag = ARPlayer::NormalizeCharacterTag(OldCharacterTag);
	const FGameplayTag NormalizedNewCharacterTag = ResolveSignalCharacterTag(this);
	if (!NormalizedOldCharacterTag.MatchesTagExact(NormalizedNewCharacterTag))
	{
		OnCurrentCharacterTagChanged.Broadcast(NormalizedNewCharacterTag, NormalizedOldCharacterTag);
	}

	if (OldCharacter != CharacterPicked)
	{
		OnRep_CharacterPicked(OldCharacter);
	}
	else if (OldCharacterTag != CurrentCharacterTag)
	{
		EvaluateTravelReadinessAndBroadcast();
	}

	SetLoadoutTags_Internal(NextProjectedLoadout, bMarkSaveDirty);
	EnsureDefaultLoadoutIfEmpty();

	if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		bool bCreatedRuntime = false;
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(this, CurrentCharacterTag, bCreatedRuntime))
		{
			if (bCreatedRuntime)
			{
				if (UARSaveGame* SaveGameForRuntimeHydration = GetCurrentSaveGame(this))
				{
					FARCharacterSaveData CharacterState;
					int32 CharacterStateIndex = INDEX_NONE;
					if (SaveGameForRuntimeHydration->FindCharacterStateDataByTag(CurrentCharacterTag, CharacterState, CharacterStateIndex))
					{
						Runtime->ApplySaveData(CharacterState);
					}
				}
			}

			SetCurrentCharacterRuntime(Runtime);
			Runtime->SetCurrentPawn(GetPawn());
		}
	}

	// Character identity drives baseline invader color. Re-evaluate after tag/runtime changes so
	// swap flows that bind runtime first cannot leave stale previous-character color.
	EvaluateInvaderColorFromASCOverrideTags();

	ForceNetUpdate();
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

	if (CurrentCharacterRuntime && !bForceBroadcast && CurrentCharacterRuntime->GetInvaderPlayerColor() == NewColor)
	{
		return;
	}

	const EARAffinityColor OldColor = GetInvaderPlayerColor();

	if (CurrentCharacterRuntime)
	{
		CurrentCharacterRuntime->SetInvaderPlayerColor(NewColor);
	}
	else
	{
		return;
	}

	ForceNetUpdate();

	if (NewColor == EARAffinityColor::None || NewColor == EARAffinityColor::White)
	{
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[InvaderSpice|Color] Player '%s' entered non-baseline color %d (Old=%d PlayerSlotId=%d Character=%d)."),
			*GetNameSafe(this),
			static_cast<int32>(NewColor),
			static_cast<int32>(OldColor),
			PlayerSlotId,
			static_cast<int32>(CharacterPicked));
	}
}

void AARPlayerStateBase::SetSpiceSharingActive_Internal(const bool bNewIsSharing, const bool bForceBroadcast)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!CurrentCharacterRuntime)
	{
		return;
	}

	if (!bForceBroadcast && CurrentCharacterRuntime->IsSpiceSharingActive() == bNewIsSharing)
	{
		return;
	}

	CurrentCharacterRuntime->SetSpiceSharingActive(bNewIsSharing);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetSpicyTrackCursorTier_Internal(int32 NewCursorTier, const bool bForceBroadcast)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!CurrentCharacterRuntime)
	{
		return;
	}

	const int32 ClampedCursor = ClampSpicyTrackCursorTier(NewCursorTier);
	if (!bForceBroadcast && CurrentCharacterRuntime->GetSpicyTrackCursorTier() == ClampedCursor)
	{
		return;
	}

	CurrentCharacterRuntime->SetSpicyTrackCursorTier(ClampedCursor);
	ForceNetUpdate();
}

int32 AARPlayerStateBase::ClampSpicyTrackCursorTier(const int32 RequestedCursorTier) const
{
	const int32 MaxSelectableTier = ResolveMaxSelectableSpicyTrackCursorTier();
	return FMath::Clamp(RequestedCursorTier, 0, FMath::Max(0, MaxSelectableTier));
}

int32 AARPlayerStateBase::ResolveMaxSelectableSpicyTrackCursorTier() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AARInvaderGameState* InvaderGameState = World->GetGameState<AARInvaderGameState>())
		{
			return InvaderGameState->GetMaxSelectableTrackCursorTierForPlayer(this);
		}
	}

	// Safe fallback when not in Invader mode/runtime context.
	return 0;
}

int32 AARPlayerStateBase::ResolveSpiceTierFromValue(const float SpiceValue) const
{
	const UARInvaderSpicyTrackSettings* Settings = GetDefault<UARInvaderSpicyTrackSettings>();
	const int32 SpicePerTier = Settings ? FMath::Max(1, Settings->SpicePerTier) : 100;
	const float SanitizedSpice = FMath::Max(0.0f, SpiceValue);
	return FMath::Max(0, FMath::FloorToInt(SanitizedSpice / static_cast<float>(SpicePerTier)));
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
	return (ARPlayer::GetCharacterChoiceForTag(CurrentCharacterTag) == EARCharacterChoice::Sister) ? EARAffinityColor::Red : EARAffinityColor::Blue;
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

	if (!CurrentCharacterRuntime)
	{
		return;
	}

	const bool bResolvedDowned = CurrentCharacterRuntime->IsDeadState() ? false : bNewDowned;
	if (CurrentCharacterRuntime->IsDowned() == bResolvedDowned)
	{
		return;
	}

	CurrentCharacterRuntime->SetDownedState(bResolvedDowned);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetDead_Internal(bool bNewDead)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!CurrentCharacterRuntime)
	{
		return;
	}

	if (CurrentCharacterRuntime->IsDeadState() == bNewDead)
	{
		return;
	}

	CurrentCharacterRuntime->SetDeadState(bNewDead);
	ForceNetUpdate();
}

void AARPlayerStateBase::SetDialogueAutoAdvanceEnabled_Internal(bool bEnabled)
{
	if (!HasAuthority() || bDialogueAutoAdvanceEnabled == bEnabled)
	{
		return;
	}

	const bool bOldEnabled = bDialogueAutoAdvanceEnabled;
	bDialogueAutoAdvanceEnabled = bEnabled;
	OnRep_DialogueAutoAdvanceEnabled(bOldEnabled);
	ForceNetUpdate();

	if (FARPlayerStateSaveData* PlayerSaveData = FindOrAddCurrentSavePlayerData(this))
	{
		PlayerSaveData->Identity = BuildPlayerIdentityForSave(this);
		PlayerSaveData->bDialogueAutoAdvanceEnabled = bDialogueAutoAdvanceEnabled;
		PlayerSaveData->CurrentCharacterTag = CurrentCharacterTag;
		PlayerSaveData->SyncCharacterSelectionFromCurrentTag();
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

	if (CharacterPicked == EARCharacterChoice::None || !CurrentCharacterTag.IsValid())
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

		const EARCharacterChoice PreferredChoice = EARCharacterChoice::Brother;
		const EARCharacterChoice AlternateChoice = EARCharacterChoice::Sister;

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
			// Never leave character unset; default to Brother when both are currently occupied.
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[PlayerState] Ready prerequisites found both character choices already taken for '%s'; assigning default fallback %d."),
				*GetNameSafe(this),
				static_cast<int32>(PreferredChoice));
			SetCharacterPicked_Internal(PreferredChoice);
		}
	}

	return true;
}

void AARPlayerStateBase::SetLoadoutTags_Internal(const FGameplayTagContainer& NewLoadoutTags, const bool bMarkSaveDirty)
{
	if (!HasAuthority())
	{
		return;
	}

	FGameplayTagContainer NormalizedTags = NewLoadoutTags;
	NormalizeLoadoutTagsForSlotRules(NormalizedTags);

	const FGameplayTagContainer CurrentLoadoutTags = GetCurrentCharacterLoadoutTags();
	if (CurrentLoadoutTags == NormalizedTags)
	{
		return;
	}

	const FGameplayTagContainer OldLoadoutTags = CurrentLoadoutTags;
	if (!CurrentCharacterRuntime)
	{
		UE_LOG(ARLog, Warning, TEXT("[PlayerState] SetLoadoutTags_Internal ignored for '%s': no current character runtime."), *GetNameSafe(this));
		return;
	}

	CurrentCharacterRuntime->SetLoadoutTags(NormalizedTags);
	OnRep_Loadout(OldLoadoutTags);
	ForceNetUpdate();

	CacheCharacterOwnedLoadout(CurrentCharacterTag, NormalizedTags);

	// Mirror the projected runtime loadout into the canonical character-owned save row.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame(); SaveGame && CurrentCharacterTag.IsValid())
			{
				FARCharacterSaveData& ActiveCharacterState = SaveGame->FindOrAddCharacterStateData(CurrentCharacterTag);
				if (CurrentCharacterRuntime
					&& ARPlayer::NormalizeCharacterTag(CurrentCharacterRuntime->GetCharacterTag()).MatchesTagExact(ARPlayer::NormalizeCharacterTag(CurrentCharacterTag)))
				{
					CurrentCharacterRuntime->WriteSaveData(ActiveCharacterState);
				}
				else
				{
					ActiveCharacterState.LoadoutTags = NormalizedTags;
				}
			}

			if (FARPlayerStateSaveData* PlayerSaveData = FindOrAddCurrentSavePlayerData(this))
			{
				PlayerSaveData->Identity = BuildPlayerIdentityForSave(this);
				PlayerSaveData->CurrentCharacterTag = CurrentCharacterTag;
				PlayerSaveData->bDialogueAutoAdvanceEnabled = bDialogueAutoAdvanceEnabled;
				PlayerSaveData->SyncCharacterSelectionFromCurrentTag();
			}

			if (bMarkSaveDirty)
			{
				SaveSubsystem->MarkSaveDirty();
			}
		}
	}
}

void AARPlayerStateBase::CacheCharacterOwnedLoadout(const FGameplayTag CharacterTag, const FGameplayTagContainer& LoadoutTagsToCache)
{
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return;
	}

	if (UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->FindCharacterRuntimeForPlayer(this, NormalizedCharacterTag))
		{
			Runtime->SetLoadoutTags(LoadoutTagsToCache);
		}
	}
}

bool AARPlayerStateBase::TryResolveCharacterOwnedLoadout(const FGameplayTag CharacterTag, FGameplayTagContainer& OutLoadoutTags) const
{
	OutLoadoutTags.Reset();
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return false;
	}

	if (const UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->FindCharacterRuntimeForPlayer(this, NormalizedCharacterTag))
		{
			OutLoadoutTags = Runtime->GetLoadoutTags();
			return true;
		}
	}

	if (const UARSaveGame* SaveGame = GetCurrentSaveGame(this))
	{
		FARCharacterSaveData CharacterState;
		int32 CharacterIndex = INDEX_NONE;
		if (SaveGame->FindCharacterStateDataByTag(NormalizedCharacterTag, CharacterState, CharacterIndex))
		{
			OutLoadoutTags = CharacterState.LoadoutTags;
			return true;
		}
	}

	return false;
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
		// Keep default baseline intact for editor/testing flows where callers may submit empty tags.
		EnsureDefaultLoadoutIfEmpty();
		return;
	}

	FGameplayTag SlotRootTag;
	const bool bHasSlotRoot = UGameplayTagUtilities::TryGetTagAtDepth(NewTag, 2, SlotRootTag);

	FGameplayTagContainer NewLoadout = GetCurrentCharacterLoadoutTags();
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

	const FGameplayTagContainer CurrentLoadout = GetCurrentCharacterLoadoutTags();
	if (!CurrentLoadout.HasTagExact(TagToRemove))
	{
		return;
	}

	FGameplayTagContainer NewLoadout = CurrentLoadout;
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

	// Seamless-travel copy is intentionally explicit for PlayerState identity/runtime fields.
	// Avoid generic StructSerializable by-name overlays here; BP-defined fallback structs can
	// transiently stamp stale identity mirrors during travel handoff and create collisions.
	TargetPS->PlayerSlotId = PlayerSlotId;

	FGameplayTag CopiedCharacterTag = CurrentCharacterTag.IsValid()
		? CurrentCharacterTag
		: ARPlayer::GetCharacterTagForChoice(CharacterPicked);
	CopiedCharacterTag = ARPlayer::NormalizeCharacterTag(CopiedCharacterTag);
	if (!CopiedCharacterTag.IsValid())
	{
		CopiedCharacterTag = ARPlayer::GetCharacterTagForChoice(CharacterPicked);
	}

	TargetPS->CurrentCharacterTag = CopiedCharacterTag;
	TargetPS->CharacterPicked = ARPlayer::GetCharacterChoiceForTag(CopiedCharacterTag);
	if (TargetPS->CharacterPicked == EARCharacterChoice::None)
	{
		TargetPS->CharacterPicked = CharacterPicked;
	}
	TargetPS->DisplayName = DisplayName;
	TargetPS->bDialogueAutoAdvanceEnabled = bDialogueAutoAdvanceEnabled;
	TargetPS->bIsSetup = true;
	TargetPS->bIsReady = false;
	TargetPS->bCachedTravelReady = TargetPS->IsTravelReady();
	TargetPS->ForceNetUpdate();
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
	if (!HasAuthority() || !GetCurrentCharacterLoadoutTags().IsEmpty())
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
	UAbilitySystemComponent* ActiveASC = GetASC();
	if (!ActiveASC)
	{
		return;
	}

	if (BoundTrackedASC.Get() != ActiveASC)
	{
		UnbindTrackedAttributeDelegates();
	}

	if (!HealthChangedDelegateHandle.IsValid())
	{
		HealthChangedDelegateHandle = ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleHealthAttributeChanged);
	}

	if (!MaxHealthChangedDelegateHandle.IsValid())
	{
		MaxHealthChangedDelegateHandle = ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxHealthAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleMaxHealthAttributeChanged);
	}

	if (!SpiceChangedDelegateHandle.IsValid())
	{
		SpiceChangedDelegateHandle = ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetSpiceAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleSpiceAttributeChanged);
	}

	if (!MaxSpiceChangedDelegateHandle.IsValid())
	{
		MaxSpiceChangedDelegateHandle = ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxSpiceAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleMaxSpiceAttributeChanged);
	}

	if (!MoveSpeedChangedDelegateHandle.IsValid())
	{
		MoveSpeedChangedDelegateHandle = ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleMoveSpeedAttributeChanged);
	}

	if (!StrengthChangedDelegateHandle.IsValid())
	{
		StrengthChangedDelegateHandle = ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetStrengthAttribute())
			.AddUObject(this, &AARPlayerStateBase::HandleStrengthAttributeChanged);
	}

	if (!DownedTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false);
		if (DownedTag.IsValid())
		{
			DownedTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(DownedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleDownedTagChanged);
		}
	}

	if (!DeadTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
		if (DeadTag.IsValid())
		{
			DeadTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleDeadTagChanged);
		}
	}

	if (!ColorNoneTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
		if (ColorNoneTag.IsValid())
		{
			ColorNoneTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(ColorNoneTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!ColorRedTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
		if (ColorRedTag.IsValid())
		{
			ColorRedTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(ColorRedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!ColorWhiteTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
		if (ColorWhiteTag.IsValid())
		{
			ColorWhiteTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(ColorWhiteTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!ColorBlueTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);
		if (ColorBlueTag.IsValid())
		{
			ColorBlueTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(ColorBlueTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleInvaderColorOverrideTagChanged);
		}
	}

	if (!SharingSpiceTagChangedDelegateHandle.IsValid())
	{
		const FGameplayTag SharingSpiceTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sharing"), false);
		if (SharingSpiceTag.IsValid())
		{
			SharingSpiceTagChangedDelegateHandle = ActiveASC->RegisterGameplayTagEvent(SharingSpiceTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AARPlayerStateBase::HandleSpiceSharingTagChanged);
			SetSpiceSharingActive_Internal(ActiveASC->HasMatchingGameplayTag(SharingSpiceTag), true);
		}
	}

	BoundTrackedASC = ActiveASC;
	EvaluateInvaderColorFromASCOverrideTags();
}

void AARPlayerStateBase::UnbindTrackedAttributeDelegates()
{
	UAbilitySystemComponent* ActiveASC = BoundTrackedASC.Get();
	if (!ActiveASC)
	{
		HealthChangedDelegateHandle.Reset();
		MaxHealthChangedDelegateHandle.Reset();
		SpiceChangedDelegateHandle.Reset();
		MaxSpiceChangedDelegateHandle.Reset();
		MoveSpeedChangedDelegateHandle.Reset();
		StrengthChangedDelegateHandle.Reset();
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
		ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}

	if (MaxHealthChangedDelegateHandle.IsValid())
	{
		ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
		MaxHealthChangedDelegateHandle.Reset();
	}

	if (SpiceChangedDelegateHandle.IsValid())
	{
		ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetSpiceAttribute()).Remove(SpiceChangedDelegateHandle);
		SpiceChangedDelegateHandle.Reset();
	}

	if (MaxSpiceChangedDelegateHandle.IsValid())
	{
		ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMaxSpiceAttribute()).Remove(MaxSpiceChangedDelegateHandle);
		MaxSpiceChangedDelegateHandle.Reset();
	}

	if (MoveSpeedChangedDelegateHandle.IsValid())
	{
		ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetMoveSpeedAttribute()).Remove(MoveSpeedChangedDelegateHandle);
		MoveSpeedChangedDelegateHandle.Reset();
	}

	if (StrengthChangedDelegateHandle.IsValid())
	{
		ActiveASC->GetGameplayAttributeValueChangeDelegate(UARAttributeSetCore::GetStrengthAttribute()).Remove(StrengthChangedDelegateHandle);
		StrengthChangedDelegateHandle.Reset();
	}

	const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false);
	if (DownedTag.IsValid() && DownedTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(DownedTag, EGameplayTagEventType::NewOrRemoved).Remove(DownedTagChangedDelegateHandle);
		DownedTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
	if (DeadTag.IsValid() && DeadTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagChangedDelegateHandle);
		DeadTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	if (ColorNoneTag.IsValid() && ColorNoneTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(ColorNoneTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorNoneTagChangedDelegateHandle);
		ColorNoneTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	if (ColorRedTag.IsValid() && ColorRedTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(ColorRedTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorRedTagChangedDelegateHandle);
		ColorRedTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	if (ColorWhiteTag.IsValid() && ColorWhiteTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(ColorWhiteTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorWhiteTagChangedDelegateHandle);
		ColorWhiteTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);
	if (ColorBlueTag.IsValid() && ColorBlueTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(ColorBlueTag, EGameplayTagEventType::NewOrRemoved).Remove(ColorBlueTagChangedDelegateHandle);
		ColorBlueTagChangedDelegateHandle.Reset();
	}

	const FGameplayTag SharingSpiceTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sharing"), false);
	if (SharingSpiceTag.IsValid() && SharingSpiceTagChangedDelegateHandle.IsValid())
	{
		ActiveASC->RegisterGameplayTagEvent(SharingSpiceTag, EGameplayTagEventType::NewOrRemoved).Remove(SharingSpiceTagChangedDelegateHandle);
		SharingSpiceTagChangedDelegateHandle.Reset();
	}

	BoundTrackedASC = nullptr;
}

void AARPlayerStateBase::BroadcastTrackedAttributeSnapshot()
{
	const FARPlayerCoreAttributeSnapshot Snapshot = GetCoreAttributeSnapshot();
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Health, Snapshot.Health, Snapshot.Health);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxHealth, Snapshot.MaxHealth, Snapshot.MaxHealth);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Spice, Snapshot.Spice, Snapshot.Spice);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxSpice, Snapshot.MaxSpice, Snapshot.MaxSpice);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MoveSpeed, Snapshot.MoveSpeed, Snapshot.MoveSpeed);
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Strength, Snapshot.Strength, Snapshot.Strength);

	OnHealthChanged.Broadcast(this, ResolveSignalCharacterTag(this), Snapshot.Health, Snapshot.Health);
	OnMaxHealthChanged.Broadcast(this, ResolveSignalCharacterTag(this), Snapshot.MaxHealth, Snapshot.MaxHealth);
	OnSpiceChanged.Broadcast(this, ResolveSignalCharacterTag(this), Snapshot.Spice, Snapshot.Spice);
	OnMaxSpiceChanged.Broadcast(this, ResolveSignalCharacterTag(this), Snapshot.MaxSpice, Snapshot.MaxSpice);
	OnMoveSpeedChanged.Broadcast(this, ResolveSignalCharacterTag(this), Snapshot.MoveSpeed, Snapshot.MoveSpeed);
	OnStrengthChanged.Broadcast(this, ResolveSignalCharacterTag(this), Snapshot.Strength, Snapshot.Strength);
	const int32 CurrentCursorTier = GetSpicyTrackCursorTier();
	OnSpicyTrackCursorChanged.Broadcast(this, ResolveSignalCharacterTag(this), CurrentCursorTier, CurrentCursorTier);
}

void AARPlayerStateBase::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Health, ChangeData.NewValue, ChangeData.OldValue);
	OnHealthChanged.Broadcast(this, ResolveSignalCharacterTag(this), ChangeData.NewValue, ChangeData.OldValue);
	EvaluateLifeStateFromASC();
}

void AARPlayerStateBase::HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxHealth, ChangeData.NewValue, ChangeData.OldValue);
	OnMaxHealthChanged.Broadcast(this, ResolveSignalCharacterTag(this), ChangeData.NewValue, ChangeData.OldValue);
	EvaluateLifeStateFromASC();
}

void AARPlayerStateBase::HandleSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Spice, ChangeData.NewValue, ChangeData.OldValue);
	OnSpiceChanged.Broadcast(this, ResolveSignalCharacterTag(this), ChangeData.NewValue, ChangeData.OldValue);
	bHasPredictedSpiceValue = false;
	PredictedSpiceValue = ChangeData.NewValue;

	if (HasAuthority())
	{
		const int32 OldSpiceTier = ResolveSpiceTierFromValue(ChangeData.OldValue);
		const int32 NewSpiceTier = ResolveSpiceTierFromValue(ChangeData.NewValue);
		if (NewSpiceTier > OldSpiceTier)
		{
			SnapSpicyTrackCursorToHighestSelectable();
		}
		else
		{
			// Keep cursor valid if spice dropped below current selection.
			SetSpicyTrackCursorTier_Internal(GetSpicyTrackCursorTier());
		}
	}
}

void AARPlayerStateBase::HandleMaxSpiceAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MaxSpice, ChangeData.NewValue, ChangeData.OldValue);
	OnMaxSpiceChanged.Broadcast(this, ResolveSignalCharacterTag(this), ChangeData.NewValue, ChangeData.OldValue);

	if (HasAuthority())
	{
		SetSpicyTrackCursorTier_Internal(GetSpicyTrackCursorTier());
	}
}

void AARPlayerStateBase::HandleMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::MoveSpeed, ChangeData.NewValue, ChangeData.OldValue);
	OnMoveSpeedChanged.Broadcast(this, ResolveSignalCharacterTag(this), ChangeData.NewValue, ChangeData.OldValue);
}

void AARPlayerStateBase::HandleStrengthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	BroadcastCoreAttributeChanged(EARCoreAttributeType::Strength, ChangeData.NewValue, ChangeData.OldValue);
	OnStrengthChanged.Broadcast(this, ResolveSignalCharacterTag(this), ChangeData.NewValue, ChangeData.OldValue);
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
	UAbilitySystemComponent* ActiveASC = GetASC();
	if (!HasAuthority() || !ActiveASC)
	{
		return;
	}

	const FGameplayTag SharingSpiceTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sharing"), false);
	const bool bSharingNow = SharingSpiceTag.IsValid() && ActiveASC->HasMatchingGameplayTag(SharingSpiceTag);
	SetSpiceSharingActive_Internal(bSharingNow);
}

void AARPlayerStateBase::EvaluateInvaderColorFromASCOverrideTags()
{
	if (!HasAuthority() || !GetASC())
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
	const UAbilitySystemComponent* ActiveASC = GetASC();
	if (!ActiveASC)
	{
		return ResolveDefaultInvaderPlayerColorFromCharacter(CharacterPicked);
	}

	const FGameplayTag ColorNoneTag = FGameplayTag::RequestGameplayTag(TEXT("Color.None"), false);
	const FGameplayTag ColorWhiteTag = FGameplayTag::RequestGameplayTag(TEXT("Color.White"), false);
	const FGameplayTag ColorRedTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Red"), false);
	const FGameplayTag ColorBlueTag = FGameplayTag::RequestGameplayTag(TEXT("Color.Blue"), false);

	// Override precedence: None > White > Red > Blue.
	if (ColorNoneTag.IsValid() && ActiveASC->HasMatchingGameplayTag(ColorNoneTag))
	{
		return EARAffinityColor::None;
	}

	if (ColorWhiteTag.IsValid() && ActiveASC->HasMatchingGameplayTag(ColorWhiteTag))
	{
		return EARAffinityColor::White;
	}

	if (ColorRedTag.IsValid() && ActiveASC->HasMatchingGameplayTag(ColorRedTag))
	{
		return EARAffinityColor::Red;
	}

	if (ColorBlueTag.IsValid() && ActiveASC->HasMatchingGameplayTag(ColorBlueTag))
	{
		return EARAffinityColor::Blue;
	}

	return ResolveDefaultInvaderPlayerColorFromCharacter(CharacterPicked);
}

void AARPlayerStateBase::ApplyInvaderColorGameplayTags(const EARAffinityColor NewColor)
{
	UAbilitySystemComponent* ActiveASC = GetASC();
	if (!HasAuthority() || !ActiveASC || bApplyingInvaderColorTags)
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
		ActiveASC->RemoveLooseGameplayTags(AllColorTags, 1, EGameplayTagReplicationState::TagOnly);

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
			ActiveASC->AddLooseGameplayTags(ActiveColorTag, 1, EGameplayTagReplicationState::TagOnly);
		}
		bApplyingInvaderColorTags = false;
	}
}

void AARPlayerStateBase::EvaluateLifeStateFromASC()
{
	UAbilitySystemComponent* ActiveASC = GetASC();
	if (!HasAuthority() || !ActiveASC)
	{
		return;
	}

	const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
	const bool bDeadFromTag = DeadTag.IsValid() && ActiveASC->HasMatchingGameplayTag(DeadTag);
	const bool bDownedFromTag = DownedTag.IsValid() && ActiveASC->HasMatchingGameplayTag(DownedTag);

	const float MaxHealth = ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetMaxHealthAttribute());
	const float Health = ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	const bool bDownedFromHealth = (MaxHealth > 0.f && Health <= 0.f);

	SetDead_Internal(bDeadFromTag);
	SetDowned_Internal(!bDeadFromTag && (bDownedFromTag || bDownedFromHealth));
}

void AARPlayerStateBase::BroadcastCoreAttributeChanged(EARCoreAttributeType AttributeType, float NewValue, float OldValue)
{
	OnCoreAttributeChanged.Broadcast(this, ResolveSignalCharacterTag(this), AttributeType, NewValue, OldValue);
}

void AARPlayerStateBase::SetSpiceMeter_Internal(float NewSpiceValue)
{
	UAbilitySystemComponent* ActiveASC = GetASC();
	if (!HasAuthority() || !ActiveASC)
	{
		return;
	}

	float MaxSpice = ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetMaxSpiceAttribute());
	if (MaxSpice <= KINDA_SMALL_NUMBER)
	{
		// Defensive recovery: if this runtime missed a prior max-spice sync, recover from
		// Invader's shared-cap authority (tier1=100, +100 per tier) before clamping.
		if (const AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
		{
			MaxSpice = static_cast<float>(InvaderGameState->GetSharedMaxSpice());
			ActiveASC->SetNumericAttributeBase(UARAttributeSetCore::GetMaxSpiceAttribute(), MaxSpice);
		}
	}

	const float ClampedValue = FMath::Clamp(NewSpiceValue, 0.f, FMath::Max(0.f, MaxSpice));
	const float CurrentValue = ActiveASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceAttribute());
	if (FMath::IsNearlyEqual(CurrentValue, ClampedValue))
	{
		return;
	}

	ActiveASC->SetNumericAttributeBase(UARAttributeSetCore::GetSpiceAttribute(), ClampedValue);
}

void AARPlayerStateBase::SetStrength_Internal(const float NewStrength)
{
	UAbilitySystemComponent* ActiveASC = GetASC();
	if (!HasAuthority() || !ActiveASC)
	{
		return;
	}

	ActiveASC->SetNumericAttributeBase(UARAttributeSetCore::GetStrengthAttribute(), FMath::Max(0.0f, NewStrength));

	if (CurrentCharacterRuntime)
	{
		CurrentCharacterRuntime->SetStrength(NewStrength);
	}
}

bool AARPlayerStateBase::IsTravelReady() const
{
	return ARPlayer::NormalizeCharacterTag(CurrentCharacterTag).IsValid()
		&& bIsReady;
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

