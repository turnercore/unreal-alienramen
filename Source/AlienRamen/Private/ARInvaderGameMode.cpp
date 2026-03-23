#include "ARInvaderGameMode.h"

#include "ARCharacterStateRuntime.h"
#include "ARCharacterSubsystem.h"
#include "ARGameStateBase.h"
#include "ARInvaderDirectorSubsystem.h"
#include "ARLoadoutSettings.h"
#include "ARLoadoutTypes.h"
#include "ARInvaderPlayerController.h"
#include "ARLog.h"
#include "ARPlayerCharacterInvader.h"
#include "ARPlayerStateBase.h"
#include "ARRunBuffSubsystem.h"
#include "ARSaveGame.h"
#include "ARSaveSubsystem.h"
#include "ARTaggedPlayerStart.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "StructUtils/InstancedStruct.h"
#include "TagKeySubsystem.h"
#include "TimerManager.h"

namespace
{
	static FGameplayTag ResolveCharacterTagForController(const AController* Controller)
	{
		const AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		if (!PlayerState)
		{
			return FGameplayTag();
		}

		const FGameplayTag CanonicalCharacterTag = ARPlayer::NormalizeCharacterTag(PlayerState->GetCurrentCharacterTag());
		const FGameplayTag ChoiceCharacterTag = ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());
		if (ChoiceCharacterTag.IsValid())
		{
			if (!CanonicalCharacterTag.IsValid() || !CanonicalCharacterTag.MatchesTagExact(ChoiceCharacterTag))
			{
				return ChoiceCharacterTag;
			}
		}

		if (CanonicalCharacterTag.IsValid())
		{
			return CanonicalCharacterTag;
		}

		return ARPlayer::GetCharacterTagForChoice(PlayerState->GetCharacterPicked());
	}

	static void ApplyActiveRunBuffsForController(AARInvaderGameMode* GameMode, AController* Controller)
	{
		if (!GameMode || !Controller || !GameMode->HasAuthority())
		{
			return;
		}

		if (UGameInstance* GameInstance = GameMode->GetGameInstance())
		{
			if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
			{
				if (AARPlayerStateBase* PlayerState = Controller->GetPlayerState<AARPlayerStateBase>())
				{
					RunBuffSubsystem->ApplyActiveRunBuffsToPlayerState(PlayerState);
				}
			}
		}
	}
}

AARInvaderGameMode::AARInvaderGameMode()
{
	ModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
	ensureMsgf(ModeTag.IsValid(), TEXT("[InvaderGameMode] Required gameplay tag 'Mode.Invader' is missing."));
	bAutosaveOnQuit = false;
	bAllowManualSaveInMode = false;
	bShareLocalPauseAcrossControllersInMode = true;
	bRouteModeTravelThroughTransitionMap = true;
	TransitionTravelMapURL = TEXT("/Game/Maps/Lvl_Loading");
	TransitionSourceMode = EARTransitionSourceMode::Invader;
	TransitionReason = EARTransitionReason::InvaderToScrapyard;
}

void AARInvaderGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UARRunBuffSubsystem* RunBuffSubsystem = GameInstance->GetSubsystem<UARRunBuffSubsystem>())
		{
			RunBuffSubsystem->RotateRunBuffsAtInvaderInit();
		}
		else
		{
			UE_LOG(ARLog, Warning, TEXT("[InvaderGameMode] Missing RunBuffSubsystem during invader init rotation."));
		}
	}

	if (UARInvaderDirectorSubsystem* DirectorSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARInvaderDirectorSubsystem>() : nullptr)
	{
		DirectorSubsystem->OnRunEnded.AddUniqueDynamic(this, &AARInvaderGameMode::HandleInvaderRunEnded);
	}
	else
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderGameMode] Missing InvaderDirectorSubsystem; run-end handling hook was not bound."));
	}

	ReconcileInitialControlledCharacterPawns();
	TryRestoreMissingCharacterPawns();
	UpdateInactiveCharacterPawnDamageState();
}

void AARInvaderGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RunEndAutoTravelTimer);
		RunEndAutoTravelTimer.Invalidate();

		if (UARInvaderDirectorSubsystem* DirectorSubsystem = World->GetSubsystem<UARInvaderDirectorSubsystem>())
		{
			DirectorSubsystem->OnRunEnded.RemoveDynamic(this, &AARInvaderGameMode::HandleInvaderRunEnded);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AARInvaderGameMode::FinalizeInvaderRunAndTravel(const FString& InTravelURL)
{
	if (!HasAuthority())
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderGameMode] FinalizeInvaderRunAndTravel ignored: not authority."));
		return false;
	}

	const FString TravelURL = InTravelURL.IsEmpty() ? DefaultPostRunTravelURL : InTravelURL;
	if (TravelURL.IsEmpty())
	{
		UE_LOG(ARLog, Warning, TEXT("[InvaderGameMode] FinalizeInvaderRunAndTravel failed: destination URL is empty."));
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RunEndAutoTravelTimer);
		RunEndAutoTravelTimer.Invalidate();
	}

	if (bRunEndTravelRequested)
	{
		UE_LOG(ARLog, Verbose, TEXT("[InvaderGameMode] FinalizeInvaderRunAndTravel ignored: travel already requested."));
		return true;
	}

	bRunEndTravelRequested = true;
	if (!EndModeAndTravel(TravelURL, TEXT(""), true))
	{
		bRunEndTravelRequested = false;
		UE_LOG(ARLog, Warning, TEXT("[InvaderGameMode] FinalizeInvaderRunAndTravel failed for URL '%s'."), *TravelURL);
		return false;
	}

	UE_LOG(ARLog, Log, TEXT("[InvaderGameMode] FinalizeInvaderRunAndTravel started: URL='%s' Reason=%d."),
		*TravelURL,
		static_cast<int32>(LastHandledRunEndReason));
	return true;
}

void AARInvaderGameMode::HandleInvaderRunEnded(const EARInvaderRunEndReason EndReason)
{
	if (!HasAuthority())
	{
		return;
	}

	LastHandledRunEndReason = EndReason;
	bRunEndTravelRequested = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RunEndAutoTravelTimer);
		RunEndAutoTravelTimer.Invalidate();
	}

	NotifyControllersInvaderRunEnded(EndReason);
	OnInvaderRunEnded(EndReason);
	TriggerAutoTravelAfterRunEnd();
}

void AARInvaderGameMode::NotifyControllersInvaderRunEnded(const EARInvaderRunEndReason EndReason)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || Controller->IsPendingKillPending())
		{
			continue;
		}

		if (AARInvaderPlayerController* InvaderController = Cast<AARInvaderPlayerController>(Controller))
		{
			InvaderController->ClientHandleInvaderRunEnded(EndReason);
		}
	}
}

void AARInvaderGameMode::TriggerAutoTravelAfterRunEnd()
{
	if (!bAutoTravelAfterRunEnd || bRunEndTravelRequested)
	{
		return;
	}

	if (AutoTravelAfterRunEndDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		FinalizeInvaderRunAndTravel();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RunEndAutoTravelTimer,
			this,
			&AARInvaderGameMode::HandleInvaderRunEndAutoTravelTimer,
			AutoTravelAfterRunEndDelaySeconds,
			false);
	}
}

void AARInvaderGameMode::HandleInvaderRunEndAutoTravelTimer()
{
	FinalizeInvaderRunAndTravel();
}

void AARInvaderGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	ApplyActiveRunBuffsForController(this, NewPlayer);
}

void AARInvaderGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	ApplyActiveRunBuffsForController(this, NewPlayer);
	TryRestoreMissingCharacterPawns();
	UpdateInactiveCharacterPawnDamageState();
}

UClass* AARInvaderGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (InController)
	{
		const AARPlayerStateBase* PlayerState = InController->GetPlayerState<AARPlayerStateBase>();
		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ResolveCharacterTagForController(InController));
		TSubclassOf<APawn> ResolvedPawnClass;
		if (PlayerState && ResolveInvaderPawnClassForCharacterTag(CharacterTag, PlayerState, ResolvedPawnClass) && ResolvedPawnClass)
		{
			return ResolvedPawnClass.Get();
		}
	}

	UE_LOG(
		ARLog,
		Error,
		TEXT("[InvaderGameMode] Could not resolve ship loadout tag for controller '%s'; invader pawn spawn aborted."),
		*GetNameSafe(InController));

	return nullptr;
}

