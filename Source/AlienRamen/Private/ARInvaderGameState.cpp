#include "ARInvaderGameState.h"

#include "ARAttributeSetCore.h"
#include "AREnemyBase.h"
#include "ARGameStateModeStructs.h"
#include "ARInvaderDropBase.h"
#include "ARInvaderDirectorSettings.h"
#include "ARInvaderSpicyTrackSettings.h"
#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARProjectileBase.h"
#include "AbilitySystemComponent.h"
#include "Curves/CurveFloat.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AARInvaderGameState::AARInvaderGameState()
{
	ClassStateStruct = FARInvaderGameStateData::StaticStruct();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
}

UScriptStruct* AARInvaderGameState::GetStateStruct_Implementation() const
{
	return FARInvaderGameStateData::StaticStruct();
}

void AARInvaderGameState::BeginPlay()
{
	Super::BeginPlay();

	OfferRng.Initialize(FMath::Rand());
	if (HasAuthority())
	{
		RegisterDebugConsoleCommands();
		InitializeSpicyTrackState();
		OnTrackedPlayersChanged.AddUniqueDynamic(this, &AARInvaderGameState::HandleTrackedPlayersChanged);
		HandleTrackedPlayersChanged();
	}
}

void AARInvaderGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnTrackedPlayersChanged.RemoveDynamic(this, &AARInvaderGameState::HandleTrackedPlayersChanged);
	ClearWhileSlottedEffects();
	ActiveSpiceSharers.Reset();
	UnregisterDebugConsoleCommands();
	Super::EndPlay(EndPlayReason);
}

void AARInvaderGameState::RegisterDebugConsoleCommands()
{
	if (!HasAuthority())
	{
		return;
	}

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	UnregisterDebugConsoleCommands();

	CmdDebugSetSpice = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.SetSpice"),
		TEXT("Usage: AR.Invader.Debug.SetSpice <p1|p2> <value>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleSetSpice),
		ECVF_Cheat);

	CmdDebugAddSpice = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.AddSpice"),
		TEXT("Usage: AR.Invader.Debug.AddSpice <p1|p2> <delta>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleAddSpice),
		ECVF_Cheat);

	CmdDebugAddScrap = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.AddScrap"),
		TEXT("Usage: AR.Invader.Debug.AddScrap <delta>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleAddScrap),
		ECVF_Cheat);

	CmdDebugAddMoney = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.AddMoney"),
		TEXT("Usage: AR.Invader.Debug.AddMoney <delta>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleAddMoney),
		ECVF_Cheat);

	CmdDebugAddMeat = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.AddMeat"),
		TEXT("Usage: AR.Invader.Debug.AddMeat <delta> [red|blue|white|unspecified]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleAddMeat),
		ECVF_Cheat);

	CmdDebugSetDropEarthGravity = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.SetDropEarthGravity"),
		TEXT("Usage: AR.Invader.Debug.SetDropEarthGravity <0|1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleSetDropEarthGravity),
		ECVF_Cheat);

	CmdDebugSetCursor = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.SetCursor"),
		TEXT("Usage: AR.Invader.Debug.SetCursor <p1|p2> <tier>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleSetCursor),
		ECVF_Cheat);

	CmdDebugInjectTopSlot = ConsoleManager.RegisterConsoleCommand(
		TEXT("AR.Invader.Debug.InjectTopSlot"),
		TEXT("Usage: AR.Invader.Debug.InjectTopSlot [UpgradeTag] [Level] [Uses|-1 for infinite]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &AARInvaderGameState::HandleConsoleInjectTopSlot),
		ECVF_Cheat);
}

void AARInvaderGameState::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	if (CmdDebugSetSpice)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugSetSpice, false);
		CmdDebugSetSpice = nullptr;
	}
	if (CmdDebugAddSpice)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugAddSpice, false);
		CmdDebugAddSpice = nullptr;
	}
	if (CmdDebugAddScrap)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugAddScrap, false);
		CmdDebugAddScrap = nullptr;
	}
	if (CmdDebugAddMoney)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugAddMoney, false);
		CmdDebugAddMoney = nullptr;
	}
	if (CmdDebugAddMeat)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugAddMeat, false);
		CmdDebugAddMeat = nullptr;
	}
	if (CmdDebugSetDropEarthGravity)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugSetDropEarthGravity, false);
		CmdDebugSetDropEarthGravity = nullptr;
	}
	if (CmdDebugSetCursor)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugSetCursor, false);
		CmdDebugSetCursor = nullptr;
	}
	if (CmdDebugInjectTopSlot)
	{
		ConsoleManager.UnregisterConsoleObject(CmdDebugInjectTopSlot, false);
		CmdDebugInjectTopSlot = nullptr;
	}
}

AARPlayerStateBase* AARInvaderGameState::ResolvePlayerStateFromDebugToken(const FString& Token) const
{
	const FString Normalized = Token.TrimStartAndEnd().ToLower();
	if (Normalized.IsEmpty())
	{
		return nullptr;
	}

	EARPlayerSlot DesiredSlot = EARPlayerSlot::Unknown;
	if (Normalized == TEXT("p1") || Normalized == TEXT("1"))
	{
		DesiredSlot = EARPlayerSlot::P1;
	}
	else if (Normalized == TEXT("p2") || Normalized == TEXT("2"))
	{
		DesiredSlot = EARPlayerSlot::P2;
	}

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (!PlayerState)
		{
			continue;
		}

		if (DesiredSlot != EARPlayerSlot::Unknown && PlayerState->GetPlayerSlot() == DesiredSlot)
		{
			return PlayerState;
		}
	}

	return nullptr;
}

bool AARInvaderGameState::ResolveUpgradeTagForDebugInject(const FString& TagToken, FGameplayTag& OutUpgradeTag) const
{
	OutUpgradeTag = FGameplayTag();

	const FString Trimmed = TagToken.TrimStartAndEnd();
	if (!Trimmed.IsEmpty())
	{
		OutUpgradeTag = FGameplayTag::RequestGameplayTag(FName(*Trimmed), false);
		return OutUpgradeTag.IsValid();
	}

	TMap<FGameplayTag, FARInvaderUpgradeDefRow> UpgradeDefinitions;
	if (!BuildUpgradeDefinitionMap(UpgradeDefinitions) || UpgradeDefinitions.IsEmpty())
	{
		return false;
	}

	TArray<FGameplayTag> UpgradeTags;
	UpgradeDefinitions.GetKeys(UpgradeTags);
	UpgradeTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
		{
			return A.ToString() < B.ToString();
		});
	OutUpgradeTag = UpgradeTags[0];
	return OutUpgradeTag.IsValid();
}

void AARInvaderGameState::HandleConsoleSetSpice(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] Usage: AR.Invader.Debug.SetSpice [p1|p2] <value>"));
		return;
	}

	const bool bHasExplicitPlayer = Args.Num() >= 2;
	const FString PlayerToken = bHasExplicitPlayer ? Args[0] : TEXT("p1");
	const FString ValueToken = bHasExplicitPlayer ? Args[1] : Args[0];

	AARPlayerStateBase* PlayerState = ResolvePlayerStateFromDebugToken(PlayerToken);
	if (!PlayerState)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] SetSpice failed: could not resolve player token '%s'."), *PlayerToken);
		return;
	}

	const float Value = FCString::Atof(*ValueToken);
	PlayerState->SetSpiceMeter(Value);
	UE_LOG(ARLog, Log, TEXT("[InvaderSpice|Debug] SetSpice '%s' -> %.2f"), *GetNameSafe(PlayerState), Value);
}

void AARInvaderGameState::HandleConsoleAddSpice(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] Usage: AR.Invader.Debug.AddSpice [p1|p2] <delta>"));
		return;
	}

	const bool bHasExplicitPlayer = Args.Num() >= 2;
	const FString PlayerToken = bHasExplicitPlayer ? Args[0] : TEXT("p1");
	const FString DeltaToken = bHasExplicitPlayer ? Args[1] : Args[0];

	AARPlayerStateBase* PlayerState = ResolvePlayerStateFromDebugToken(PlayerToken);
	if (!PlayerState)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] AddSpice failed: could not resolve player token '%s'."), *PlayerToken);
		return;
	}

	const float Delta = FCString::Atof(*DeltaToken);
	const float Current = PlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
	PlayerState->SetSpiceMeter(Current + Delta);
	UE_LOG(ARLog, Log, TEXT("[InvaderSpice|Debug] AddSpice '%s' %+0.2f -> %.2f"), *GetNameSafe(PlayerState), Delta, PlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice));
}

void AARInvaderGameState::HandleConsoleAddScrap(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSave|Debug] Usage: AR.Invader.Debug.AddScrap <delta>"));
		return;
	}

	const int32 Delta = FCString::Atoi(*Args[0]);
	const int32 OldScrap = GetScrap();
	const int32 NewScrap = OldScrap + Delta;
	SetScrapFromSave(NewScrap);

	UE_LOG(ARLog, Log, TEXT("[InvaderSave|Debug] AddScrap %+d -> %d"), Delta, GetScrap());
}

void AARInvaderGameState::HandleConsoleAddMoney(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSave|Debug] Usage: AR.Invader.Debug.AddMoney <delta>"));
		return;
	}

	const int32 Delta = FCString::Atoi(*Args[0]);
	const int32 OldMoney = GetMoney();
	const int32 NewMoney = OldMoney + Delta;
	SetMoneyFromSave(NewMoney);

	UE_LOG(ARLog, Log, TEXT("[InvaderSave|Debug] AddMoney %+d -> %d"), Delta, GetMoney());
}

