#include "ARGameStateBase.h"

#include "ARLog.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"
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