bool AARInvaderGameMode::ResolveInvaderPawnClassForCharacterTag(
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	TSubclassOf<APawn>& OutPawnClass) const
{
	OutPawnClass = nullptr;

	FGameplayTagContainer LoadoutTags;
	if (!ResolveCharacterOwnedLoadout(CharacterTag, OwnerPlayerState, LoadoutTags))
	{
		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Could not resolve character-owned loadout for '%s'."),
			*CharacterTag.ToString());
		return false;
	}

	const FGameplayTag ShipRootTag = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Ship"), false);
	FGameplayTag ShipTag;
	if (!ShipRootTag.IsValid() || !FindFirstTagUnderRoot(LoadoutTags, ShipRootTag, ShipTag))
	{
		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Loadout for '%s' has no valid ship tag. Tags=%s"),
			*CharacterTag.ToString(),
			*LoadoutTags.ToStringSimple());
		return false;
	}

	return ResolveInvaderPawnClassFromShipTag(ShipTag, OutPawnClass) && OutPawnClass;
}

bool AARInvaderGameMode::ResolveCharacterOwnedLoadout(
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	FGameplayTagContainer& OutLoadoutTags) const
{
	OutLoadoutTags.Reset();

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid() || !OwnerPlayerState)
	{
		return false;
	}

	if (const UARCharacterSubsystem* CharacterSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UARCharacterSubsystem>() : nullptr)
	{
		if (AARCharacterStateRuntime* Runtime = CharacterSubsystem->FindCharacterRuntimeForPlayer(OwnerPlayerState, NormalizedCharacterTag))
		{
			OutLoadoutTags = Runtime->GetLoadoutTags();
			if (!OutLoadoutTags.IsEmpty())
			{
				return true;
			}
		}
	}

	if (const UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
	{
		if (const UARSaveGame* SaveGame = SaveSubsystem->GetCurrentSaveGame())
		{
			FARCharacterSaveData CharacterState;
			int32 CharacterIndex = INDEX_NONE;
			if (SaveGame->FindCharacterStateDataByTag(NormalizedCharacterTag, CharacterState, CharacterIndex) && !CharacterState.LoadoutTags.IsEmpty())
			{
				OutLoadoutTags = CharacterState.LoadoutTags;
				return true;
			}
		}
	}

	const UARLoadoutSettings* LoadoutSettings = GetDefault<UARLoadoutSettings>();
	OutLoadoutTags = LoadoutSettings ? LoadoutSettings->DefaultPlayerLoadoutTags : FGameplayTagContainer();
	return !OutLoadoutTags.IsEmpty();
}

AARPlayerStateBase* AARInvaderGameMode::ResolveCharacterOwnerForTag(const FGameplayTag CharacterTag) const
{
	AARGameStateBase* SharedGameState = GetGameState<AARGameStateBase>();
	if (!SharedGameState)
	{
		return nullptr;
	}

	if (AARPlayerStateBase* TaggedPlayerState = SharedGameState->GetPlayerStateByCharacterTag(CharacterTag))
	{
		const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
		if (OwningController && OwningController->PlayerState == TaggedPlayerState)
		{
			return TaggedPlayerState;
		}
	}

	const TArray<AARPlayerStateBase*> Players = SharedGameState->GetPlayerStates();
	for (AARPlayerStateBase* PlayerState : Players)
	{
		const AController* OwningController = PlayerState ? Cast<AController>(PlayerState->GetOwner()) : nullptr;
		if (OwningController && OwningController->PlayerState == PlayerState)
		{
			return PlayerState;
		}
	}

	return nullptr;
}

bool AARInvaderGameMode::ResolveCharacterSpawnTransform(
	const FGameplayTag CharacterTag,
	const AARPlayerStateBase* OwnerPlayerState,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AARTaggedPlayerStart* BestTaggedStart = nullptr;
	for (TActorIterator<AARTaggedPlayerStart> It(World); It; ++It)
	{
		AARTaggedPlayerStart* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->MatchesSpawnIdentityTag(CharacterTag, true))
		{
			BestTaggedStart = Candidate;
			break;
		}

		if (!BestTaggedStart && Candidate->MatchesSpawnIdentityTag(CharacterTag, false))
		{
			BestTaggedStart = Candidate;
		}
	}

	if (BestTaggedStart)
	{
		OutTransform = BestTaggedStart->GetActorTransform();
		return true;
	}

	if (TActorIterator<APlayerStart> StartIt(World); StartIt)
	{
		if (APlayerStart* DefaultStart = *StartIt)
		{
			OutTransform = DefaultStart->GetActorTransform();
			return true;
		}
	}

	if (const APawn* OwnerPawn = OwnerPlayerState ? OwnerPlayerState->GetPawn() : nullptr)
	{
		OutTransform = OwnerPawn->GetActorTransform();
		FVector Location = OutTransform.GetLocation();
		Location += OutTransform.GetUnitAxis(EAxis::Y) * 150.0f;
		OutTransform.SetLocation(Location);
		return true;
	}

	return false;
}