void AARInvaderGameState::HandleConsoleAddMeat(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSave|Debug] Usage: AR.Invader.Debug.AddMeat <delta> [red|blue|white|unspecified]"));
		return;
	}

	const int32 Delta = FCString::Atoi(*Args[0]);
	EARAffinityColor ColorBucket = EARAffinityColor::None;
	bool bUseUnspecifiedBucket = true;
	if (Args.Num() > 1)
	{
		const FString ColorToken = Args[1].TrimStartAndEnd().ToLower();
		if (ColorToken == TEXT("red"))
		{
			ColorBucket = EARAffinityColor::Red;
			bUseUnspecifiedBucket = false;
		}
		else if (ColorToken == TEXT("blue"))
		{
			ColorBucket = EARAffinityColor::Blue;
			bUseUnspecifiedBucket = false;
		}
		else if (ColorToken == TEXT("white"))
		{
			ColorBucket = EARAffinityColor::White;
			bUseUnspecifiedBucket = false;
		}
		else if (ColorToken == TEXT("unspecified") || ColorToken == TEXT("none"))
		{
			bUseUnspecifiedBucket = true;
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[InvaderSave|Debug] Invalid meat color '%s'. Expected red|blue|white|unspecified."),
				*Args[1]);
			return;
		}
	}

	FARMeatState MeatState = GetMeat();
	if (bUseUnspecifiedBucket)
	{
		MeatState.UnspecifiedAmount += Delta;
	}
	else
	{
		switch (ColorBucket)
		{
		case EARAffinityColor::Red:
			MeatState.RedAmount += Delta;
			break;
		case EARAffinityColor::Blue:
			MeatState.BlueAmount += Delta;
			break;
		case EARAffinityColor::White:
			MeatState.WhiteAmount += Delta;
			break;
		default:
			MeatState.UnspecifiedAmount += Delta;
			break;
		}
	}

	SetMeatFromSave(MeatState);
	const TCHAR* BucketLabel = bUseUnspecifiedBucket ? TEXT("unspecified") : *Args[1];
	UE_LOG(ARLog, Log, TEXT("[InvaderSave|Debug] AddMeat bucket=%s %+d -> total=%d"), BucketLabel, Delta, GetMeat().GetTotalAmount());
}

void AARInvaderGameState::HandleConsoleSetDropEarthGravity(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderDrop|Debug] Usage: AR.Invader.Debug.SetDropEarthGravity <0|1>"));
		return;
	}

	const int32 RawValue = FCString::Atoi(*Args[0]);
	const bool bEnabled = RawValue != 0;
	bDebugDropEarthGravityEnabled = bEnabled;
	SetDropEarthGravityEnabledForAll(bEnabled);
	UE_LOG(ARLog, Log, TEXT("[InvaderDrop|Debug] SetDropEarthGravity -> %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void AARInvaderGameState::HandleConsoleSetCursor(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority() || Args.Num() < 1)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] Usage: AR.Invader.Debug.SetCursor [p1|p2] <tier>"));
		return;
	}

	const bool bHasExplicitPlayer = Args.Num() >= 2;
	const FString PlayerToken = bHasExplicitPlayer ? Args[0] : TEXT("p1");
	const FString TierToken = bHasExplicitPlayer ? Args[1] : Args[0];

	AARPlayerStateBase* PlayerState = ResolvePlayerStateFromDebugToken(PlayerToken);
	if (!PlayerState)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] SetCursor failed: could not resolve player token '%s'."), *PlayerToken);
		return;
	}

	const int32 RequestedTier = FCString::Atoi(*TierToken);
	PlayerState->SetSpicyTrackCursorTier(RequestedTier);
	UE_LOG(ARLog, Log, TEXT("[InvaderSpice|Debug] SetCursor '%s' -> %d"), *GetNameSafe(PlayerState), PlayerState->GetSpicyTrackCursorTier());
}

void AARInvaderGameState::HandleConsoleInjectTopSlot(const TArray<FString>& Args, UWorld* /*World*/)
{
	if (!HasAuthority())
	{
		return;
	}

	FGameplayTag UpgradeTag;
	const FString TagToken = Args.Num() > 0 ? Args[0] : FString();
	if (!ResolveUpgradeTagForDebugInject(TagToken, UpgradeTag))
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] InjectTopSlot failed: no valid upgrade tag resolved. Provide tag or ensure UpgradeDataTable has rows."));
		return;
	}

	const int32 Level = Args.Num() > 1 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1;
	const int32 UsesArg = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 1;
	const bool bInfiniteUses = UsesArg < 0;
	const int32 RemainingUses = bInfiniteUses ? INDEX_NONE : FMath::Max(1, UsesArg);

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 MaxTrackSlots = Settings ? FMath::Clamp(Settings->MaxFullBlastTier - 1, 0, 4) : 4;
	if (MaxTrackSlots <= 0)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice|Debug] InjectTopSlot failed: MaxTrackSlots resolved to 0."));
		return;
	}

	const int32 TargetSlotIndex = FMath::Clamp(FMath::Max(1, SharedFullBlastTier - 1), 1, MaxTrackSlots);
	const TArray<FARInvaderTrackSlotState> OldSlots = SharedTrackSlots;
	const int32 OldTier = SharedFullBlastTier;

	EnsureTrackSlotCount(TargetSlotIndex);
	FARInvaderTrackSlotState& Slot = SharedTrackSlots[TargetSlotIndex - 1];
	Slot.SlotIndex = TargetSlotIndex;
	Slot.UpgradeTag = UpgradeTag;
	Slot.UpgradeLevel = Level;
	Slot.bHasBeenActivated = false;
	Slot.bInfiniteUses = bInfiniteUses;
	Slot.RemainingActivationUses = RemainingUses;

	SharedFullBlastTier = FMath::Max(SharedFullBlastTier, TargetSlotIndex + 1);
	if (Settings)
	{
		SharedFullBlastTier = FMath::Clamp(SharedFullBlastTier, 1, Settings->MaxFullBlastTier);
	}

	TrimTrackToTierLimit();
	SyncSharedMaxSpiceToPlayers();
	RefreshWhileSlottedEffects();
	ReconcilePlayerCursorSelection();

	OnRep_SharedTrackSlots(OldSlots);
	if (OldTier != SharedFullBlastTier)
	{
		OnRep_SharedFullBlastTier(OldTier);
	}
	ForceNetUpdate();

	UE_LOG(
		ARLog,
		Log,
		TEXT("[InvaderSpice|Debug] InjectTopSlot slot=%d tag=%s level=%d uses=%s"),
		TargetSlotIndex,
		*UpgradeTag.ToString(),
		Level,
		bInfiniteUses ? TEXT("INF") : *LexToString(RemainingUses));
}

void AARInvaderGameState::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	const float ServerTime = GetServerWorldTimeSeconds();
	TickComboTimeouts(ServerTime);
	TickShareTransfers(DeltaSeconds);
}

void AARInvaderGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARInvaderGameState, SharedTrackSlots);
	DOREPLIFETIME(AARInvaderGameState, SharedFullBlastTier);
	DOREPLIFETIME(AARInvaderGameState, FullBlastSession);
	DOREPLIFETIME(AARInvaderGameState, OfferPresenceStates);
}

void AARInvaderGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	if (HasAuthority() && Cast<AARPlayerStateBase>(PlayerState))
	{
		SyncSharedMaxSpiceToPlayers();
		RefreshWhileSlottedEffects();
	}
}

void AARInvaderGameState::RemovePlayerState(APlayerState* PlayerState)
{
	if (HasAuthority())
	{
		if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
		{
			ActiveSpiceSharers.Remove(ARPlayerState);
			ClearWhileSlottedEffectsForPlayer(ARPlayerState);
			ClearOfferPresence(ARPlayerState);
		}
	}

	Super::RemovePlayerState(PlayerState);

	if (HasAuthority() && Cast<AARPlayerStateBase>(PlayerState))
	{
		SyncSharedMaxSpiceToPlayers();
	}
}

void AARInvaderGameState::OnRep_SharedTrackSlots(const TArray<FARInvaderTrackSlotState>& /*OldSlots*/)
{
	OnInvaderSharedTrackChanged.Broadcast();
}

void AARInvaderGameState::OnRep_SharedFullBlastTier(const int32 /*OldTier*/)
{
	OnInvaderSharedTrackChanged.Broadcast();
}

void AARInvaderGameState::OnRep_FullBlastSession(const FARInvaderFullBlastSessionState& /*OldSession*/)
{
	OnInvaderFullBlastSessionChanged.Broadcast(FullBlastSession.bIsActive);
}

void AARInvaderGameState::OnRep_OfferPresenceStates(const TArray<FARInvaderOfferPresenceState>& /*OldPresenceStates*/)
{
	OnInvaderOfferPresenceChanged.Broadcast();
}

const UARInvaderSpicyTrackSettings* AARInvaderGameState::GetSpicyTrackSettings() const
{
	return GetDefault<UARInvaderSpicyTrackSettings>();
}

int32 AARInvaderGameState::GetSharedMaxSpice() const
{
	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 SpicePerTier = Settings ? FMath::Max(1, Settings->SpicePerTier) : 100;
	return FMath::Max(1, SharedFullBlastTier) * SpicePerTier;
}

int32 AARInvaderGameState::GetMaxSelectableTrackCursorTierForPlayer(const AARPlayerStateBase* PlayerState) const
{
	if (!PlayerState)
	{
		return 0;
	}

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 SpicePerTier = Settings ? FMath::Max(1, Settings->SpicePerTier) : 100;
	const int32 MaxTrackSlots = Settings ? FMath::Clamp(Settings->MaxFullBlastTier - 1, 0, 4) : 4;
	const int32 MaxUnlockedTier = FMath::Clamp(SharedFullBlastTier - 1, 0, MaxTrackSlots);

	const float CurrentSpice = FMath::Max(0.0f, PlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice));
	const int32 AffordableTier = FMath::Max(0, FMath::FloorToInt(CurrentSpice / static_cast<float>(SpicePerTier)));
	const int32 HighestCandidateTier = FMath::Min(MaxUnlockedTier, AffordableTier);
	if (HighestCandidateTier <= 0)
	{
		return 0;
	}

	// Prefer highest currently slotted/valid tier so activation is always meaningful.
	for (int32 Tier = HighestCandidateTier; Tier >= 1; --Tier)
	{
		const int32 SlotIndex = Tier - 1;
		if (!SharedTrackSlots.IsValidIndex(SlotIndex))
		{
			continue;
		}

		const FARInvaderTrackSlotState& SlotState = SharedTrackSlots[SlotIndex];
		const bool bHasUsableFiniteCharges = SlotState.RemainingActivationUses > 0;
		if (SlotState.UpgradeTag.IsValid() && (SlotState.bInfiniteUses || bHasUsableFiniteCharges))
		{
			return Tier;
		}
	}

	return 0;
}

