#include "ARCharacterStateRuntime.h"

#include "ARAttributeSetCore.h"
#include "ARAttributeSetPlayer.h"
#include "ARCharacterSubsystem.h"
#include "ARInvaderSpicyTrackSettings.h"
#include "ARPlayerStateBase.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AARCharacterStateRuntime::AARCharacterStateRuntime()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(20.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetCore = CreateDefaultSubobject<UARAttributeSetCore>(TEXT("AttributeSetCore"));
	AttributeSetPlayer = CreateDefaultSubobject<UARAttributeSetPlayer>(TEXT("AttributeSetPlayer"));
}

void AARCharacterStateRuntime::BeginPlay()
{
	Super::BeginPlay();
	RefreshAbilityActorInfo();

	if (UWorld* World = GetWorld())
	{
		if (UARCharacterSubsystem* CharacterSubsystem = World->GetSubsystem<UARCharacterSubsystem>())
		{
			CharacterSubsystem->RegisterRuntime(this);
		}
	}
}

void AARCharacterStateRuntime::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UARCharacterSubsystem* CharacterSubsystem = World->GetSubsystem<UARCharacterSubsystem>())
		{
			CharacterSubsystem->UnregisterRuntime(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AARCharacterStateRuntime::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AARCharacterStateRuntime, CharacterTag);
	DOREPLIFETIME(AARCharacterStateRuntime, OwningPlayerState);
	DOREPLIFETIME(AARCharacterStateRuntime, CurrentPawn);
	DOREPLIFETIME(AARCharacterStateRuntime, LoadoutTags);
	DOREPLIFETIME(AARCharacterStateRuntime, CharacterProgressionTags);
	DOREPLIFETIME(AARCharacterStateRuntime, bIsDowned);
	DOREPLIFETIME(AARCharacterStateRuntime, bIsDeadState);
	DOREPLIFETIME(AARCharacterStateRuntime, InvaderPlayerColor);
	DOREPLIFETIME(AARCharacterStateRuntime, InvaderComboCount);
	DOREPLIFETIME(AARCharacterStateRuntime, ActivatedInvaderUpgradeTags);
	DOREPLIFETIME(AARCharacterStateRuntime, bIsSharingSpice);
	DOREPLIFETIME(AARCharacterStateRuntime, SpicyTrackCursorTier);
}

UAbilitySystemComponent* AARCharacterStateRuntime::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AARCharacterStateRuntime::SetCharacterTag(FGameplayTag NewCharacterTag)
{
	if (!HasAuthority())
	{
		return;
	}

	const FGameplayTag OldTag = CharacterTag;
	const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(NewCharacterTag);
	if (OldTag == NormalizedTag)
	{
		return;
	}

	CharacterTag = NormalizedTag;
	OnRep_CharacterTag(OldTag);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::SetOwningPlayerState(AARPlayerStateBase* NewOwningPlayerState)
{
	if (!HasAuthority() || OwningPlayerState == NewOwningPlayerState)
	{
		return;
	}

	AARPlayerStateBase* OldOwningPlayerState = OwningPlayerState;
	OwningPlayerState = NewOwningPlayerState;
	OnRep_OwningPlayerState(OldOwningPlayerState);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::SetCurrentPawn(APawn* NewPawn)
{
	if (!HasAuthority() || CurrentPawn == NewPawn)
	{
		return;
	}

	APawn* OldPawn = CurrentPawn;
	CurrentPawn = NewPawn;
	OnRep_CurrentPawn(OldPawn);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::SetLoadoutTags(const FGameplayTagContainer& NewLoadoutTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (LoadoutTags == NewLoadoutTags)
	{
		return;
	}

	const FGameplayTagContainer OldLoadoutTags = LoadoutTags;
	LoadoutTags = NewLoadoutTags;
	OnRep_LoadoutTags(OldLoadoutTags);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::SetCharacterProgressionTags(const FGameplayTagContainer& NewCharacterProgressionTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CharacterProgressionTags == NewCharacterProgressionTags)
	{
		return;
	}

	CharacterProgressionTags = NewCharacterProgressionTags;
	ForceNetUpdate();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(CharacterTag);
				CharacterState.CharacterProgressionTags = CharacterProgressionTags;
			}

			SaveSubsystem->MarkSaveDirty();
		}
	}
}

