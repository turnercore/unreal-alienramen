#include "ARGameStateBase.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
	static constexpr uint8 PauseVoteBitP1 = 1 << 0;
	static constexpr uint8 PauseVoteBitP2 = 1 << 1;

	static uint8 GetPauseVoteBit(const EARPlayerSlot Slot)
	{
		switch (Slot)
		{
		case EARPlayerSlot::P1:
			return PauseVoteBitP1;
		case EARPlayerSlot::P2:
			return PauseVoteBitP2;
		default:
			return 0;
		}
	}

	static uint8 GetExternalPauseReasonBit(const EARPauseExternalReason Reason)
	{
		switch (Reason)
		{
		case EARPauseExternalReason::DialogueShared:
			return 1 << 0;
		case EARPauseExternalReason::InvaderFullBlast:
			return 1 << 1;
		default:
			return 0;
		}
	}

	static FARMeatState SanitizeMeatState(const FARMeatState& InMeat)
	{
		FARMeatState OutMeat = InMeat;
		OutMeat.RedAmount = FMath::Max(0, OutMeat.RedAmount);
		OutMeat.BlueAmount = FMath::Max(0, OutMeat.BlueAmount);
		OutMeat.WhiteAmount = FMath::Max(0, OutMeat.WhiteAmount);
		OutMeat.UnspecifiedAmount = FMath::Max(0, OutMeat.UnspecifiedAmount);

		for (FARMeatTypeAmount& Entry : OutMeat.AdditionalAmountsByType)
		{
			Entry.Amount = FMath::Max(0, Entry.Amount);
		}
		OutMeat.NormalizeAdditionalAmounts();

		return OutMeat;
	}

	static bool AreMeatStatesEqual(const FARMeatState& A, const FARMeatState& B)
	{
		const FARMeatState Left = SanitizeMeatState(A);
		const FARMeatState Right = SanitizeMeatState(B);

		if (Left.RedAmount != Right.RedAmount
			|| Left.BlueAmount != Right.BlueAmount
			|| Left.WhiteAmount != Right.WhiteAmount
			|| Left.UnspecifiedAmount != Right.UnspecifiedAmount
			|| Left.AdditionalAmountsByType.Num() != Right.AdditionalAmountsByType.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.AdditionalAmountsByType.Num(); ++Index)
		{
			const FARMeatTypeAmount& LeftEntry = Left.AdditionalAmountsByType[Index];
			const FARMeatTypeAmount& RightEntry = Right.AdditionalAmountsByType[Index];
			if (LeftEntry.MeatType != RightEntry.MeatType || LeftEntry.Amount != RightEntry.Amount)
			{
				return false;
			}
		}

		return true;
	}
}

AARGameStateBase::AARGameStateBase()
{
	bReplicates = true;
}

void AARGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	for (APlayerState* PlayerState : PlayerArray)
	{
		BindPlayerStateSignals(Cast<AARPlayerStateBase>(PlayerState));
	}

	if (HasAuthority())
	{
		RefreshAllPlayersTravelReady();
		RefreshPauseResolution();
	}
	else
	{
		// Client-side listeners can still query immediately; replicated value remains authoritative.
		bAllPlayersTravelReady = ComputeAllPlayersTravelReady();
	}

	if (!HasAuthority())
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			SaveSubsystem->RequestGameStateHydration(this);
		}
	}
}

void AARGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AARGameStateBase, bAllPlayersTravelReady);
	DOREPLIFETIME(AARGameStateBase, Unlocks);
	DOREPLIFETIME(AARGameStateBase, Money);
	DOREPLIFETIME(AARGameStateBase, Scrap);
	DOREPLIFETIME(AARGameStateBase, Meat);
	DOREPLIFETIME(AARGameStateBase, Cycles);
	DOREPLIFETIME(AARGameStateBase, ActiveFactionTag);
	DOREPLIFETIME(AARGameStateBase, ActiveFactionEffectTags);
	DOREPLIFETIME(AARGameStateBase, PauseMenuVoteMask);
	DOREPLIFETIME(AARGameStateBase, ExternalPauseReasonMask);
	DOREPLIFETIME(AARGameStateBase, bAllPlayersPausedByMenu);
	DOREPLIFETIME(AARGameStateBase, bAnyExternalPauseActive);
	DOREPLIFETIME(AARGameStateBase, bEffectivePauseStateActive);
}

AARPlayerStateBase* AARGameStateBase::GetPlayerBySlot(EARPlayerSlot Slot) const
{
	for (APlayerState* PS : PlayerArray)
	{
		AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
		if (IsValid(Player) && Player->GetPlayerSlot() == Slot)
		{
			return Player;
		}
	}

	return nullptr;
}

