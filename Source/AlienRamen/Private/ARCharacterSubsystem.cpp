#include "ARCharacterSubsystem.h"

#include "ARCharacterStateRuntime.h"
#include "ARLog.h"
#include "ARPlayerController.h"
#include "ARPlayerStateBase.h"
#include "ARPlayerTypes.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"

void UARCharacterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisteredRuntimes.Reset();
	RuntimeByCharacterTag.Reset();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AARCharacterStateRuntime> It(World); It; ++It)
		{
			RegisterRuntime(*It);
		}
	}
}

void UARCharacterSubsystem::Deinitialize()
{
	RegisteredRuntimes.Reset();
	RuntimeByCharacterTag.Reset();
	Super::Deinitialize();
}

bool UARCharacterSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

void UARCharacterSubsystem::CompactRegistry() const
{
	for (int32 RuntimeIndex = RegisteredRuntimes.Num() - 1; RuntimeIndex >= 0; --RuntimeIndex)
	{
		AARCharacterStateRuntime* Runtime = RegisteredRuntimes[RuntimeIndex];
		if (!IsValid(Runtime))
		{
			RegisteredRuntimes.RemoveAtSwap(RuntimeIndex);
		}
	}

	RuntimeByCharacterTag.Reset();
	for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
	{
		if (!IsValid(Runtime))
		{
			continue;
		}

		const FGameplayTag NormalizedTag = ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag());
		if (!NormalizedTag.IsValid())
		{
			continue;
		}

		if (AARCharacterStateRuntime* ExistingRuntime = RuntimeByCharacterTag.FindRef(NormalizedTag))
		{
			if (ExistingRuntime != Runtime)
			{
				UE_LOG(
					ARLog,
					Warning,
					TEXT("[CharacterSubsystem] Duplicate runtime registration for tag '%s' (%s vs %s)."),
					*NormalizedTag.ToString(),
					*GetNameSafe(ExistingRuntime),
					*GetNameSafe(Runtime));
			}
			continue;
		}

		RuntimeByCharacterTag.Add(NormalizedTag, Runtime);
	}
}

AARCharacterStateRuntime* UARCharacterSubsystem::SpawnRuntimeActor(AARPlayerStateBase* OwningPlayerState, FGameplayTag CharacterTag) const
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	AARCharacterStateRuntime* Runtime = World->SpawnActor<AARCharacterStateRuntime>(
		AARCharacterStateRuntime::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	if (!Runtime)
	{
		return nullptr;
	}

	Runtime->SetOwningPlayerState(OwningPlayerState);
	Runtime->SetCharacterTag(CharacterTag);
	return Runtime;
}

AARCharacterStateRuntime* UARCharacterSubsystem::EnsureCharacterRuntime(
	AARPlayerStateBase* OwningPlayerState,
	FGameplayTag CharacterTag,
	bool& bOutCreated)
{
	bOutCreated = false;

	if (!OwningPlayerState)
	{
		return nullptr;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return nullptr;
	}

	CompactRegistry();

	if (AARCharacterStateRuntime* ExistingGlobalRuntime = RuntimeByCharacterTag.FindRef(NormalizedCharacterTag))
	{
		if (!IsValid(ExistingGlobalRuntime))
		{
			RuntimeByCharacterTag.Remove(NormalizedCharacterTag);
		}
		else if (ExistingGlobalRuntime->GetOwningPlayerState() == OwningPlayerState)
		{
			return ExistingGlobalRuntime;
		}
		else
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[CharacterSubsystem] Rejecting runtime creation for '%s': character '%s' is already owned by '%s'."),
				*GetNameSafe(OwningPlayerState),
				*NormalizedCharacterTag.ToString(),
				*GetNameSafe(ExistingGlobalRuntime->GetOwningPlayerState()));
			return nullptr;
		}
	}

	for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
	{
		if (!IsValid(Runtime))
		{
			continue;
		}

		if (Runtime->GetOwningPlayerState() == OwningPlayerState
			&& ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag()).MatchesTagExact(NormalizedCharacterTag))
		{
			return Runtime;
		}
	}

	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return FindCharacterRuntimeForPlayer(OwningPlayerState, NormalizedCharacterTag);
	}

	AARCharacterStateRuntime* Runtime = SpawnRuntimeActor(OwningPlayerState, NormalizedCharacterTag);
	if (!Runtime)
	{
		return nullptr;
	}

	RegisterRuntime(Runtime);
	bOutCreated = true;

	UARSaveSubsystem* SaveSubsystem = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr;
	UARSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	int32 CharacterStateIndex = INDEX_NONE;
	FARCharacterSaveData* ExistingCharacterState = SaveGame
		? SaveGame->FindCharacterStateDataMutable(NormalizedCharacterTag, CharacterStateIndex)
		: nullptr;

	if (ExistingCharacterState)
	{
		Runtime->ApplySaveData(*ExistingCharacterState);
	}

	if (OwningPlayerState->GetCurrentCharacterTag().MatchesTagExact(NormalizedCharacterTag))
	{
		Runtime->SetCurrentPawn(OwningPlayerState->GetPawn());
	}

	return Runtime;
}

