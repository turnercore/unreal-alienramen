#include "ARGameStateBase.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AARGameStateBase::AARGameStateBase()
{
	bReplicates = true;
}

void AARGameStateBase::BeginPlay()
{
	Super::BeginPlay();

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
	OnTrackedPlayersChanged.Broadcast();
}

void AARGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	OnTrackedPlayersChanged.Broadcast();
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