bool AARCharacterStateRuntime::AddCharacterProgressionTag(FGameplayTag ProgressionTag)
{
	if (!HasAuthority() || !ProgressionTag.IsValid() || CharacterProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	CharacterProgressionTags.AddTag(ProgressionTag);
	ForceNetUpdate();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(CharacterTag);
				CharacterState.CharacterProgressionTags = CharacterProgressionTags;
			}

			SaveSubsystem->MarkSaveDirty();
		}
	}

	return true;
}

bool AARCharacterStateRuntime::RemoveCharacterProgressionTag(FGameplayTag ProgressionTag)
{
	if (!HasAuthority() || !ProgressionTag.IsValid() || !CharacterProgressionTags.HasTagExact(ProgressionTag))
	{
		return false;
	}

	CharacterProgressionTags.RemoveTag(ProgressionTag);
	ForceNetUpdate();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UARSaveSubsystem>())
		{
			if (UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
			{
				FARCharacterSaveData& CharacterState = SaveGame->FindOrAddCharacterStateData(CharacterTag);
				CharacterState.CharacterProgressionTags = CharacterProgressionTags;
			}

			SaveSubsystem->MarkSaveDirty();
		}
	}

	return true;
}

float AARCharacterStateRuntime::GetCoreAttributeValue(EARCoreAttributeType AttributeType) const
{
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}

	switch (AttributeType)
	{
	case EARCoreAttributeType::Health:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetHealthAttribute());
	case EARCoreAttributeType::MaxHealth:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMaxHealthAttribute());
	case EARCoreAttributeType::MoveSpeed:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetCore::GetMoveSpeedAttribute());
	default:
		return 0.0f;
	}
}

FARCharacterRuntimeCoreAttributeSnapshot AARCharacterStateRuntime::GetCoreAttributeSnapshot() const
{
	FARCharacterRuntimeCoreAttributeSnapshot Snapshot;
	Snapshot.Health = GetCoreAttributeValue(EARCoreAttributeType::Health);
	Snapshot.MaxHealth = GetCoreAttributeValue(EARCoreAttributeType::MaxHealth);
	Snapshot.MoveSpeed = GetCoreAttributeValue(EARCoreAttributeType::MoveSpeed);
	return Snapshot;
}

float AARCharacterStateRuntime::GetPlayerAttributeValue(EARPlayerAttributeType AttributeType) const
{
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}

	switch (AttributeType)
	{
	case EARPlayerAttributeType::Spice:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetPlayer::GetSpiceAttribute());
	case EARPlayerAttributeType::MaxSpice:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetPlayer::GetMaxSpiceAttribute());
	case EARPlayerAttributeType::Strength:
		return AbilitySystemComponent->GetNumericAttribute(UARAttributeSetPlayer::GetStrengthAttribute());
	default:
		return 0.0f;
	}
}

FARCharacterRuntimePlayerAttributeSnapshot AARCharacterStateRuntime::GetPlayerAttributeSnapshot() const
{
	FARCharacterRuntimePlayerAttributeSnapshot Snapshot;
	Snapshot.Spice = GetPlayerAttributeValue(EARPlayerAttributeType::Spice);
	Snapshot.MaxSpice = GetPlayerAttributeValue(EARPlayerAttributeType::MaxSpice);
	Snapshot.Strength = GetPlayerAttributeValue(EARPlayerAttributeType::Strength);
	return Snapshot;
}

float AARCharacterStateRuntime::GetSpiceNormalized() const
{
	const float MaxSpice = GetPlayerAttributeValue(EARPlayerAttributeType::MaxSpice);
	if (MaxSpice <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetPlayerAttributeValue(EARPlayerAttributeType::Spice) / MaxSpice, 0.0f, 1.0f);
}