AARCharacterStateRuntime* UARCharacterSubsystem::EnsureCharacterRuntime(AARPlayerStateBase* OwningPlayerState, FGameplayTag CharacterTag)
{
	bool bCreated = false;
	return EnsureCharacterRuntime(OwningPlayerState, CharacterTag, bCreated);
}

void UARCharacterSubsystem::RegisterRuntime(AARCharacterStateRuntime* Runtime)
{
	if (!IsValid(Runtime))
	{
		return;
	}

	const bool bAlreadyRegistered = RegisteredRuntimes.Contains(Runtime);
	if (!bAlreadyRegistered)
	{
		RegisteredRuntimes.Add(Runtime);
	}

	CompactRegistry();

	if (!bAlreadyRegistered)
	{
		OnCharacterRuntimeRegistered.Broadcast(Runtime);
	}
}

void UARCharacterSubsystem::UnregisterRuntime(AARCharacterStateRuntime* Runtime)
{
	if (!Runtime)
	{
		return;
	}

	const int32 RemovedCount = RegisteredRuntimes.Remove(Runtime);
	for (auto It = RuntimeByCharacterTag.CreateIterator(); It; ++It)
	{
		if (It.Value() == Runtime)
		{
			It.RemoveCurrent();
		}
	}

	if (RemovedCount > 0)
	{
		OnCharacterRuntimeUnregistered.Broadcast(Runtime);
	}
}

void UARCharacterSubsystem::BindRuntimePawn(AARCharacterStateRuntime* Runtime, APawn* NewPawn)
{
	if (!IsValid(Runtime))
	{
		return;
	}

	const APawn* OldPawn = Runtime->GetCurrentPawn();
	if (OldPawn == NewPawn)
	{
		return;
	}

	if (Runtime->HasAuthority())
	{
		Runtime->SetCurrentPawn(NewPawn);
	}

	OnCharacterRuntimePawnBound.Broadcast(Runtime, NewPawn);
}

AARCharacterStateRuntime* UARCharacterSubsystem::FindCharacterRuntimeByTag(FGameplayTag CharacterTag) const
{
	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return nullptr;
	}

	CompactRegistry();
	return RuntimeByCharacterTag.FindRef(NormalizedCharacterTag);
}

AARCharacterStateRuntime* UARCharacterSubsystem::FindCharacterRuntimeForPlayer(
	const AARPlayerStateBase* OwningPlayerState,
	FGameplayTag CharacterTag) const
{
	if (!OwningPlayerState)
	{
		return nullptr;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return nullptr;
	}

	CompactRegistry();
	for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
	{
		if (!IsValid(Runtime))
		{
			continue;
		}

		if (Runtime->GetOwningPlayerState() == OwningPlayerState
			&& ARPlayer::NormalizeCharacterTag(Runtime->GetCharacterTag()).MatchesTagExact(NormalizedCharacterTag))
		{
			return Runtime;
		}
	}

	return nullptr;
}

APawn* UARCharacterSubsystem::FindCharacterPawnByTag(FGameplayTag CharacterTag) const
{
	if (AARCharacterStateRuntime* Runtime = FindCharacterRuntimeByTag(CharacterTag))
	{
		return Runtime->GetCurrentPawn();
	}

	return nullptr;
}

AARCharacterStateRuntime* UARCharacterSubsystem::FindCharacterRuntimeByPawn(const APawn* Pawn) const
{
	if (!IsValid(Pawn))
	{
		return nullptr;
	}

	CompactRegistry();
	for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
	{
		if (IsValid(Runtime) && Runtime->GetCurrentPawn() == Pawn)
		{
			return Runtime;
		}
	}

	return nullptr;
}

AController* UARCharacterSubsystem::FindCharacterControllerByTag(FGameplayTag CharacterTag) const
{
	if (APawn* Pawn = FindCharacterPawnByTag(CharacterTag))
	{
		return Pawn->GetController();
	}

	if (AARCharacterStateRuntime* Runtime = FindCharacterRuntimeByTag(CharacterTag))
	{
		if (AARPlayerStateBase* PlayerState = Runtime->GetOwningPlayerState())
		{
			return Cast<AController>(PlayerState->GetOwner());
		}
	}

	return nullptr;
}