void AARInvaderGameState::InitializeSpicyTrackState()
{
	if (!HasAuthority())
	{
		return;
	}

	const TArray<FARInvaderTrackSlotState> OldSlots = SharedTrackSlots;
	const int32 OldTier = SharedFullBlastTier;
	const FARInvaderFullBlastSessionState OldSession = FullBlastSession;
	const TArray<FARInvaderOfferPresenceState> OldPresenceStates = OfferPresenceStates;

	SharedTrackSlots.Reset();
	SharedFullBlastTier = 1;
	FullBlastSession = FARInvaderFullBlastSessionState();
	OfferPresenceStates.Reset();
	ActiveSpiceSharers.Reset();

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (!PlayerState)
		{
			continue;
		}

		PlayerState->ResetInvaderCombo();
		PlayerState->ClearActivatedInvaderUpgrades();
		PlayerState->ResetSpicyTrackCursor();
	}

	SyncSharedMaxSpiceToPlayers();
	RefreshWhileSlottedEffects();
	OnRep_SharedTrackSlots(OldSlots);
	OnRep_SharedFullBlastTier(OldTier);
	OnRep_FullBlastSession(OldSession);
	OnRep_OfferPresenceStates(OldPresenceStates);
	ForceNetUpdate();
}

void AARInvaderGameState::EnsureTrackSlotCount(const int32 RequiredSlots)
{
	const int32 TargetSlots = FMath::Max(0, RequiredSlots);
	while (SharedTrackSlots.Num() < TargetSlots)
	{
		FARInvaderTrackSlotState NewSlot;
		NewSlot.SlotIndex = SharedTrackSlots.Num() + 1;
		SharedTrackSlots.Add(NewSlot);
	}

	NormalizeTrackSlotIndices();
}

void AARInvaderGameState::NormalizeTrackSlotIndices()
{
	for (int32 Index = 0; Index < SharedTrackSlots.Num(); ++Index)
	{
		SharedTrackSlots[Index].SlotIndex = Index + 1;
	}
}

void AARInvaderGameState::TrimTrackToTierLimit()
{
	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 MaxTrackSlots = Settings ? FMath::Clamp(Settings->MaxFullBlastTier - 1, 0, 4) : 4;
	const int32 AllowedSlotsByTier = FMath::Clamp(SharedFullBlastTier - 1, 0, MaxTrackSlots);
	if (SharedTrackSlots.Num() > AllowedSlotsByTier)
	{
		SharedTrackSlots.SetNum(AllowedSlotsByTier, EAllowShrinking::No);
	}

	NormalizeTrackSlotIndices();
}

void AARInvaderGameState::SyncSharedMaxSpiceToPlayers()
{
	if (!HasAuthority())
	{
		return;
	}

	const float SharedMaxSpice = static_cast<float>(GetSharedMaxSpice());
	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (!PlayerState)
		{
			continue;
		}

		if (UAbilitySystemComponent* ASC = PlayerState->GetASC())
		{
			ASC->SetNumericAttributeBase(UARAttributeSetCore::GetMaxSpiceAttribute(), SharedMaxSpice);
		}

		PlayerState->SetSpiceMeter(PlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice));
	}
}

void AARInvaderGameState::ResetAllPlayerSpiceMeters()
{
	if (!HasAuthority())
	{
		return;
	}

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (PlayerState)
		{
			PlayerState->ClearSpiceMeter();
		}
	}
}

void AARInvaderGameState::HandleTrackedPlayersChanged()
{
	if (!HasAuthority())
	{
		return;
	}

	SyncSharedMaxSpiceToPlayers();
	RefreshWhileSlottedEffects();
	ReconcilePlayerCursorSelection();
}

void AARInvaderGameState::ReconcilePlayerCursorSelection()
{
	if (!HasAuthority())
	{
		return;
	}

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (PlayerState)
		{
			PlayerState->SetSpicyTrackCursorTier(PlayerState->GetSpicyTrackCursorTier());
		}
	}
}

bool AARInvaderGameState::BuildUpgradeDefinitionMap(TMap<FGameplayTag, FARInvaderUpgradeDefRow>& OutDefinitions) const
{
	OutDefinitions.Reset();

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	if (!Settings)
	{
		return false;
	}

	UDataTable* UpgradeTable = Settings->UpgradeDataTable.LoadSynchronous();
	if (!UpgradeTable)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] UpgradeDataTable is not configured."));
		return false;
	}

	TArray<FARInvaderUpgradeDefRow*> Rows;
	UpgradeTable->GetAllRows(TEXT("AARInvaderGameState::BuildUpgradeDefinitionMap"), Rows);
	for (const FARInvaderUpgradeDefRow* Row : Rows)
	{
		if (!Row || !Row->UpgradeTag.IsValid())
		{
			continue;
		}

		OutDefinitions.FindOrAdd(Row->UpgradeTag) = *Row;
	}

	return OutDefinitions.Num() > 0;
}

void AARInvaderGameState::BuildTeamActivationState(FGameplayTagContainer& OutTeamActivatedTags, TMap<FGameplayTag, int32>& OutTeamActivationCounts) const
{
	OutTeamActivatedTags.Reset();
	OutTeamActivationCounts.Reset();

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (!PlayerState)
		{
			continue;
		}

		TArray<FGameplayTag> ActivatedTags;
		PlayerState->GetActivatedInvaderUpgrades().GetGameplayTagArray(ActivatedTags);
		for (const FGameplayTag& Tag : ActivatedTags)
		{
			if (!Tag.IsValid())
			{
				continue;
			}

			OutTeamActivatedTags.AddTag(Tag);
			OutTeamActivationCounts.FindOrAdd(Tag) += 1;
		}
	}
}

int32 AARInvaderGameState::GetTeamActivationCount(const TMap<FGameplayTag, int32>& TeamActivationCounts, const FGameplayTag& UpgradeTag) const
{
	if (const int32* Count = TeamActivationCounts.Find(UpgradeTag))
	{
		return *Count;
	}

	return 0;
}

bool AARInvaderGameState::IsUpgradeEligibleForOffer(
	const FARInvaderUpgradeDefRow& UpgradeDef,
	const int32 ActivationTier,
	const FGameplayTagContainer& TeamActivatedTags,
	const TMap<FGameplayTag, int32>& TeamActivationCounts,
	const FGameplayTagContainer& SlottedUpgradeTags) const
{
	if (!UpgradeDef.UpgradeTag.IsValid())
	{
		return false;
	}

	if (SlottedUpgradeTags.HasTagExact(UpgradeDef.UpgradeTag))
	{
		return false;
	}

	if (UpgradeDef.LockedOfferTiers.Contains(ActivationTier))
	{
		return false;
	}

	if (!UpgradeDef.RequiredUnlockTags.IsEmpty() && !GetUnlocks().HasAll(UpgradeDef.RequiredUnlockTags))
	{
		return false;
	}

	if (!UpgradeDef.RequiredActivatedUpgradesForOffer.IsEmpty() && !TeamActivatedTags.HasAll(UpgradeDef.RequiredActivatedUpgradesForOffer))
	{
		return false;
	}

	switch (UpgradeDef.ClaimPolicy)
	{
	case EARInvaderUpgradeClaimPolicy::SingleTeamClaim:
		if (TeamActivatedTags.HasTagExact(UpgradeDef.UpgradeTag))
		{
			return false;
		}
		break;
	case EARInvaderUpgradeClaimPolicy::PerPlayerClaim:
		if (GetTeamActivationCount(TeamActivationCounts, UpgradeDef.UpgradeTag) >= 2)
		{
			return false;
		}
		break;
	case EARInvaderUpgradeClaimPolicy::Repeatable:
	default:
		break;
	}

	return true;
}

int32 AARInvaderGameState::RollOfferLevelFromTier(const int32 BaseTier)
{
	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	if (!Settings || Settings->LevelOffsetWeights.IsEmpty())
	{
		return FMath::Max(1, BaseTier);
	}

	float TotalWeight = 0.0f;
	for (const FARInvaderLevelOffsetWeight& Entry : Settings->LevelOffsetWeights)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return FMath::Max(1, BaseTier);
	}

	float Roll = OfferRng.FRandRange(0.0f, TotalWeight);
	int32 ChosenOffset = 0;
	for (const FARInvaderLevelOffsetWeight& Entry : Settings->LevelOffsetWeights)
	{
		const float Weight = FMath::Max(0.0f, Entry.Weight);
		if (Weight <= 0.0f)
		{
			continue;
		}

		if (Roll <= Weight)
		{
			ChosenOffset = Entry.Offset;
			break;
		}

		Roll -= Weight;
	}

	return FMath::Max(1, BaseTier + ChosenOffset);
}