bool AARInvaderGameMode::ReconcileInitialControlledCharacterPawns() const
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UARCharacterSubsystem* CharacterSubsystem = World ? World->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	if (!World || !CharacterSubsystem)
	{
		return false;
	}

	bool bMutatedAny = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const FGameplayTag CharacterTag = ARPlayer::NormalizeCharacterTag(ResolveCharacterTagForController(Controller));
		if (!Controller || !PlayerState || !CharacterTag.IsValid())
		{
			continue;
		}

		bool bCreatedRuntime = false;
		AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(PlayerState, CharacterTag, bCreatedRuntime);
		if (!Runtime)
		{
			continue;
		}

		PlayerState->SetCurrentCharacterRuntime(Runtime);

		APawn* DesiredPawn = Runtime->GetCurrentPawn();
		APawn* ControlledPawn = Controller->GetPawn();
		if (!DesiredPawn)
		{
			DesiredPawn = ControlledPawn;
		}

		if (!DesiredPawn)
		{
			continue;
		}

		if (DesiredPawn != ControlledPawn)
		{
			if (DesiredPawn->GetController() && DesiredPawn->GetController() != Controller)
			{
				continue;
			}

			CharacterSubsystem->BindRuntimePawn(Runtime, DesiredPawn);

			if (ControlledPawn)
			{
				Controller->UnPossess();
			}

			Controller->Possess(DesiredPawn);
			bMutatedAny = true;

			if (ControlledPawn && ControlledPawn != DesiredPawn && !ControlledPawn->GetController())
			{
				ControlledPawn->Destroy();
			}
		}

		FTransform DesiredTransform = FTransform::Identity;
		if (ResolveCharacterSpawnTransform(CharacterTag, PlayerState, DesiredTransform))
		{
			DesiredPawn->SetActorTransform(DesiredTransform, false, nullptr, ETeleportType::TeleportPhysics);
			bMutatedAny = true;
		}

		CharacterSubsystem->BindRuntimePawn(Runtime, DesiredPawn);
	}

	return bMutatedAny;
}

bool AARInvaderGameMode::TryRestoreMissingCharacterPawns() const
{
	if (!HasAuthority())
	{
		return false;
	}

	TSet<FGameplayTag> CandidateCharacterTags;
	for (const FGameplayTag& OrderedTag : PlayableCharacterSwitchOrder)
	{
		const FGameplayTag CanonicalTag = ARPlayer::NormalizeCharacterTag(OrderedTag);
		if (CanonicalTag.IsValid())
		{
			CandidateCharacterTags.Add(CanonicalTag);
		}
	}

	const FGameplayTag BrotherTag = ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Brother);
	const FGameplayTag SisterTag = ARPlayer::GetCharacterTagForChoice(EARCharacterChoice::Sister);
	if (BrotherTag.IsValid())
	{
		CandidateCharacterTags.Add(BrotherTag);
	}
	if (SisterTag.IsValid())
	{
		CandidateCharacterTags.Add(SisterTag);
	}

	bool bMutatedAny = false;
	for (const FGameplayTag& CharacterTag : CandidateCharacterTags)
	{
		bMutatedAny = TryRestoreMissingCharacterPawn(CharacterTag) || bMutatedAny;
	}

	return bMutatedAny;
}