void AARCharacterStateRuntime::SetSpiceMeter(float NewSpiceValue)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const float MaxSpice = AbilitySystemComponent->GetNumericAttribute(UARAttributeSetPlayer::GetMaxSpiceAttribute());
	const float ClampedValue = FMath::Clamp(NewSpiceValue, 0.0f, FMath::Max(0.0f, MaxSpice));
	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetPlayer::GetSpiceAttribute(), ClampedValue);
}

void AARCharacterStateRuntime::SetStrength(float NewStrength)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetPlayer::GetStrengthAttribute(), FMath::Max(0.0f, NewStrength));
}

void AARCharacterStateRuntime::SetDownedState(bool bNewDowned)
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
	OnRep_Downed(bOldDowned);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::SetDeadState(bool bNewDead)
{
	if (!HasAuthority() || bIsDeadState == bNewDead)
	{
		return;
	}

	const bool bOldDead = bIsDeadState;
	bIsDeadState = bNewDead;
	OnRep_Dead(bOldDead);
	ForceNetUpdate();

	if (bIsDeadState && bIsDowned)
	{
		SetDownedState(false);
	}
}

void AARCharacterStateRuntime::SetInvaderPlayerColor(EARAffinityColor NewColor)
{
	if (!HasAuthority())
	{
		return;
	}

	const EARAffinityColor SanitizedColor = (NewColor == EARAffinityColor::Unknown)
		? EARAffinityColor::None
		: NewColor;
	if (InvaderPlayerColor == SanitizedColor)
	{
		return;
	}

	const EARAffinityColor OldColor = InvaderPlayerColor;
	InvaderPlayerColor = SanitizedColor;
	OnRep_InvaderPlayerColor(OldColor);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::ResetInvaderCombo()
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

void AARCharacterStateRuntime::ReportInvaderKillCredit(EARAffinityColor EnemyColor, float ServerTimeSeconds, float ComboTimeoutSeconds)
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

void AARCharacterStateRuntime::MarkInvaderUpgradeActivated(FGameplayTag UpgradeTag)
{
	if (!HasAuthority() || !UpgradeTag.IsValid() || ActivatedInvaderUpgradeTags.HasTagExact(UpgradeTag))
	{
		return;
	}

	const FGameplayTagContainer OldTags = ActivatedInvaderUpgradeTags;
	ActivatedInvaderUpgradeTags.AddTag(UpgradeTag);
	OnRep_ActivatedInvaderUpgrades(OldTags);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::ClearActivatedInvaderUpgrades()
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

bool AARCharacterStateRuntime::HasActivatedInvaderUpgrade(FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() && ActivatedInvaderUpgradeTags.HasTagExact(UpgradeTag);
}

void AARCharacterStateRuntime::SetSpiceSharingActive(bool bNewIsSharing)
{
	if (!HasAuthority() || bIsSharingSpice == bNewIsSharing)
	{
		return;
	}

	const bool bOldIsSharing = bIsSharingSpice;
	bIsSharingSpice = bNewIsSharing;
	OnRep_IsSharingSpice(bOldIsSharing);
	ForceNetUpdate();
}

int32 AARCharacterStateRuntime::ClampSpicyTrackCursorTier(int32 RequestedCursorTier) const
{
	const UARInvaderSpicyTrackSettings* Settings = GetDefault<UARInvaderSpicyTrackSettings>();
	const int32 MaxTier = Settings ? FMath::Max(0, Settings->MaxFullBlastTier) : 0;
	return FMath::Clamp(RequestedCursorTier, 0, MaxTier);
}

void AARCharacterStateRuntime::SetSpicyTrackCursorTier(int32 NewCursorTier)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ClampedCursor = ClampSpicyTrackCursorTier(NewCursorTier);
	if (SpicyTrackCursorTier == ClampedCursor)
	{
		return;
	}

	const int32 OldCursor = SpicyTrackCursorTier;
	SpicyTrackCursorTier = ClampedCursor;
	OnRep_SpicyTrackCursorTier(OldCursor);
	ForceNetUpdate();
}

void AARCharacterStateRuntime::AdjustSpicyTrackCursorTier(int32 DeltaTier)
{
	if (!HasAuthority() || DeltaTier == 0)
	{
		return;
	}

	SetSpicyTrackCursorTier(SpicyTrackCursorTier + DeltaTier);
}

void AARCharacterStateRuntime::WriteSaveData(FARCharacterSaveData& InOutSaveData) const
{
	InOutSaveData.CharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	InOutSaveData.CharacterProgressionTags = CharacterProgressionTags;
	InOutSaveData.LoadoutTags = LoadoutTags;
	InOutSaveData.bIsDowned = bIsDowned;
	InOutSaveData.bIsDeadState = bIsDeadState;
	InOutSaveData.InvaderRuntime.PlayerColor = InvaderPlayerColor;
	InOutSaveData.InvaderRuntime.ComboCount = FMath::Max(0, InvaderComboCount);
	InOutSaveData.InvaderRuntime.ActivatedUpgradeTags = ActivatedInvaderUpgradeTags;
	InOutSaveData.InvaderRuntime.bIsSharingSpice = bIsSharingSpice;
	InOutSaveData.InvaderRuntime.SpicyTrackCursorTier = FMath::Max(0, SpicyTrackCursorTier);

	const FARCharacterRuntimeCoreAttributeSnapshot AttributeSnapshot = GetCoreAttributeSnapshot();
	const FARCharacterRuntimePlayerAttributeSnapshot PlayerSnapshot = GetPlayerAttributeSnapshot();
	InOutSaveData.CoreAttributes.Health = AttributeSnapshot.Health;
	InOutSaveData.CoreAttributes.MaxHealth = AttributeSnapshot.MaxHealth;
	InOutSaveData.CoreAttributes.Spice = PlayerSnapshot.Spice;
	InOutSaveData.CoreAttributes.MaxSpice = PlayerSnapshot.MaxSpice;
	InOutSaveData.CoreAttributes.MoveSpeed = AttributeSnapshot.MoveSpeed;
	InOutSaveData.CoreAttributes.Strength = PlayerSnapshot.Strength;
}

void AARCharacterStateRuntime::ApplySaveData(const FARCharacterSaveData& InSaveData)
{
	if (!HasAuthority())
	{
		return;
	}

	SetCharacterTag(InSaveData.CharacterTag);
	CharacterProgressionTags = InSaveData.CharacterProgressionTags;
	SetLoadoutTags(InSaveData.LoadoutTags);

	if (AbilitySystemComponent)
	{
		const float MaxHealth = FMath::Max(0.0f, InSaveData.CoreAttributes.MaxHealth);
		const float MaxSpice = FMath::Max(0.0f, InSaveData.CoreAttributes.MaxSpice);
		AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetMaxHealthAttribute(), MaxHealth);
		AbilitySystemComponent->SetNumericAttributeBase(
			UARAttributeSetCore::GetHealthAttribute(),
			FMath::Clamp(InSaveData.CoreAttributes.Health, 0.0f, MaxHealth));
		AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetPlayer::GetMaxSpiceAttribute(), MaxSpice);
		AbilitySystemComponent->SetNumericAttributeBase(
			UARAttributeSetPlayer::GetSpiceAttribute(),
			FMath::Clamp(InSaveData.CoreAttributes.Spice, 0.0f, MaxSpice));
		AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetCore::GetMoveSpeedAttribute(), FMath::Max(0.0f, InSaveData.CoreAttributes.MoveSpeed));
		AbilitySystemComponent->SetNumericAttributeBase(UARAttributeSetPlayer::GetStrengthAttribute(), FMath::Max(0.0f, InSaveData.CoreAttributes.Strength));
	}

	SetDeadState(InSaveData.bIsDeadState);
	SetDownedState(InSaveData.bIsDowned);
	SetInvaderPlayerColor(InSaveData.InvaderRuntime.PlayerColor);

	const int32 OldComboCount = InvaderComboCount;
	InvaderComboCount = FMath::Max(0, InSaveData.InvaderRuntime.ComboCount);
	LastInvaderKillCreditServerTime = -1.0f;
	if (InvaderComboCount != OldComboCount)
	{
		OnRep_InvaderComboCount(OldComboCount);
	}

	const FGameplayTagContainer OldActivatedTags = ActivatedInvaderUpgradeTags;
	ActivatedInvaderUpgradeTags = InSaveData.InvaderRuntime.ActivatedUpgradeTags;
	if (!(ActivatedInvaderUpgradeTags == OldActivatedTags))
	{
		OnRep_ActivatedInvaderUpgrades(OldActivatedTags);
	}

	SetSpiceSharingActive(InSaveData.InvaderRuntime.bIsSharingSpice);
	SetSpicyTrackCursorTier(InSaveData.InvaderRuntime.SpicyTrackCursorTier);
}