bool AARInvaderGameState::RequestActivateFullBlast(AARPlayerStateBase* RequestingPlayerState)
{
	if (!HasAuthority() || !RequestingPlayerState || FullBlastSession.bIsActive)
	{
		return false;
	}

	const float RequiredSpice = static_cast<float>(GetSharedMaxSpice());
	const float CurrentSpice = RequestingPlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
	if (CurrentSpice + KINDA_SMALL_NUMBER < RequiredSpice)
	{
		return false;
	}

	TMap<FGameplayTag, FARInvaderUpgradeDefRow> UpgradeDefinitions;
	if (!BuildUpgradeDefinitionMap(UpgradeDefinitions))
	{
		return false;
	}

	FGameplayTagContainer TeamActivatedTags;
	TMap<FGameplayTag, int32> TeamActivationCounts;
	BuildTeamActivationState(TeamActivatedTags, TeamActivationCounts);

	FGameplayTagContainer SlottedUpgradeTags;
	for (const FARInvaderTrackSlotState& Slot : SharedTrackSlots)
	{
		if (Slot.UpgradeTag.IsValid())
		{
			SlottedUpgradeTags.AddTag(Slot.UpgradeTag);
		}
	}

	TArray<FGameplayTag> Candidates;
	Candidates.Reserve(UpgradeDefinitions.Num());
	for (const TPair<FGameplayTag, FARInvaderUpgradeDefRow>& Pair : UpgradeDefinitions)
	{
		if (IsUpgradeEligibleForOffer(Pair.Value, SharedFullBlastTier, TeamActivatedTags, TeamActivationCounts, SlottedUpgradeTags))
		{
			Candidates.Add(Pair.Key);
		}
	}

	if (Candidates.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] Full blast has no eligible upgrade candidates."));
		return false;
	}

	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = OfferRng.RandRange(0, Index);
		Candidates.Swap(Index, SwapIndex);
	}

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 OfferCount = Settings ? FMath::Clamp(Settings->FullBlastOfferCount, 1, Candidates.Num()) : FMath::Min(3, Candidates.Num());

	const FARInvaderFullBlastSessionState OldSession = FullBlastSession;
	const TArray<FARInvaderOfferPresenceState> OldPresenceStates = OfferPresenceStates;
	FullBlastSession = FARInvaderFullBlastSessionState();
	OfferPresenceStates.Reset();
	FullBlastSession.bIsActive = true;
	FullBlastSession.RequestingPlayerSlot = RequestingPlayerState->GetPlayerSlot();
	FullBlastSession.ActivationTier = SharedFullBlastTier;
	FullBlastSession.Offers.Reserve(OfferCount);
	for (int32 OfferIndex = 0; OfferIndex < OfferCount; ++OfferIndex)
	{
		FARInvaderUpgradeOffer Offer;
		Offer.UpgradeTag = Candidates[OfferIndex];
		Offer.OfferedLevel = RollOfferLevelFromTier(SharedFullBlastTier);
		FullBlastSession.Offers.Add(Offer);
	}

	OnRep_FullBlastSession(OldSession);
	OnRep_OfferPresenceStates(OldPresenceStates);
	ForceNetUpdate();
	UGameplayStatics::SetGamePaused(GetWorld(), true);
	return true;
}

bool AARInvaderGameState::ResolveFullBlastSelection(AARPlayerStateBase* RequestingPlayerState, const FGameplayTag SelectedUpgradeTag, const int32 DesiredDestinationSlot)
{
	if (!HasAuthority() || !RequestingPlayerState || !FullBlastSession.bIsActive || !SelectedUpgradeTag.IsValid())
	{
		return false;
	}

	if (FullBlastSession.RequestingPlayerSlot != EARPlayerSlot::Unknown
		&& RequestingPlayerState->GetPlayerSlot() != FullBlastSession.RequestingPlayerSlot)
	{
		return false;
	}

	const FARInvaderUpgradeOffer* SelectedOffer = FullBlastSession.Offers.FindByPredicate(
		[&SelectedUpgradeTag](const FARInvaderUpgradeOffer& Offer)
		{
			return Offer.UpgradeTag == SelectedUpgradeTag;
		});
	if (!SelectedOffer)
	{
		return false;
	}

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 MaxTrackSlots = Settings ? FMath::Clamp(Settings->MaxFullBlastTier - 1, 0, 4) : 4;
	const bool bAtTopTier = Settings && SharedFullBlastTier >= Settings->MaxFullBlastTier;
	const int32 DestinationSlot = bAtTopTier
		? DesiredDestinationSlot
		: FMath::Clamp(FullBlastSession.ActivationTier, 1, MaxTrackSlots);

	if (DestinationSlot <= 0 || DestinationSlot > MaxTrackSlots)
	{
		return false;
	}

	if (bAtTopTier && (DestinationSlot < 2 || DestinationSlot > 4))
	{
		return false;
	}

	TMap<FGameplayTag, FARInvaderUpgradeDefRow> UpgradeDefinitions;
	const FARInvaderUpgradeDefRow* SelectedOfferDef = nullptr;
	if (BuildUpgradeDefinitionMap(UpgradeDefinitions))
	{
		SelectedOfferDef = UpgradeDefinitions.Find(SelectedOffer->UpgradeTag);
	}

	const TArray<FARInvaderTrackSlotState> OldSlots = SharedTrackSlots;
	const int32 OldTier = SharedFullBlastTier;
	const FARInvaderFullBlastSessionState OldSession = FullBlastSession;
	const TArray<FARInvaderOfferPresenceState> OldPresenceStates = OfferPresenceStates;

	EnsureTrackSlotCount(DestinationSlot);
	FARInvaderTrackSlotState& Slot = SharedTrackSlots[DestinationSlot - 1];
	Slot.SlotIndex = DestinationSlot;
	Slot.UpgradeTag = SelectedOffer->UpgradeTag;
	Slot.UpgradeLevel = FMath::Max(1, SelectedOffer->OfferedLevel);
	Slot.bHasBeenActivated = false;
	Slot.bInfiniteUses = SelectedOfferDef ? SelectedOfferDef->bInfiniteActivationUses : false;
	const int32 AuthoredMaxUses = SelectedOfferDef ? FMath::Max(1, SelectedOfferDef->MaxActivationUses) : 1;
	Slot.RemainingActivationUses = Slot.bInfiniteUses ? INDEX_NONE : AuthoredMaxUses;

	if (Settings)
	{
		SharedFullBlastTier = FMath::Clamp(SharedFullBlastTier + 1, 1, Settings->MaxFullBlastTier);
	}
	else
	{
		SharedFullBlastTier = FMath::Max(1, SharedFullBlastTier + 1);
	}

	TrimTrackToTierLimit();
	FullBlastSession = FARInvaderFullBlastSessionState();
	OfferPresenceStates.Reset();

	ResetAllPlayerSpiceMeters();
	SyncSharedMaxSpiceToPlayers();
	RefreshWhileSlottedEffects();
	ReconcilePlayerCursorSelection();

	OnRep_SharedTrackSlots(OldSlots);
	OnRep_FullBlastSession(OldSession);
	OnRep_OfferPresenceStates(OldPresenceStates);
	if (OldTier != SharedFullBlastTier)
	{
		OnRep_SharedFullBlastTier(OldTier);
	}

	ResolveFullBlastCommonPostChoice(false, OldSession.RequestingPlayerSlot, OldSession.ActivationTier);
	ForceNetUpdate();
	return true;
}

bool AARInvaderGameState::ResolveFullBlastSkip(AARPlayerStateBase* RequestingPlayerState)
{
	if (!HasAuthority() || !RequestingPlayerState || !FullBlastSession.bIsActive)
	{
		return false;
	}

	if (FullBlastSession.RequestingPlayerSlot != EARPlayerSlot::Unknown
		&& RequestingPlayerState->GetPlayerSlot() != FullBlastSession.RequestingPlayerSlot)
	{
		return false;
	}

	const FARInvaderFullBlastSessionState OldSession = FullBlastSession;
	const TArray<FARInvaderOfferPresenceState> OldPresenceStates = OfferPresenceStates;
	FullBlastSession = FARInvaderFullBlastSessionState();
	OfferPresenceStates.Reset();

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const int32 SkipReward = Settings ? Settings->GetSkipScrapRewardForTier(OldSession.ActivationTier) : 0;
	if (SkipReward > 0)
	{
		SetScrapFromSave(GetScrap() + SkipReward);
	}

	ResetAllPlayerSpiceMeters();
	OnRep_FullBlastSession(OldSession);
	OnRep_OfferPresenceStates(OldPresenceStates);
	ResolveFullBlastCommonPostChoice(true, OldSession.RequestingPlayerSlot, OldSession.ActivationTier);
	ForceNetUpdate();
	return true;
}