bool AARInvaderGameMode::TryRestoreMissingCharacterPawn(const FGameplayTag CharacterTag) const
{
	if (!HasAuthority())
	{
		return false;
	}

	const FGameplayTag NormalizedCharacterTag = ARPlayer::NormalizeCharacterTag(CharacterTag);
	if (!NormalizedCharacterTag.IsValid())
	{
		return false;
	}

	AARGameStateBase* SharedGameState = GetGameState<AARGameStateBase>();
	if (SharedGameState)
	{
		if (AARPlayerStateBase* TaggedPlayerState = SharedGameState->GetPlayerStateByCharacterTag(NormalizedCharacterTag))
		{
			const AController* OwningController = Cast<AController>(TaggedPlayerState->GetOwner());
			if (OwningController && OwningController->PlayerState == TaggedPlayerState)
			{
				return false;
			}
		}
	}

	AARPlayerStateBase* OwnerPlayerState = ResolveCharacterOwnerForTag(NormalizedCharacterTag);
	UWorld* World = GetWorld();
	UARCharacterSubsystem* CharacterSubsystem = World ? World->GetSubsystem<UARCharacterSubsystem>() : nullptr;
	if (!OwnerPlayerState || !World || !CharacterSubsystem)
	{
		return false;
	}

	bool bCreatedRuntime = false;
	AARCharacterStateRuntime* Runtime = CharacterSubsystem->EnsureCharacterRuntime(OwnerPlayerState, NormalizedCharacterTag, bCreatedRuntime);
	if (!Runtime)
	{
		return false;
	}

	APawn* ExistingPawn = Runtime->GetCurrentPawn();
	if (ExistingPawn && ExistingPawn->GetController())
	{
		return false;
	}

	TSubclassOf<APawn> PawnClass;
	if (!ResolveInvaderPawnClassForCharacterTag(NormalizedCharacterTag, OwnerPlayerState, PawnClass) || !PawnClass)
	{
		return false;
	}

	FTransform SpawnTransform = FTransform::Identity;
	if (!ResolveCharacterSpawnTransform(NormalizedCharacterTag, OwnerPlayerState, SpawnTransform))
	{
		return false;
	}

	APawn* PawnToBind = ExistingPawn;
	if (!PawnToBind)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		PawnToBind = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams);
		if (!PawnToBind)
		{
			return false;
		}
	}
	else
	{
		PawnToBind->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	CharacterSubsystem->BindRuntimePawn(Runtime, PawnToBind);
	if (AARPlayerCharacterInvader* InvaderPawn = Cast<AARPlayerCharacterInvader>(PawnToBind))
	{
		InvaderPawn->SetCanBeDamaged(false);
	}
	return true;
}

void AARInvaderGameMode::UpdateInactiveCharacterPawnDamageState() const
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AARPlayerCharacterInvader> It(World); It; ++It)
	{
		AARPlayerCharacterInvader* InvaderPawn = *It;
		if (IsValid(InvaderPawn))
		{
			InvaderPawn->SetCanBeDamaged(InvaderPawn->GetController() != nullptr);
		}
	}
}

bool AARInvaderGameMode::ResolveInvaderPawnClassFromShipTag(const FGameplayTag ShipTag, TSubclassOf<APawn>& OutPawnClass) const
{
	OutPawnClass = nullptr;
	if (!ShipTag.IsValid())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTagKeySubsystem* Resolver = GameInstance ? GameInstance->GetSubsystem<UTagKeySubsystem>() : nullptr;
	if (!Resolver)
	{
		return false;
	}

	FInstancedStruct ShipRow;
	FString ResolveError;
	if (!Resolver->TryResolveRowStructForTag(ShipTag, ShipRow, ResolveError))
	{
		UE_LOG(
			ARLog,
			Warning,
			TEXT("[InvaderGameMode] Failed resolving ship row '%s': %s"),
			*ShipTag.ToString(),
			*ResolveError);
		return false;
	}

	const FARShipDefRow* ShipDef = ShipRow.GetPtr<FARShipDefRow>();
	if (!ShipDef)
	{
		const UScriptStruct* RowStruct = ShipRow.GetScriptStruct();
		UE_LOG(
			ARLog,
			Error,
			TEXT("[InvaderGameMode] Ship row '%s' resolved to unexpected struct '%s'; expected FARShipDefRow."),
			*ShipTag.ToString(),
			*GetNameSafe(RowStruct));
		return false;
	}

	if (ShipDef->InvaderPawnClass.IsNull())
	{
		return false;
	}

	if (UClass* PawnClass = ShipDef->InvaderPawnClass.LoadSynchronous())
	{
		OutPawnClass = PawnClass;
		return true;
	}

	UE_LOG(
		ARLog,
		Error,
		TEXT("[InvaderGameMode] Ship row '%s' InvaderPawnClass failed to load (Path=%s)."),
		*ShipTag.ToString(),
		*ShipDef->InvaderPawnClass.ToString());
	return false;
}

bool AARInvaderGameMode::FindFirstTagUnderRoot(const FGameplayTagContainer& InTags, const FGameplayTag& RootTag, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();
	if (!RootTag.IsValid())
	{
		return false;
	}

	for (const FGameplayTag Tag : InTags)
	{
		if (Tag.IsValid() && Tag.MatchesTag(RootTag))
		{
			OutTag = Tag;
			return true;
		}
	}

	return false;
}
