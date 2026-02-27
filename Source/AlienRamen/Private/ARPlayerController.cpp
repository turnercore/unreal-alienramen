#include "ARPlayerController.h"
#include "ARLog.h"
#include "ARSaveSubsystem.h"
#include "Engine/GameInstance.h"

AARPlayerController::AARPlayerController()
{
	// Nothing required here yet.
	// Input bindings can stay in BP; this controller just provides the CommonAbilitySet reference.
}

void AARPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController() && !HasAuthority() && !bRequestedInitialCanonicalSaveSync)
	{
		bRequestedInitialCanonicalSaveSync = true;
		ServerRequestCanonicalSaveSync();
	}
}

void AARPlayerController::ClientPersistCanonicalSave_Implementation(const TArray<uint8>& SaveBytes, FName SlotBaseName, int32 SlotNumber)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			FARSaveResult Result;
			if (!SaveSubsystem->PersistCanonicalSaveFromBytes(SaveBytes, SlotBaseName, SlotNumber, Result))
			{
				UE_LOG(ARLog, Warning, TEXT("[SaveSubsystem] ClientPersistCanonicalSave failed for '%s' rev %d: %s"), *SlotBaseName.ToString(), SlotNumber, *Result.Error);
			}
		}
	}
}

void AARPlayerController::ServerRequestCanonicalSaveSync_Implementation()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UARSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UARSaveSubsystem>())
		{
			FARSaveResult Result;
			if (!SaveSubsystem->PushCurrentSaveToPlayer(this, Result))
			{
				UE_LOG(ARLog, Verbose, TEXT("[SaveSubsystem] ServerRequestCanonicalSaveSync: no current save pushed (%s)."), *Result.Error);
			}
		}
	}
}