bool AARInvaderGameState::SetOfferPresence(
	AARPlayerStateBase* SourcePlayerState,
	FGameplayTag HoveredUpgradeTag,
	const int32 HoveredDestinationSlot,
	FVector2D CursorNormalized,
	const bool bHasCursor)
{
	if (!HasAuthority() || !SourcePlayerState || !FullBlastSession.bIsActive)
	{
		return false;
	}

	const EARPlayerSlot SourceSlot = SourcePlayerState->GetPlayerSlot();
	if (SourceSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	if (HoveredUpgradeTag.IsValid())
	{
		const FARInvaderUpgradeOffer* MatchingOffer = FullBlastSession.Offers.FindByPredicate(
			[&HoveredUpgradeTag](const FARInvaderUpgradeOffer& Offer)
			{
				return Offer.UpgradeTag == HoveredUpgradeTag;
			});
		if (!MatchingOffer)
		{
			HoveredUpgradeTag = FGameplayTag();
		}
	}

	FARInvaderOfferPresenceState NewPresenceState;
	NewPresenceState.PlayerSlot = SourceSlot;
	NewPresenceState.HoveredUpgradeTag = HoveredUpgradeTag;
	NewPresenceState.HoveredDestinationSlot = HoveredDestinationSlot > 0 ? HoveredDestinationSlot : -1;
	NewPresenceState.bHasCursor = bHasCursor;
	NewPresenceState.CursorNormalized = bHasCursor
		? FVector2D(FMath::Clamp(CursorNormalized.X, 0.0f, 1.0f), FMath::Clamp(CursorNormalized.Y, 0.0f, 1.0f))
		: FVector2D::ZeroVector;

	const TArray<FARInvaderOfferPresenceState> OldPresenceStates = OfferPresenceStates;
	const int32 ExistingIndex = OfferPresenceStates.IndexOfByPredicate(
		[SourceSlot](const FARInvaderOfferPresenceState& State)
		{
			return State.PlayerSlot == SourceSlot;
		});

	if (ExistingIndex != INDEX_NONE)
	{
		const FARInvaderOfferPresenceState& ExistingState = OfferPresenceStates[ExistingIndex];
		const bool bUnchanged = ExistingState.HoveredUpgradeTag == NewPresenceState.HoveredUpgradeTag
			&& ExistingState.HoveredDestinationSlot == NewPresenceState.HoveredDestinationSlot
			&& ExistingState.bHasCursor == NewPresenceState.bHasCursor
			&& ExistingState.CursorNormalized.Equals(NewPresenceState.CursorNormalized, KINDA_SMALL_NUMBER);
		if (bUnchanged)
		{
			return true;
		}

		OfferPresenceStates[ExistingIndex] = NewPresenceState;
	}
	else
	{
		OfferPresenceStates.Add(NewPresenceState);
		OfferPresenceStates.Sort(
			[](const FARInvaderOfferPresenceState& A, const FARInvaderOfferPresenceState& B)
			{
				return static_cast<uint8>(A.PlayerSlot) < static_cast<uint8>(B.PlayerSlot);
			});
	}

	OnRep_OfferPresenceStates(OldPresenceStates);
	ForceNetUpdate();
	return true;
}

bool AARInvaderGameState::ClearOfferPresence(AARPlayerStateBase* SourcePlayerState)
{
	if (!HasAuthority() || !SourcePlayerState)
	{
		return false;
	}

	const EARPlayerSlot SourceSlot = SourcePlayerState->GetPlayerSlot();
	if (SourceSlot == EARPlayerSlot::Unknown)
	{
		return false;
	}

	const TArray<FARInvaderOfferPresenceState> OldPresenceStates = OfferPresenceStates;
	const int32 RemovedCount = OfferPresenceStates.RemoveAll(
		[SourceSlot](const FARInvaderOfferPresenceState& State)
		{
			return State.PlayerSlot == SourceSlot;
		});
	if (RemovedCount <= 0)
	{
		return false;
	}

	OnRep_OfferPresenceStates(OldPresenceStates);
	ForceNetUpdate();
	return true;
}

bool AARInvaderGameState::CanPlayerActivateUpgrade(
	const FARInvaderUpgradeDefRow& UpgradeDef,
	AARPlayerStateBase* RequestingPlayerState,
	const FGameplayTagContainer& TeamActivatedTags,
	const TMap<FGameplayTag, int32>& TeamActivationCounts) const
{
	if (!RequestingPlayerState || !UpgradeDef.UpgradeTag.IsValid())
	{
		return false;
	}

	if (!UpgradeDef.RequiredActivatedUpgradesForActivation.IsEmpty()
		&& !RequestingPlayerState->GetActivatedInvaderUpgrades().HasAll(UpgradeDef.RequiredActivatedUpgradesForActivation))
	{
		return false;
	}

	switch (UpgradeDef.ClaimPolicy)
	{
	case EARInvaderUpgradeClaimPolicy::SingleTeamClaim:
		return !TeamActivatedTags.HasTagExact(UpgradeDef.UpgradeTag);
	case EARInvaderUpgradeClaimPolicy::PerPlayerClaim:
		return !RequestingPlayerState->HasActivatedInvaderUpgrade(UpgradeDef.UpgradeTag)
			&& GetTeamActivationCount(TeamActivationCounts, UpgradeDef.UpgradeTag) < 2;
	case EARInvaderUpgradeClaimPolicy::Repeatable:
	default:
		return true;
	}
}

bool AARInvaderGameState::ApplyUpgradeActivation(AARPlayerStateBase* RequestingPlayerState, const FARInvaderUpgradeDefRow& UpgradeDef)
{
	if (!RequestingPlayerState || !UpgradeDef.UpgradeTag.IsValid())
	{
		return false;
	}

	RequestingPlayerState->MarkInvaderUpgradeActivated(UpgradeDef.UpgradeTag);

	if (UAbilitySystemComponent* ASC = RequestingPlayerState->GetASC())
	{
		if (UClass* EffectClass = UpgradeDef.OnActivateGameplayEffect.LoadSynchronous())
		{
			const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	return true;
}

bool AARInvaderGameState::ActivateTrackUpgrade(AARPlayerStateBase* RequestingPlayerState, const int32 SlotIndex)
{
	if (!HasAuthority() || !RequestingPlayerState || SlotIndex <= 0 || SlotIndex > SharedTrackSlots.Num())
	{
		return false;
	}

	const FARInvaderTrackSlotState SelectedSlot = SharedTrackSlots[SlotIndex - 1];
	if (!SelectedSlot.UpgradeTag.IsValid())
	{
		return false;
	}

	TMap<FGameplayTag, FARInvaderUpgradeDefRow> UpgradeDefinitions;
	if (!BuildUpgradeDefinitionMap(UpgradeDefinitions))
	{
		return false;
	}

	const FARInvaderUpgradeDefRow* UpgradeDef = UpgradeDefinitions.Find(SelectedSlot.UpgradeTag);
	if (!UpgradeDef)
	{
		return false;
	}

	FGameplayTagContainer TeamActivatedTags;
	TMap<FGameplayTag, int32> TeamActivationCounts;
	BuildTeamActivationState(TeamActivatedTags, TeamActivationCounts);
	if (!CanPlayerActivateUpgrade(*UpgradeDef, RequestingPlayerState, TeamActivatedTags, TeamActivationCounts))
	{
		return false;
	}

	if (!ApplyUpgradeActivation(RequestingPlayerState, *UpgradeDef))
	{
		return false;
	}

	const TArray<FARInvaderTrackSlotState> OldSlots = SharedTrackSlots;
	int32 OldTier = SharedFullBlastTier;
	bool bConsumedSlotThisActivation = false;

	if (FARInvaderTrackSlotState* MutableSlot = SharedTrackSlots.IsValidIndex(SlotIndex - 1) ? &SharedTrackSlots[SlotIndex - 1] : nullptr)
	{
		MutableSlot->bHasBeenActivated = true;

		if (!MutableSlot->bInfiniteUses)
		{
			MutableSlot->RemainingActivationUses = FMath::Max(0, MutableSlot->RemainingActivationUses - 1);
			bConsumedSlotThisActivation = (MutableSlot->RemainingActivationUses <= 0);
		}
	}

	if (bConsumedSlotThisActivation)
	{
		SharedTrackSlots.RemoveAt(SlotIndex - 1);
		SharedFullBlastTier = FMath::Max(1, SharedFullBlastTier - 1);
		TrimTrackToTierLimit();
		NormalizeTrackSlotIndices();
	}

	ResetAllPlayerSpiceMeters();
	SyncSharedMaxSpiceToPlayers();
	if (bConsumedSlotThisActivation)
	{
		RefreshWhileSlottedEffects();
	}
	ReconcilePlayerCursorSelection();

	OnRep_SharedTrackSlots(OldSlots);
	if (bConsumedSlotThisActivation && OldTier != SharedFullBlastTier)
	{
		OnRep_SharedFullBlastTier(OldTier);
	}

	ForceNetUpdate();
	return true;
}

void AARInvaderGameState::StartSharingSpice(AARPlayerStateBase* SourcePlayerState)
{
	if (!HasAuthority() || !SourcePlayerState)
	{
		return;
	}

	ActiveSpiceSharers.Add(SourcePlayerState);
}

void AARInvaderGameState::StopSharingSpice(AARPlayerStateBase* SourcePlayerState)
{
	if (!HasAuthority() || !SourcePlayerState)
	{
		return;
	}

	ActiveSpiceSharers.Remove(SourcePlayerState);
}

void AARInvaderGameState::TickShareTransfers(const float DeltaSeconds)
{
	if (!HasAuthority() || ActiveSpiceSharers.IsEmpty() || DeltaSeconds <= 0.0f)
	{
		return;
	}

	TArray<TWeakObjectPtr<AARPlayerStateBase>> InvalidSharers;
	for (const TWeakObjectPtr<AARPlayerStateBase>& WeakSharer : ActiveSpiceSharers)
	{
		AARPlayerStateBase* SourcePlayerState = WeakSharer.Get();
		if (!SourcePlayerState || !SourcePlayerState->GetASC())
		{
			InvalidSharers.Add(WeakSharer);
			continue;
		}

		AARPlayerStateBase* PartnerPlayerState = GetOtherPlayerStateFromPlayerState(SourcePlayerState);
		if (!PartnerPlayerState)
		{
			InvalidSharers.Add(WeakSharer);
			continue;
		}

		const float SourceSpice = SourcePlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
		const float PartnerSpice = PartnerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
		const float PartnerMaxSpice = PartnerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::MaxSpice);
		if (SourceSpice <= KINDA_SMALL_NUMBER || PartnerSpice >= PartnerMaxSpice - KINDA_SMALL_NUMBER)
		{
			InvalidSharers.Add(WeakSharer);
			continue;
		}

		const UAbilitySystemComponent* SourceASC = SourcePlayerState->GetASC();
		const float DrainRate = SourceASC ? FMath::Max(0.0f, SourceASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceDrainRateAttribute())) : 0.0f;
		const float ShareRatio = SourceASC ? FMath::Max(0.0f, SourceASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceShareRatioAttribute())) : 0.0f;
		if (DrainRate <= KINDA_SMALL_NUMBER || ShareRatio <= KINDA_SMALL_NUMBER)
		{
			InvalidSharers.Add(WeakSharer);
			continue;
		}

		const float DrainAttempt = FMath::Min(SourceSpice, DrainRate * DeltaSeconds);
		const float PartnerRoom = FMath::Max(0.0f, PartnerMaxSpice - PartnerSpice);
		const float GainAmount = FMath::Min(PartnerRoom, DrainAttempt * ShareRatio);
		if (GainAmount <= KINDA_SMALL_NUMBER)
		{
			InvalidSharers.Add(WeakSharer);
			continue;
		}

		const float ActualDrain = FMath::Min(SourceSpice, GainAmount / ShareRatio);
		SourcePlayerState->SetSpiceMeter(SourceSpice - ActualDrain);
		PartnerPlayerState->SetSpiceMeter(PartnerSpice + GainAmount);

		const float NewSourceSpice = SourcePlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
		const float NewPartnerSpice = PartnerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
		const float NewPartnerMaxSpice = PartnerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::MaxSpice);
		if (NewSourceSpice <= KINDA_SMALL_NUMBER || NewPartnerSpice >= NewPartnerMaxSpice - KINDA_SMALL_NUMBER)
		{
			InvalidSharers.Add(WeakSharer);
		}
	}

	for (const TWeakObjectPtr<AARPlayerStateBase>& InvalidSharer : InvalidSharers)
	{
		ActiveSpiceSharers.Remove(InvalidSharer);
	}
}