void UARCharacterSubsystem::GetRegisteredRuntimes(TArray<AARCharacterStateRuntime*>& OutRuntimes) const
{
	CompactRegistry();
	OutRuntimes.Reset();
	OutRuntimes.Reserve(RegisteredRuntimes.Num());

	for (AARCharacterStateRuntime* Runtime : RegisteredRuntimes)
	{
		if (IsValid(Runtime))
		{
			OutRuntimes.Add(Runtime);
		}
	}
}

bool UARCharacterSubsystem::TrySwapCharacter(
	AARPlayerController* RequestingController,
	FGameplayTag TargetCharacterTag,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();

	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		OutFailureReason = TEXT("Character swap requires server authority.");
		return false;
	}

	if (!RequestingController)
	{
		OutFailureReason = TEXT("Swap request has no controller.");
		return false;
	}

	AARPlayerStateBase* RequestingPlayerState = RequestingController->GetPlayerState<AARPlayerStateBase>();
	if (!RequestingPlayerState)
	{
		OutFailureReason = TEXT("Swap request has no player state.");
		return false;
	}

	TargetCharacterTag = ARPlayer::NormalizeCharacterTag(TargetCharacterTag);
	if (!TargetCharacterTag.IsValid())
	{
		OutFailureReason = TEXT("Target character tag is invalid.");
		return false;
	}

	if (AARCharacterStateRuntime* GlobalRuntime = FindCharacterRuntimeByTag(TargetCharacterTag))
	{
		if (AARPlayerStateBase* GlobalOwner = GlobalRuntime->GetOwningPlayerState())
		{
			if (GlobalOwner != RequestingPlayerState)
			{
				OutFailureReason = TEXT("Target character is already owned by another player.");
				return false;
			}
		}
	}

	bool bRuntimeCreated = false;
	AARCharacterStateRuntime* TargetRuntime = EnsureCharacterRuntime(RequestingPlayerState, TargetCharacterTag, bRuntimeCreated);
	(void)bRuntimeCreated;
	if (!TargetRuntime)
	{
		OutFailureReason = TEXT("Failed to resolve character runtime.");
		return false;
	}

	AARCharacterStateRuntime* PreviousRuntime = RequestingPlayerState->GetCurrentCharacterRuntime();
	APawn* PreviousPawn = RequestingController->GetPawn();
	APawn* TargetPawn = TargetRuntime->GetCurrentPawn();

	if (IsValid(TargetPawn) && TargetPawn->GetController() && TargetPawn->GetController() != RequestingController)
	{
		OutFailureReason = TEXT("Target character pawn is currently occupied.");
		return false;
	}

	if (PreviousRuntime && PreviousRuntime != TargetRuntime && IsValid(PreviousPawn))
	{
		BindRuntimePawn(PreviousRuntime, PreviousPawn);
	}

	const bool bNeedSpawn = !IsValid(TargetPawn);
	if (bNeedSpawn)
	{
		if (IsValid(PreviousPawn) && PreviousPawn->GetController() == RequestingController)
		{
			RequestingController->UnPossess();
		}

		RequestingPlayerState->SetCurrentCharacterRuntime(TargetRuntime);
		RequestingPlayerState->SetCurrentCharacterTag(TargetCharacterTag);

		AGameModeBase* GameMode = World->GetAuthGameMode();
		if (!GameMode)
		{
			OutFailureReason = TEXT("No game mode available to spawn target character pawn.");
			return false;
		}

		GameMode->RestartPlayer(RequestingController);
		TargetPawn = RequestingController->GetPawn();
		if (!IsValid(TargetPawn))
		{
			OutFailureReason = TEXT("Failed to spawn target character pawn.");
			return false;
		}
	}
	else if (TargetPawn != PreviousPawn)
	{
		if (IsValid(PreviousPawn) && PreviousPawn->GetController() == RequestingController)
		{
			RequestingController->UnPossess();
		}

		RequestingController->Possess(TargetPawn);
	}

	RequestingPlayerState->SetCurrentCharacterRuntime(TargetRuntime);
	if (!ARPlayer::NormalizeCharacterTag(RequestingPlayerState->GetCurrentCharacterTag()).MatchesTagExact(TargetCharacterTag))
	{
		RequestingPlayerState->SetCurrentCharacterTag(TargetCharacterTag);
	}

	BindRuntimePawn(TargetRuntime, RequestingController->GetPawn());
	OnCharacterSwapCompleted.Broadcast(RequestingController, TargetCharacterTag, RequestingController->GetPawn(), PreviousPawn);
	return true;
}