void AARCharacterStateRuntime::OnRep_CharacterTag(FGameplayTag OldCharacterTag)
{
	(void)OldCharacterTag;

	if (UWorld* World = GetWorld())
	{
		if (UARCharacterSubsystem* CharacterSubsystem = World->GetSubsystem<UARCharacterSubsystem>())
		{
			CharacterSubsystem->RegisterRuntime(this);
		}
	}
}

void AARCharacterStateRuntime::OnRep_OwningPlayerState(AARPlayerStateBase* OldOwningPlayerState)
{
	(void)OldOwningPlayerState;
	RefreshAbilityActorInfo();
}

void AARCharacterStateRuntime::OnRep_CurrentPawn(APawn* OldPawn)
{
	RefreshAbilityActorInfo();
	OnCurrentPawnChanged.Broadcast(this, CurrentPawn, OldPawn);
}

void AARCharacterStateRuntime::OnRep_LoadoutTags(const FGameplayTagContainer& OldLoadoutTags)
{
	OnLoadoutChanged.Broadcast(this, LoadoutTags, OldLoadoutTags);
}

void AARCharacterStateRuntime::OnRep_Downed(bool bOldDowned)
{
	OnDownedStateChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), bIsDowned, bOldDowned);
}

void AARCharacterStateRuntime::OnRep_Dead(bool bOldDead)
{
	OnDeadStateChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), bIsDeadState, bOldDead);
}