void AARInvaderGameState::TickComboTimeouts(const float ServerTimeSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	const float TimeoutSeconds = Settings ? Settings->ComboTimeoutSeconds : 0.0f;
	if (TimeoutSeconds <= 0.0f)
	{
		return;
	}

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (!PlayerState || PlayerState->GetInvaderComboCount() <= 0)
		{
			continue;
		}

		const float LastKillTime = PlayerState->GetInvaderLastKillCreditServerTime();
		if (LastKillTime < 0.0f)
		{
			continue;
		}

		if ((ServerTimeSeconds - LastKillTime) > TimeoutSeconds)
		{
			PlayerState->ResetInvaderCombo();
		}
	}
}

bool AARInvaderGameState::AwardKillCredit(AARPlayerStateBase* KillerPlayerState, const EARAffinityColor EnemyColor, const float BaseSpiceValueOverride)
{
	return AwardKillCreditInternal(
		KillerPlayerState,
		EnemyColor,
		BaseSpiceValueOverride,
		FVector::ZeroVector,
		false,
		FGameplayTag());
}

bool AARInvaderGameState::AwardKillCreditInternal(
	AARPlayerStateBase* KillerPlayerState,
	const EARAffinityColor EnemyColor,
	const float BaseSpiceValueOverride,
	const FVector EffectOrigin,
	const bool bHasEffectOrigin,
	const FGameplayTag EnemyIdentifierTag)
{
	if (!HasAuthority() || !KillerPlayerState)
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] AwardKillCreditInternal rejected (HasAuthority=%d KillerPS=%s)."),
			HasAuthority() ? 1 : 0, *GetNameSafe(KillerPlayerState));
		return false;
	}

	const EARAffinityColor EnemyAsPlayerColor = ToPlayerColor(EnemyColor);
	const EARAffinityColor KillerColor = KillerPlayerState->GetInvaderPlayerColor();
	const bool bColorMatched =
		KillerColor != EARAffinityColor::None
		&& EnemyAsPlayerColor != EARAffinityColor::None
		&& (KillerColor == EARAffinityColor::White
			|| EnemyAsPlayerColor == EARAffinityColor::White
			|| KillerColor == EnemyAsPlayerColor);

	// Combo should still update/reset on every credited kill, even when no spice is awarded.
	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	KillerPlayerState->ReportInvaderKillCredit(
		EnemyAsPlayerColor,
		GetServerWorldTimeSeconds(),
		Settings ? Settings->ComboTimeoutSeconds : 0.0f);

	if (!bColorMatched)
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] Kill credit awarded no spice due to color mismatch. Player='%s' PlayerColor=%d EnemyColor=%d"),
			*KillerPlayerState->GetName(),
			static_cast<int32>(KillerColor),
			static_cast<int32>(EnemyAsPlayerColor));
		return false;
	}

	const float BaseSpiceValue = (BaseSpiceValueOverride >= 0.0f)
		? BaseSpiceValueOverride
		: (GetSpicyTrackSettings() ? GetSpicyTrackSettings()->DefaultBaseKillSpiceValue : 0.0f);
	if (BaseSpiceValue <= 0.0f)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] AwardKillCreditInternal rejected for '%s': BaseSpiceValue=%.2f (Override=%.2f)."),
			*KillerPlayerState->GetName(), BaseSpiceValue, BaseSpiceValueOverride);
		return false;
	}

	float GainMultiplier = 1.0f;
	if (const UAbilitySystemComponent* ASC = KillerPlayerState->GetASC())
	{
		GainMultiplier = FMath::Max(0.0f, ASC->GetNumericAttribute(UARAttributeSetCore::GetSpiceGainMultiplierAttribute()));
	}

	const float SpiceGained = BaseSpiceValue * GainMultiplier;
	if (SpiceGained <= 0.0f)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] AwardKillCreditInternal rejected for '%s': SpiceGained=%.2f (Base=%.2f Multiplier=%.2f)."),
			*KillerPlayerState->GetName(), SpiceGained, BaseSpiceValue, GainMultiplier);
		return false;
	}

	const float CurrentSpice = KillerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
	KillerPlayerState->SetSpiceMeter(CurrentSpice + SpiceGained);
	const float NewSpice = KillerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::Spice);
	const float MaxSpice = KillerPlayerState->GetCoreAttributeValue(EARCoreAttributeType::MaxSpice);

	OnInvaderKillCreditAwarded.Broadcast(
		KillerPlayerState,
		KillerPlayerState->GetPlayerSlot(),
		SpiceGained,
		KillerPlayerState->GetInvaderComboCount());

	FARInvaderKillCreditFxEvent FxEventData;
	FxEventData.TargetPlayerSlot = KillerPlayerState->GetPlayerSlot();
	FxEventData.SpiceGained = SpiceGained;
	FxEventData.NewComboCount = KillerPlayerState->GetInvaderComboCount();
	FxEventData.EnemyColor = EnemyColor;
	FxEventData.EnemyIdentifierTag = EnemyIdentifierTag;
	FxEventData.bHasEffectOrigin = bHasEffectOrigin;
	FxEventData.EffectOrigin = bHasEffectOrigin ? EffectOrigin : FVector::ZeroVector;
	if (!FxEventData.bHasEffectOrigin)
	{
		if (const AController* OwnerController = Cast<AController>(KillerPlayerState->GetOwner()))
		{
			if (const APawn* Pawn = OwnerController->GetPawn())
			{
				FxEventData.EffectOrigin = Pawn->GetActorLocation();
				FxEventData.bHasEffectOrigin = true;
			}
		}
	}

	MulticastNotifyKillCreditFxEvent(FxEventData);
	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] Kill credit awarded to '%s' Slot=%d EnemyColor=%d Gained=%.2f Spice %.2f -> %.2f / %.2f Combo=%d"),
		*KillerPlayerState->GetName(),
		static_cast<int32>(KillerPlayerState->GetPlayerSlot()),
		static_cast<int32>(EnemyColor),
		SpiceGained,
		CurrentSpice,
		NewSpice,
		MaxSpice,
		KillerPlayerState->GetInvaderComboCount());
	return true;
}

void AARInvaderGameState::NotifyEnemyKilled(AAREnemyBase* Enemy, AActor* InstigatorActor)
{
	if (!HasAuthority() || !Enemy)
	{
		return;
	}

	AARPlayerStateBase* KillerPlayerState = ResolvePlayerStateFromInstigatorActor(InstigatorActor);
	if (!KillerPlayerState)
	{
		const bool bExpectedEnemyOwnedDeath = InstigatorActor == Enemy || Cast<AAREnemyBase>(InstigatorActor) != nullptr;
		if (bExpectedEnemyOwnedDeath)
		{
			UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] NotifyEnemyKilled no killer PS (expected enemy-owned death). Enemy='%s' InstigatorActor='%s'"),
				*GetNameSafe(Enemy), *GetNameSafe(InstigatorActor));
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] NotifyEnemyKilled could not resolve killer PS. Enemy='%s' InstigatorActor='%s'"),
				*GetNameSafe(Enemy), *GetNameSafe(InstigatorActor));
		}
		return;
	}

	const float BaseSpiceValue = ResolveEnemyBaseSpiceValue(Enemy);
	AwardKillCreditInternal(
		KillerPlayerState,
		Enemy->GetEnemyColor(),
		BaseSpiceValue,
		Enemy->GetActorLocation(),
		true,
		Enemy->GetEnemyIdentifierTag());

	TrySpawnEnemyDrop(Enemy, KillerPlayerState);
}

void AARInvaderGameState::TrySpawnEnemyDrop(AAREnemyBase* Enemy, AARPlayerStateBase* KillerPlayerState)
{
	if (!HasAuthority() || !Enemy || !KillerPlayerState)
	{
		return;
	}

	UAbilitySystemComponent* EnemyASC = Enemy->GetASC();
	if (!EnemyASC)
	{
		return;
	}

	const EARInvaderDropType DropType = Enemy->GetEnemyDropType();
	if (DropType == EARInvaderDropType::None)
	{
		return;
	}

	const float DropChance = FMath::Clamp(
		EnemyASC->GetNumericAttribute(UARAttributeSetCore::GetDropChanceAttribute()),
		0.0f,
		1.0f);
	if (DropChance <= 0.0f || FMath::FRand() > DropChance)
	{
		return;
	}

	const float EnemyDropAmount = FMath::Max(
		0.0f,
		EnemyASC->GetNumericAttribute(UARAttributeSetCore::GetDropAmountAttribute()));
	if (EnemyDropAmount <= 0.0f)
	{
		return;
	}

	const float VariedDropAmount = RollDropAmountWithVariance(EnemyDropAmount, DropType);
	const float KillerDropMultiplier = ResolveKillerDropMultiplier(KillerPlayerState, DropType);
	const int32 FinalDropAmount = FMath::RoundToInt(VariedDropAmount * KillerDropMultiplier);
	if (FinalDropAmount <= 0)
	{
		return;
	}

	TArray<FDropSpawnPlanEntry> SpawnPlan;
	if (!BuildDropSpawnPlan(DropType, FinalDropAmount, SpawnPlan) || SpawnPlan.IsEmpty() || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = Enemy->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	const float MinSpeed = Settings ? FMath::Max(0.0f, Settings->DropInitialLinearSpeedMin) : 120.0f;
	const float MaxSpeed = Settings ? FMath::Max(MinSpeed, Settings->DropInitialLinearSpeedMax) : 220.0f;

	for (const FDropSpawnPlanEntry& PlanEntry : SpawnPlan)
	{
		if (PlanEntry.Amount <= 0 || !PlanEntry.DropClass)
		{
			continue;
		}

		AARInvaderDropBase* SpawnedDrop = GetWorld()->SpawnActor<AARInvaderDropBase>(
			PlanEntry.DropClass,
			Enemy->GetActorLocation(),
			FRotator::ZeroRotator,
			SpawnParams);
		if (!SpawnedDrop)
		{
			continue;
		}

		SpawnedDrop->InitializeDrop(DropType, PlanEntry.Amount, Enemy->GetEnemyColor());
		SpawnedDrop->SetEarthGravityEnabled(bDebugDropEarthGravityEnabled);

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(SpawnedDrop->GetRootComponent()))
		{
			const float AngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Speed = FMath::FRandRange(MinSpeed, MaxSpeed);
			const FVector InitialVelocity(FMath::Cos(AngleRadians) * Speed, FMath::Sin(AngleRadians) * Speed, 0.0f);
			RootPrimitive->SetPhysicsLinearVelocity(InitialVelocity);
		}
	}
}