TArray<AARPlayerStateBase*> AARGameStateBase::GetPlayerStates() const
{
	TArray<AARPlayerStateBase*> Result;
	Result.Reserve(PlayerArray.Num());

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
		{
			Result.Add(ARPlayerState);
		}
	}

	return Result;
}

bool AARGameStateBase::AreAllPlayersTravelReady() const
{
	return bAllPlayersTravelReady;
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromPlayerState(const AARPlayerStateBase* CurrentPlayerState) const
{
	for (APlayerState* PS : PlayerArray)
	{
		AARPlayerStateBase* Player = Cast<AARPlayerStateBase>(PS);
		if (!IsValid(Player) || Player == CurrentPlayerState)
		{
			continue;
		}
		return Player;
	}

	return nullptr;
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromController(const APlayerController* CurrentPlayerController) const
{
	if (!CurrentPlayerController)
	{
		return GetOtherPlayerStateFromPlayerState(nullptr);
	}

	return GetOtherPlayerStateFromPlayerState(CurrentPlayerController->GetPlayerState<AARPlayerStateBase>());
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromPawn(const APawn* CurrentPlayerPawn) const
{
	if (!CurrentPlayerPawn)
	{
		return GetOtherPlayerStateFromPlayerState(nullptr);
	}

	return GetOtherPlayerStateFromPlayerState(CurrentPlayerPawn->GetPlayerState<AARPlayerStateBase>());
}

void AARGameStateBase::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
	{
		BindPlayerStateSignals(ARPlayerState);
		OnTrackedPlayersChanged.Broadcast();

		if (HasAuthority())
		{
			RefreshAllPlayersTravelReady();
			RefreshPauseResolution();
		}
	}
}

void AARGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
	if (HasAuthority())
	{
		if (const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
		{
			ClearPauseVoteForSlot(ARPlayerState->GetPlayerSlot());
		}
	}

	if (AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState))
	{
		UnbindPlayerStateSignals(ARPlayerState);
	}

	Super::RemovePlayerState(PlayerState);
	if (Cast<AARPlayerStateBase>(PlayerState))
	{
		OnTrackedPlayersChanged.Broadcast();

		if (HasAuthority())
		{
			RefreshAllPlayersTravelReady();
			RefreshPauseResolution();
		}
	}
}

void AARGameStateBase::HandlePlayerReadyStatusChanged(AARPlayerStateBase* SourcePlayerState, EARPlayerSlot SourcePlayerSlot, bool bNewReady, bool bOldReady)
{
	OnPlayerReadyChanged.Broadcast(SourcePlayerState, SourcePlayerSlot, bNewReady, bOldReady);

	if (HasAuthority())
	{
		RefreshAllPlayersTravelReady();
	}
}

void AARGameStateBase::HandlePlayerSlotChanged(EARPlayerSlot NewSlot, EARPlayerSlot OldSlot)
{
	if (HasAuthority() && NewSlot != OldSlot)
	{
		if (OldSlot != EARPlayerSlot::Unknown)
		{
			ClearPauseVoteForSlot(OldSlot);
		}

		RefreshAllPlayersTravelReady();
		RefreshPauseResolution();
	}
}

void AARGameStateBase::HandlePlayerCharacterPickedChanged(AARPlayerStateBase* SourcePlayerState, EARPlayerSlot SourcePlayerSlot, EARCharacterChoice NewCharacter, EARCharacterChoice OldCharacter)
{
	if (HasAuthority() && NewCharacter != OldCharacter)
	{
		RefreshAllPlayersTravelReady();
	}
}

void AARGameStateBase::OnRep_AllPlayersTravelReady(bool bOldAllPlayersTravelReady)
{
	OnAllPlayersTravelReadyChanged.Broadcast(bAllPlayersTravelReady, bOldAllPlayersTravelReady);
}

bool AARGameStateBase::ApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	if (!SavedState.IsValid())
	{
		return false;
	}

	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[GameState] ApplyStateFromStruct rejected on non-authority '%s'."), *GetNameSafe(this));
		return false;
	}

	return IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARGameStateBase::SyncCyclesFromSave(int32 NewCycles)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewCycles);
	if (Cycles == Clamped)
	{
		return;
	}

	const int32 OldCycles = Cycles;
	Cycles = Clamped;
	OnRep_Cycles(OldCycles);
	ForceNetUpdate();
}