void AARCharacterStateRuntime::OnRep_InvaderPlayerColor(EARAffinityColor OldColor)
{
	OnInvaderPlayerColorChanged.Broadcast(InvaderPlayerColor, OldColor);
}

void AARCharacterStateRuntime::OnRep_InvaderComboCount(int32 OldComboCount)
{
	OnInvaderComboChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), InvaderComboCount, OldComboCount);
}

void AARCharacterStateRuntime::OnRep_ActivatedInvaderUpgrades(const FGameplayTagContainer& OldActivatedTags)
{
	OnActivatedInvaderUpgradesChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), ActivatedInvaderUpgradeTags, OldActivatedTags);
}

void AARCharacterStateRuntime::OnRep_IsSharingSpice(bool bOldIsSharingSpice)
{
	OnSpiceSharingStateChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), bIsSharingSpice, bOldIsSharingSpice);
}

void AARCharacterStateRuntime::OnRep_SpicyTrackCursorTier(int32 OldCursorTier)
{
	OnSpicyTrackCursorChanged.Broadcast(this, ARPlayer::NormalizeCharacterTag(CharacterTag), SpicyTrackCursorTier, OldCursorTier);
}

void AARCharacterStateRuntime::RefreshAbilityActorInfo()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AActor* AvatarActor = CurrentPawn ? Cast<AActor>(CurrentPawn) : Cast<AActor>(this);
	AbilitySystemComponent->InitAbilityActorInfo(this, AvatarActor);
}

bool AARCharacterStateRuntime::DoesInvaderColorMatch(EARAffinityColor PlayerColor, EARAffinityColor EnemyColor)
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