void AARInvaderGameState::SetDropEarthGravityEnabledForAll(const bool bEnabled)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	for (TActorIterator<AARInvaderDropBase> It(GetWorld()); It; ++It)
	{
		AARInvaderDropBase* Drop = *It;
		if (!Drop || Drop->IsPendingKillPending())
		{
			continue;
		}

		Drop->SetEarthGravityEnabled(bEnabled);
	}
}

float AARInvaderGameState::RollDropAmountWithVariance(const float BaseDropAmount, const EARInvaderDropType DropType) const
{
	if (BaseDropAmount <= 0.0f)
	{
		return 0.0f;
	}

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!Settings)
	{
		return BaseDropAmount;
	}

	float VarianceFraction = Settings->DropAmountVarianceFraction;
	TSoftObjectPtr<UCurveFloat> SelectedCurve = Settings->DropAmountVarianceCurve;
	if (DropType == EARInvaderDropType::Scrap)
	{
		VarianceFraction = Settings->ScrapDropAmountVarianceFraction;
		if (!Settings->ScrapDropAmountVarianceCurve.IsNull())
		{
			SelectedCurve = Settings->ScrapDropAmountVarianceCurve;
		}
	}
	else if (DropType == EARInvaderDropType::Meat)
	{
		VarianceFraction = Settings->MeatDropAmountVarianceFraction;
		if (!Settings->MeatDropAmountVarianceCurve.IsNull())
		{
			SelectedCurve = Settings->MeatDropAmountVarianceCurve;
		}
	}

	VarianceFraction = FMath::Clamp(VarianceFraction, 0.0f, 1.0f);
	if (VarianceFraction <= 0.0f)
	{
		return BaseDropAmount;
	}

	// Default distribution is center-weighted triangular noise in [-1..1].
	float SignedCenterWeightedNoise = (FMath::FRand() - FMath::FRand());

	if (UCurveFloat* VarianceCurve = SelectedCurve.LoadSynchronous())
	{
		const float SampleX = FMath::FRandRange(0.0f, 1.0f);
		SignedCenterWeightedNoise = FMath::Clamp(VarianceCurve->GetFloatValue(SampleX), -1.0f, 1.0f);
	}

	const float Multiplier = FMath::Max(0.0f, 1.0f + (SignedCenterWeightedNoise * VarianceFraction));
	return BaseDropAmount * Multiplier;
}

float AARInvaderGameState::ResolveKillerDropMultiplier(const AARPlayerStateBase* KillerPlayerState, const EARInvaderDropType DropType) const
{
	if (!KillerPlayerState)
	{
		return 0.0f;
	}

	const UAbilitySystemComponent* KillerASC = KillerPlayerState->GetASC();
	if (!KillerASC)
	{
		return 1.0f;
	}

	if (DropType == EARInvaderDropType::Meat)
	{
		return FMath::Max(0.0f, KillerASC->GetNumericAttribute(UARAttributeSetCore::GetMeatDropMultiplierAttribute()));
	}

	if (DropType == EARInvaderDropType::Scrap)
	{
		return FMath::Max(0.0f, KillerASC->GetNumericAttribute(UARAttributeSetCore::GetScrapDropMultiplierAttribute()));
	}

	return 0.0f;
}

TSubclassOf<AARInvaderDropBase> AARInvaderGameState::ResolveDropClass(const EARInvaderDropType DropType) const
{
	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (Settings)
	{
		TSoftClassPtr<AARInvaderDropBase> SoftDropClass;
		if (DropType == EARInvaderDropType::Meat)
		{
			SoftDropClass = Settings->MeatDropClass;
		}
		else if (DropType == EARInvaderDropType::Scrap)
		{
			SoftDropClass = Settings->ScrapDropClass;
		}

		if (UClass* LoadedClass = SoftDropClass.LoadSynchronous())
		{
			return LoadedClass;
		}
	}

	return AARInvaderDropBase::StaticClass();
}

void AARInvaderGameState::ResolveDropStackDefinitions(
	const EARInvaderDropType DropType,
	TArray<FResolvedDropStackEntry>& OutDefinitions) const
{
	OutDefinitions.Reset();

	const UARInvaderDirectorSettings* Settings = GetDefault<UARInvaderDirectorSettings>();
	if (!Settings)
	{
		return;
	}

	const TArray<FARInvaderDropStackDefinition>* SourceDefs = nullptr;
	if (DropType == EARInvaderDropType::Meat)
	{
		SourceDefs = &Settings->MeatDropStacks;
	}
	else if (DropType == EARInvaderDropType::Scrap)
	{
		SourceDefs = &Settings->ScrapDropStacks;
	}

	if (!SourceDefs)
	{
		return;
	}

	for (const FARInvaderDropStackDefinition& Def : *SourceDefs)
	{
		if (Def.Denomination <= 0)
		{
			continue;
		}

		UClass* LoadedClass = Def.DropClass.LoadSynchronous();
		if (!LoadedClass || !LoadedClass->IsChildOf(AARInvaderDropBase::StaticClass()))
		{
			continue;
		}

		FResolvedDropStackEntry Entry;
		Entry.Denomination = Def.Denomination;
		Entry.DropClass = LoadedClass;
		OutDefinitions.Add(Entry);
	}

	OutDefinitions.Sort([](const FResolvedDropStackEntry& A, const FResolvedDropStackEntry& B)
		{
			if (A.Denomination != B.Denomination)
			{
				return A.Denomination > B.Denomination;
			}

			return A.DropClass.Get() < B.DropClass.Get();
		});

	TSet<int32> SeenDenominations;
	for (int32 Index = OutDefinitions.Num() - 1; Index >= 0; --Index)
	{
		const int32 Denomination = OutDefinitions[Index].Denomination;
		if (SeenDenominations.Contains(Denomination))
		{
			OutDefinitions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		SeenDenominations.Add(Denomination);
	}
}

bool AARInvaderGameState::BuildDropSpawnPlan(
	const EARInvaderDropType DropType,
	const int32 TotalAmount,
	TArray<FDropSpawnPlanEntry>& OutPlan) const
{
	OutPlan.Reset();

	if (TotalAmount <= 0)
	{
		return false;
	}

	TArray<FResolvedDropStackEntry> Definitions;
	ResolveDropStackDefinitions(DropType, Definitions);

	if (Definitions.IsEmpty())
	{
		TSubclassOf<AARInvaderDropBase> FallbackClass = ResolveDropClass(DropType);
		if (!FallbackClass)
		{
			return false;
		}

		FDropSpawnPlanEntry Fallback;
		Fallback.Amount = TotalAmount;
		Fallback.DropClass = FallbackClass;
		OutPlan.Add(Fallback);
		return true;
	}

	const FResolvedDropStackEntry* LowestDenominationDef = Definitions.Num() > 0 ? &Definitions.Last() : nullptr;

	const int32 Unreachable = TNumericLimits<int32>::Max() / 4;
	TArray<int32> MinPickupCount;
	TArray<int32> MaxDenomSum;
	TArray<int32> ChoiceIndex;
	MinPickupCount.Init(Unreachable, TotalAmount + 1);
	MaxDenomSum.Init(-1, TotalAmount + 1);
	ChoiceIndex.Init(INDEX_NONE, TotalAmount + 1);

	MinPickupCount[0] = 0;
	MaxDenomSum[0] = 0;

	for (int32 Amount = 1; Amount <= TotalAmount; ++Amount)
	{
		for (int32 DefIndex = 0; DefIndex < Definitions.Num(); ++DefIndex)
		{
			const int32 Denom = Definitions[DefIndex].Denomination;
			if (Denom <= 0 || Denom > Amount)
			{
				continue;
			}

			const int32 PrevAmount = Amount - Denom;
			if (MinPickupCount[PrevAmount] >= Unreachable)
			{
				continue;
			}

			const int32 CandidateCount = MinPickupCount[PrevAmount] + 1;
			const int32 CandidateDenomSum = MaxDenomSum[PrevAmount] + Denom;
			const bool bBetterCount = CandidateCount < MinPickupCount[Amount];
			const bool bTieBetterDenom = (CandidateCount == MinPickupCount[Amount]) && (CandidateDenomSum > MaxDenomSum[Amount]);
			if (bBetterCount || bTieBetterDenom)
			{
				MinPickupCount[Amount] = CandidateCount;
				MaxDenomSum[Amount] = CandidateDenomSum;
				ChoiceIndex[Amount] = DefIndex;
			}
		}
	}

	if (ChoiceIndex[TotalAmount] == INDEX_NONE)
	{
		// No exact decomposition exists. Fall back to partial denomination decomposition
		// plus one remainder pickup using the lowest-denomination class.
		if (!LowestDenominationDef || !LowestDenominationDef->DropClass)
		{
			TSubclassOf<AARInvaderDropBase> FallbackClass = ResolveDropClass(DropType);
			if (!FallbackClass)
			{
				return false;
			}

			FDropSpawnPlanEntry Fallback;
			Fallback.Amount = TotalAmount;
			Fallback.DropClass = FallbackClass;
			OutPlan.Add(Fallback);
			return true;
		}

		int32 Remaining = TotalAmount;
		for (const FResolvedDropStackEntry& Def : Definitions)
		{
			if (Def.Denomination <= 0 || !Def.DropClass)
			{
				continue;
			}

			const int32 Count = Remaining / Def.Denomination;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FDropSpawnPlanEntry PlanEntry;
				PlanEntry.Amount = Def.Denomination;
				PlanEntry.DropClass = Def.DropClass;
				OutPlan.Add(PlanEntry);
			}

			Remaining -= Count * Def.Denomination;
			if (Remaining <= 0)
			{
				break;
			}
		}

		if (Remaining > 0)
		{
			FDropSpawnPlanEntry RemainderEntry;
			RemainderEntry.Amount = Remaining;
			RemainderEntry.DropClass = LowestDenominationDef->DropClass;
			OutPlan.Add(RemainderEntry);
		}

		return true;
	}

	TMap<int32, int32> CountsByDenomination;
	int32 Remaining = TotalAmount;
	while (Remaining > 0)
	{
		const int32 DefIndex = ChoiceIndex[Remaining];
		if (!Definitions.IsValidIndex(DefIndex))
		{
			break;
		}

		const int32 Denom = Definitions[DefIndex].Denomination;
		if (Denom <= 0 || Denom > Remaining)
		{
			break;
		}

		CountsByDenomination.FindOrAdd(Denom) += 1;
		Remaining -= Denom;
	}

	if (Remaining != 0)
	{
		TSubclassOf<AARInvaderDropBase> FallbackClass = ResolveDropClass(DropType);
		if (!FallbackClass)
		{
			return false;
		}

		FDropSpawnPlanEntry Fallback;
		Fallback.Amount = TotalAmount;
		Fallback.DropClass = FallbackClass;
		OutPlan.Add(Fallback);
		return true;
	}

	for (const FResolvedDropStackEntry& Def : Definitions)
	{
		const int32* CountPtr = CountsByDenomination.Find(Def.Denomination);
		const int32 Count = CountPtr ? *CountPtr : 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FDropSpawnPlanEntry PlanEntry;
			PlanEntry.Amount = Def.Denomination;
			PlanEntry.DropClass = Def.DropClass;
			OutPlan.Add(PlanEntry);
		}
	}

	return !OutPlan.IsEmpty();
}