void AARGameStateBase::SetUnlocksFromSave(const FGameplayTagContainer& NewUnlocks)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Unlocks == NewUnlocks)
	{
		return;
	}

	const FGameplayTagContainer OldUnlocks = Unlocks;
	Unlocks = NewUnlocks;
	OnRep_Unlocks(OldUnlocks);
	ForceNetUpdate();
}

bool AARGameStateBase::AddUnlockTag(const FGameplayTag& UnlockTag)
{
	if (!HasAuthority() || !UnlockTag.IsValid())
	{
		return false;
	}

	if (Unlocks.HasTagExact(UnlockTag))
	{
		return false;
	}

	const FGameplayTagContainer OldUnlocks = Unlocks;
	Unlocks.AddTag(UnlockTag);
	OnRep_Unlocks(OldUnlocks);
	ForceNetUpdate();
	return true;
}

bool AARGameStateBase::RemoveUnlockTag(const FGameplayTag& UnlockTag)
{
	if (!HasAuthority() || !UnlockTag.IsValid())
	{
		return false;
	}

	if (!Unlocks.HasTagExact(UnlockTag))
	{
		return false;
	}

	const FGameplayTagContainer OldUnlocks = Unlocks;
	Unlocks.RemoveTag(UnlockTag);
	OnRep_Unlocks(OldUnlocks);
	ForceNetUpdate();
	return true;
}

bool AARGameStateBase::HasUnlockTag(const FGameplayTag& UnlockTag) const
{
	return UnlockTag.IsValid() && Unlocks.HasTag(UnlockTag);
}

void AARGameStateBase::SetMoneyFromSave(int32 NewMoney)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewMoney);
	if (Money == Clamped)
	{
		return;
	}

	const int32 OldMoney = Money;
	Money = Clamped;
	OnRep_Money(OldMoney);
	ForceNetUpdate();
}

void AARGameStateBase::SetScrapFromSave(int32 NewScrap)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewScrap);
	if (Scrap == Clamped)
	{
		return;
	}

	const int32 OldScrap = Scrap;
	Scrap = Clamped;
	OnRep_Scrap(OldScrap);
	ForceNetUpdate();
}

void AARGameStateBase::SetMeatFromSave(const FARMeatState& NewMeat)
{
	if (!HasAuthority())
	{
		return;
	}

	const FARMeatState Sanitized = SanitizeMeatState(NewMeat);
	if (AreMeatStatesEqual(Meat, Sanitized))
	{
		return;
	}

	const FARMeatState OldMeat = Meat;
	Meat = Sanitized;
	OnRep_Meat(OldMeat);
	ForceNetUpdate();
}

void AARGameStateBase::SetActiveFactionTagFromSave(FGameplayTag NewActiveFactionTag)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ActiveFactionTag == NewActiveFactionTag)
	{
		return;
	}

	const FGameplayTag OldTag = ActiveFactionTag;
	ActiveFactionTag = NewActiveFactionTag;
	OnRep_ActiveFactionTag(OldTag);
	ForceNetUpdate();
}

void AARGameStateBase::SetActiveFactionEffectTagsFromSave(const FGameplayTagContainer& NewActiveFactionEffectTags)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ActiveFactionEffectTags == NewActiveFactionEffectTags)
	{
		return;
	}

	const FGameplayTagContainer OldTags = ActiveFactionEffectTags;
	ActiveFactionEffectTags = NewActiveFactionEffectTags;
	OnRep_ActiveFactionEffectTags(OldTags);
	ForceNetUpdate();
}

void AARGameStateBase::NotifyHydratedFromSave()
{
	OnHydratedFromSave.Broadcast();
}

void AARGameStateBase::OnRep_Cycles(int32 OldCycles)
{
	OnCyclesChanged.Broadcast(Cycles, OldCycles);
}

void AARGameStateBase::OnRep_Unlocks(FGameplayTagContainer OldUnlocks)
{
	OnUnlocksChanged.Broadcast(Unlocks, OldUnlocks);
}

void AARGameStateBase::OnRep_Money(int32 OldMoney)
{
	OnMoneyChanged.Broadcast(Money, OldMoney);
}

void AARGameStateBase::OnRep_Scrap(int32 OldScrap)
{
	UE_LOG(ARLog, Verbose, TEXT("[Save|Currency] Scrap changed old=%d new=%d"), OldScrap, Scrap);
	OnScrapChanged.Broadcast(Scrap, OldScrap);
}

void AARGameStateBase::OnRep_Meat(FARMeatState OldMeat)
{
	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Save|Currency] Meat changed oldTotal=%d newTotal=%d"),
		OldMeat.GetTotalAmount(),
		Meat.GetTotalAmount());
	OnMeatChanged.Broadcast(Meat, OldMeat);
}

