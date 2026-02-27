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
	DOREPLIFETIME(AARGameStateBase, Players);
}

bool AARGameStateBase::AddTrackedPlayer(AARPlayerStateBase* Player)
{
	if (!HasAuthority() || !Player)
	{
		return false;
	}

	const int32 BeforeCount = Players.Num();
	Players.AddUnique(Player);
	if (Players.Num() != BeforeCount)
	{
		OnTrackedPlayersChanged.Broadcast();
		ForceNetUpdate();
		return true;
	}
	return false;
}

bool AARGameStateBase::RemoveTrackedPlayer(AARPlayerStateBase* Player)
{
	if (!HasAuthority() || !Player)
	{
		return false;
	}

	const int32 Removed = Players.Remove(Player);
	if (Removed > 0)
	{
		OnTrackedPlayersChanged.Broadcast();
		ForceNetUpdate();
		return true;
	}
	return false;
}

bool AARGameStateBase::ContainsTrackedPlayer(const AARPlayerStateBase* Player) const
{
	return Player && Players.Contains(const_cast<AARPlayerStateBase*>(Player));
}

AARPlayerStateBase* AARGameStateBase::GetPlayerBySlot(EARPlayerSlot Slot) const
{
	for (AARPlayerStateBase* Player : Players)
	{
		if (IsValid(Player) && Player->GetPlayerSlot() == Slot)
		{
			return Player;
		}
	}

	return nullptr;
}

AARPlayerStateBase* AARGameStateBase::GetOtherPlayerStateFromPlayerState(const AARPlayerStateBase* CurrentPlayerState) const
{
	for (AARPlayerStateBase* Player : Players)
	{
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

void AARGameStateBase::OnRep_Players()
{
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
