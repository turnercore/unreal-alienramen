#include "ARPlayerController.h"
#include "ARGameModeBase.h"
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

void AARPlayerController::LeaveSession()
{
	if (HasAuthority())
	{
		LeaveSessionInternal();
		return;
	}

	ServerLeaveSession();
}

void AARPlayerController::ServerLeaveSession_Implementation()
{
	LeaveSessionInternal();
}

void AARPlayerController::LeaveSessionInternal()
{
	UE_LOG(ARLog, Log, TEXT("[Session] LeaveSession requested by controller '%s' (Authority=%d)."), *GetNameSafe(this), HasAuthority() ? 1 : 0);

	if (UGameInstance* GI = GetGameInstance())
	{
		GI->ReturnToMainMenu();
		return;
	}

	ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Leaving session")));
}

void AARPlayerController::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify)
{
	if (HasAuthority())
	{
		TryStartTravelInternal(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify);
		return;
	}

	ServerTryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify);
}

void AARPlayerController::ServerTryStartTravel_Implementation(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify)
{
	TryStartTravelInternal(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify);
}

void AARPlayerController::TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify)
{
	if (AARGameModeBase* ARGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AARGameModeBase>() : nullptr)
	{
		if (!ARGameMode->TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify))
		{
			UE_LOG(ARLog, Warning, TEXT("[Travel] Controller '%s' TryStartTravel failed. URL='%s' Options='%s'"), *GetNameSafe(this), *URL, *Options);
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Travel] Controller '%s' TryStartTravel ignored: no authoritative AARGameModeBase."), *GetNameSafe(this));
}