void AARGameStateBase::OnRep_ActiveFactionTag(FGameplayTag OldActiveFactionTag)
{
	OnActiveFactionTagChanged.Broadcast(ActiveFactionTag, OldActiveFactionTag);
}

void AARGameStateBase::OnRep_ActiveFactionEffectTags(FGameplayTagContainer OldActiveFactionEffectTags)
{
	OnActiveFactionEffectTagsChanged.Broadcast(ActiveFactionEffectTags, OldActiveFactionEffectTags);
}

void AARGameStateBase::OnRep_PauseMenuVoteMask(const uint8 /*OldPauseMenuVoteMask*/)
{
	// Intentionally no-op; aggregate pause delegates are emitted from replicated aggregate fields.
}

void AARGameStateBase::OnRep_ExternalPauseReasonMask(const uint8 /*OldExternalPauseReasonMask*/)
{
	// Intentionally no-op; aggregate pause delegates are emitted from replicated aggregate fields.
}

void AARGameStateBase::OnRep_AllPlayersPausedByMenu(const bool bOldAllPlayersPausedByMenu)
{
	OnAllPlayersPausedByMenuChanged.Broadcast(bAllPlayersPausedByMenu, bOldAllPlayersPausedByMenu);
}

void AARGameStateBase::OnRep_AnyExternalPauseActive(const bool bOldAnyExternalPauseActive)
{
	OnAnyExternalPauseActiveChanged.Broadcast(bAnyExternalPauseActive, bOldAnyExternalPauseActive);
}

void AARGameStateBase::OnRep_EffectivePauseStateActive(const bool bOldEffectivePauseStateActive)
{
	OnEffectivePauseStateChanged.Broadcast(bEffectivePauseStateActive, bOldEffectivePauseStateActive);
}

bool AARGameStateBase::IsPlayerPauseMenuVoteActive(const EARPlayerSlot PlayerSlot) const
{
	const uint8 VoteBit = GetPauseVoteBit(PlayerSlot);
	return VoteBit != 0 && (PauseMenuVoteMask & VoteBit) != 0;
}

bool AARGameStateBase::IsExternalPauseReasonActive(const EARPauseExternalReason Reason) const
{
	const uint8 ReasonBit = GetExternalPauseReasonBit(Reason);
	return ReasonBit != 0 && (ExternalPauseReasonMask & ReasonBit) != 0;
}

void AARGameStateBase::SetPlayerPauseMenuVote(const EARPlayerSlot PlayerSlot, const bool bPaused)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 VoteBit = GetPauseVoteBit(PlayerSlot);
	if (VoteBit == 0)
	{
		return;
	}

	const uint8 NewMask = bPaused ? (PauseMenuVoteMask | VoteBit) : (PauseMenuVoteMask & ~VoteBit);
	if (NewMask == PauseMenuVoteMask)
	{
		RefreshPauseResolution();
		return;
	}

	const uint8 OldMask = PauseMenuVoteMask;
	PauseMenuVoteMask = NewMask;
	OnRep_PauseMenuVoteMask(OldMask);
	ForceNetUpdate();
	RefreshPauseResolution();
}

void AARGameStateBase::SetExternalPauseReasonActive(const EARPauseExternalReason Reason, const bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 ReasonBit = GetExternalPauseReasonBit(Reason);
	if (ReasonBit == 0)
	{
		return;
	}

	const uint8 NewMask = bActive ? (ExternalPauseReasonMask | ReasonBit) : (ExternalPauseReasonMask & ~ReasonBit);
	if (NewMask == ExternalPauseReasonMask)
	{
		RefreshPauseResolution();
		return;
	}

	const uint8 OldMask = ExternalPauseReasonMask;
	ExternalPauseReasonMask = NewMask;
	OnRep_ExternalPauseReasonMask(OldMask);
	ForceNetUpdate();
	RefreshPauseResolution();
}

void AARGameStateBase::BindPlayerStateSignals(AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnReadyStatusChanged.AddUniqueDynamic(this, &AARGameStateBase::HandlePlayerReadyStatusChanged);
	PlayerState->OnPlayerSlotChanged.AddUniqueDynamic(this, &AARGameStateBase::HandlePlayerSlotChanged);
	PlayerState->OnCharacterPickedChanged.AddUniqueDynamic(this, &AARGameStateBase::HandlePlayerCharacterPickedChanged);
}