float AARInvaderGameState::ResolveEnemyBaseSpiceValue(const AAREnemyBase* Enemy) const
{
	const UARInvaderSpicyTrackSettings* SpicySettings = GetSpicyTrackSettings();
	const float FallbackValue = SpicySettings ? SpicySettings->DefaultBaseKillSpiceValue : 0.0f;
	if (!Enemy)
	{
		return FallbackValue;
	}

	const FGameplayTag EnemyIdentifier = Enemy->GetEnemyIdentifierTag();
	if (!EnemyIdentifier.IsValid())
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] ResolveEnemyBaseSpiceValue fallback: enemy '%s' has invalid EnemyIdentifierTag. Fallback=%.2f"),
			*GetNameSafe(Enemy), FallbackValue);
		return FallbackValue;
	}

	const UARInvaderDirectorSettings* DirectorSettings = GetDefault<UARInvaderDirectorSettings>();
	if (!DirectorSettings)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] ResolveEnemyBaseSpiceValue fallback: missing DirectorSettings. Fallback=%.2f"), FallbackValue);
		return FallbackValue;
	}

	UDataTable* EnemyTable = DirectorSettings->EnemyDataTable.LoadSynchronous();
	if (!EnemyTable)
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderSpice] ResolveEnemyBaseSpiceValue fallback: EnemyDataTable missing. Fallback=%.2f"), FallbackValue);
		return FallbackValue;
	}

	TArray<FARInvaderEnemyDefRow*> Rows;
	EnemyTable->GetAllRows(TEXT("AARInvaderGameState::ResolveEnemyBaseSpiceValue"), Rows);
	for (const FARInvaderEnemyDefRow* Row : Rows)
	{
		if (Row && Row->EnemyIdentifierTag == EnemyIdentifier)
		{
			const float Resolved = FMath::Max(0.0f, Row->BaseSpiceKillValue);
			UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] ResolveEnemyBaseSpiceValue '%s' -> %.2f"),
				*EnemyIdentifier.ToString(), Resolved);
			return Resolved;
		}
	}

	UE_LOG(ARLog, Verbose, TEXT("[InvaderSpice] ResolveEnemyBaseSpiceValue fallback: no row for '%s'. Fallback=%.2f"),
		*EnemyIdentifier.ToString(), FallbackValue);
	return FallbackValue;
}

AARPlayerStateBase* AARInvaderGameState::ResolvePlayerStateFromInstigatorActor(AActor* InstigatorActor) const
{
	AActor* CurrentActor = InstigatorActor;
	for (int32 Depth = 0; CurrentActor && Depth < 4; ++Depth)
	{
		if (const AController* Controller = Cast<AController>(CurrentActor))
		{
			if (AARPlayerStateBase* PS = Controller->GetPlayerState<AARPlayerStateBase>())
			{
				return PS;
			}
		}

		if (const APawn* Pawn = Cast<APawn>(CurrentActor))
		{
			if (AARPlayerStateBase* PS = Pawn->GetPlayerState<AARPlayerStateBase>())
			{
				return PS;
			}

			if (const AController* PawnController = Pawn->GetController())
			{
				if (AARPlayerStateBase* PS = PawnController->GetPlayerState<AARPlayerStateBase>())
				{
					return PS;
				}
			}
		}

		if (AController* InstigatorController = CurrentActor->GetInstigatorController())
		{
			if (AARPlayerStateBase* PS = InstigatorController->GetPlayerState<AARPlayerStateBase>())
			{
				return PS;
			}
		}

		CurrentActor = CurrentActor->GetOwner();
	}

	return nullptr;
}

EARAffinityColor AARInvaderGameState::ToPlayerColor(const EARAffinityColor EnemyColor)
{
	switch (EnemyColor)
	{
	case EARAffinityColor::Red:
		return EARAffinityColor::Red;
	case EARAffinityColor::Blue:
		return EARAffinityColor::Blue;
	case EARAffinityColor::White:
	default:
		return EARAffinityColor::White;
	}
}

void AARInvaderGameState::ResolveFullBlastCommonPostChoice(const bool bSkipped, const EARPlayerSlot RequestingSlot, const int32 ActivationTier)
{
	if (UGameplayStatics::IsGamePaused(GetWorld()))
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}

	ApplyFullBlastGameplayCue();
	ClearEnemyProjectilesByTag();
	OnInvaderFullBlastResolved.Broadcast(bSkipped, ActivationTier, RequestingSlot);
}

void AARInvaderGameState::ApplyFullBlastGameplayCue()
{
	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	if (!Settings || !Settings->FullBlastGameplayCueTag.IsValid())
	{
		return;
	}

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetASC() : nullptr)
		{
			FGameplayCueParameters CueParams;
			ASC->ExecuteGameplayCue(Settings->FullBlastGameplayCueTag, CueParams);
		}
	}
}

void AARInvaderGameState::ClearEnemyProjectilesByTag()
{
	const UARInvaderSpicyTrackSettings* Settings = GetSpicyTrackSettings();
	if (!Settings || !Settings->EnemyProjectileActorTag.IsValid() || !GetWorld())
	{
		return;
	}

	const FName EnemyProjectileTagName(*Settings->EnemyProjectileActorTag.ToString());
	for (TActorIterator<AARProjectileBase> It(GetWorld()); It; ++It)
	{
		AARProjectileBase* Projectile = *It;
		if (!Projectile || !Projectile->ActorHasTag(EnemyProjectileTagName))
		{
			continue;
		}

		Projectile->ReleaseProjectile();
	}
}

void AARInvaderGameState::MulticastNotifyKillCreditFxEvent_Implementation(const FARInvaderKillCreditFxEvent& EventData)
{
	OnInvaderKillCreditFxEvent.Broadcast(EventData);
}

void AARInvaderGameState::RefreshWhileSlottedEffects()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearWhileSlottedEffects();

	TMap<FGameplayTag, FARInvaderUpgradeDefRow> UpgradeDefinitions;
	if (!BuildUpgradeDefinitionMap(UpgradeDefinitions))
	{
		return;
	}

	for (AARPlayerStateBase* PlayerState : GetPlayerStates())
	{
		if (!PlayerState)
		{
			continue;
		}

		UAbilitySystemComponent* ASC = PlayerState->GetASC();
		if (!ASC)
		{
			continue;
		}

		TArray<FActiveGameplayEffectHandle>& Handles = WhileSlottedEffectHandlesByPlayer.FindOrAdd(PlayerState);
		Handles.Reset();

		for (const FARInvaderTrackSlotState& Slot : SharedTrackSlots)
		{
			const FARInvaderUpgradeDefRow* UpgradeDef = UpgradeDefinitions.Find(Slot.UpgradeTag);
			if (!UpgradeDef)
			{
				continue;
			}

			UClass* WhileSlottedEffectClass = UpgradeDef->WhileSlottedGameplayEffect.LoadSynchronous();
			if (!WhileSlottedEffectClass)
			{
				continue;
			}

			const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(WhileSlottedEffectClass, static_cast<float>(FMath::Max(1, Slot.UpgradeLevel)), Context);
			if (!Spec.IsValid())
			{
				continue;
			}

			const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (AppliedHandle.IsValid())
			{
				Handles.Add(AppliedHandle);
			}
		}
	}
}

void AARInvaderGameState::ClearWhileSlottedEffects()
{
	for (TPair<TWeakObjectPtr<AARPlayerStateBase>, TArray<FActiveGameplayEffectHandle>>& Pair : WhileSlottedEffectHandlesByPlayer)
	{
		if (AARPlayerStateBase* PlayerState = Pair.Key.Get())
		{
			if (UAbilitySystemComponent* ASC = PlayerState->GetASC())
			{
				for (const FActiveGameplayEffectHandle Handle : Pair.Value)
				{
					ASC->RemoveActiveGameplayEffect(Handle);
				}
			}
		}
	}

	WhileSlottedEffectHandlesByPlayer.Reset();
}

void AARInvaderGameState::ClearWhileSlottedEffectsForPlayer(AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	TArray<FActiveGameplayEffectHandle> Handles;
	const TWeakObjectPtr<AARPlayerStateBase> WeakPlayerState(PlayerState);
	if (!WhileSlottedEffectHandlesByPlayer.RemoveAndCopyValue(WeakPlayerState, Handles))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = PlayerState->GetASC())
	{
		for (const FActiveGameplayEffectHandle Handle : Handles)
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
}
