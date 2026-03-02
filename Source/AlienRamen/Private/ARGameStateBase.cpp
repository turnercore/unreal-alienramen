#include "ARGameStateBase.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

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
	DOREPLIFETIME(AARGameStateBase, CyclesForUI);
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

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromContext(const UObject* CurrentPlayerContext) const
{
	if (const AARPlayerStateBase* AsPS = Cast<AARPlayerStateBase>(CurrentPlayerContext))
	{
		return GetOtherPlayerStateFromPlayerState(AsPS);
	}

	if (const APlayerController* AsPC = Cast<APlayerController>(CurrentPlayerContext))
	{
		return GetOtherPlayerStateFromController(AsPC);
	}

	if (const APawn* AsPawn = Cast<APawn>(CurrentPlayerContext))
	{
		return GetOtherPlayerStateFromPawn(AsPawn);
	}

	return GetOtherPlayerStateFromPlayerState(nullptr);
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
		}
	}
}

void AARGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
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
		RefreshAllPlayersTravelReady();
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
		ServerApplyStateFromStruct(SavedState);
		return true;
	}

	return IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARGameStateBase::ServerApplyStateFromStruct_Implementation(const FInstancedStruct& SavedState)
{
	IStructSerializable::ApplyStateFromStruct_Implementation(SavedState);
}

void AARGameStateBase::SyncCyclesFromSave(int32 NewCycles)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Clamped = FMath::Max(0, NewCycles);
	if (CyclesForUI == Clamped)
	{
		return;
	}

	CyclesForUI = Clamped;
	OnRep_CyclesForUI();
	ForceNetUpdate();
}

void AARGameStateBase::NotifyHydratedFromSave()
{
	OnHydratedFromSave.Broadcast();
}

void AARGameStateBase::OnRep_CyclesForUI()
{
	// Hook for UI; currently no extra logic.
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