void AARGameStateBase::UnbindPlayerStateSignals(AARPlayerStateBase* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnReadyStatusChanged.RemoveDynamic(this, &AARGameStateBase::HandlePlayerReadyStatusChanged);
	PlayerState->OnPlayerSlotChanged.RemoveDynamic(this, &AARGameStateBase::HandlePlayerSlotChanged);
	PlayerState->OnCharacterPickedChanged.RemoveDynamic(this, &AARGameStateBase::HandlePlayerCharacterPickedChanged);
}

bool AARGameStateBase::ComputeAllPlayersTravelReady() const
{
	bool bFoundAnyPlayer = false;

	for (APlayerState* PlayerState : PlayerArray)
	{
		const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		bFoundAnyPlayer = true;
		if (!ARPlayerState->IsTravelReady())
		{
			return false;
		}
	}

	return bFoundAnyPlayer;
}

void AARGameStateBase::RefreshAllPlayersTravelReady()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bOldAllPlayersTravelReady = bAllPlayersTravelReady;
	const bool bNewAllPlayersTravelReady = ComputeAllPlayersTravelReady();
	if (bOldAllPlayersTravelReady == bNewAllPlayersTravelReady)
	{
		return;
	}

	bAllPlayersTravelReady = bNewAllPlayersTravelReady;
	OnRep_AllPlayersTravelReady(bOldAllPlayersTravelReady);
	ForceNetUpdate();
}

bool AARGameStateBase::ComputeAllPlayersPausedByMenu() const
{
	bool bFoundAnySlottedPlayer = false;

	for (APlayerState* PlayerState : PlayerArray)
	{
		const AARPlayerStateBase* ARPlayerState = Cast<AARPlayerStateBase>(PlayerState);
		if (!ARPlayerState)
		{
			continue;
		}

		const EARPlayerSlot Slot = ARPlayerState->GetPlayerSlot();
		if (Slot == EARPlayerSlot::Unknown)
		{
			continue;
		}

		bFoundAnySlottedPlayer = true;
		if (!IsPlayerPauseMenuVoteActive(Slot))
		{
			return false;
		}
	}

	return bFoundAnySlottedPlayer;
}

void AARGameStateBase::RefreshPauseResolution()
{
	if (!HasAuthority())
	{
		return;
	}

	bool bShouldForceNetUpdate = false;

	const bool bOldAllPlayersPausedByMenu = bAllPlayersPausedByMenu;
	const bool bNewAllPlayersPausedByMenu = ComputeAllPlayersPausedByMenu();
	if (bOldAllPlayersPausedByMenu != bNewAllPlayersPausedByMenu)
	{
		bAllPlayersPausedByMenu = bNewAllPlayersPausedByMenu;
		OnRep_AllPlayersPausedByMenu(bOldAllPlayersPausedByMenu);
		bShouldForceNetUpdate = true;
	}

	const bool bOldAnyExternalPauseActive = bAnyExternalPauseActive;
	const bool bNewAnyExternalPauseActive = ExternalPauseReasonMask != 0;
	if (bOldAnyExternalPauseActive != bNewAnyExternalPauseActive)
	{
		bAnyExternalPauseActive = bNewAnyExternalPauseActive;
		OnRep_AnyExternalPauseActive(bOldAnyExternalPauseActive);
		bShouldForceNetUpdate = true;
	}

	const bool bOldEffectivePauseStateActive = bEffectivePauseStateActive;
	const bool bNewEffectivePauseStateActive = bAnyExternalPauseActive || bAllPlayersPausedByMenu;
	if (bOldEffectivePauseStateActive != bNewEffectivePauseStateActive)
	{
		bEffectivePauseStateActive = bNewEffectivePauseStateActive;
		OnRep_EffectivePauseStateActive(bOldEffectivePauseStateActive);
		bShouldForceNetUpdate = true;
	}

	if (UWorld* World = GetWorld())
	{
		const bool bCurrentlyPaused = UGameplayStatics::IsGamePaused(World);
		if (bCurrentlyPaused != bEffectivePauseStateActive)
		{
			UGameplayStatics::SetGamePaused(World, bEffectivePauseStateActive);
		}
	}

	if (bShouldForceNetUpdate)
	{
		ForceNetUpdate();
	}
}

void AARGameStateBase::ClearPauseVoteForSlot(const EARPlayerSlot PlayerSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 VoteBit = GetPauseVoteBit(PlayerSlot);
	if (VoteBit == 0 || (PauseMenuVoteMask & VoteBit) == 0)
	{
		return;
	}

	const uint8 OldMask = PauseMenuVoteMask;
	PauseMenuVoteMask &= ~VoteBit;
	OnRep_PauseMenuVoteMask(OldMask);
}
