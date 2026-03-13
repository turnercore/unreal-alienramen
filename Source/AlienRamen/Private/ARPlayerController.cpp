#include "ARPlayerController.h"
#include "ARDialogueSubsystem.h"
#include "ARDialogueWidgetBase.h"
#include "ARHUDBase.h"
#include "ARInteractableRangeListener.h"
#include "ARGameStateBase.h"
#include "ARGameModeBase.h"
#include "ARInvaderGameState.h"
#include "ARLobbyPlayerController.h"
#include "ARLog.h"
#include "ARNPCCharacterBase.h"
#include "ARMeatStorageBoxActor.h"
#include "ARPlayerStateBase.h"
#include "ARSaveSubsystem.h"
#include "ARAttributeSetCore.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

namespace
{
	static UPrimitiveComponent* ResolveInteractionPhysicsPrimitive(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			return RootPrimitive;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				return Primitive;
			}
		}

		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				return Primitive;
			}
		}

		return PrimitiveComponents.Num() > 0 ? PrimitiveComponents[0] : nullptr;
	}

	static float ResolveKickStrengthForController(const AARPlayerController* Controller)
	{
		float Strength = 10.0f;
		const AARPlayerStateBase* PlayerState = Controller ? Controller->GetPlayerState<AARPlayerStateBase>() : nullptr;
		const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetASC() : nullptr;
		if (ASC)
		{
			Strength = ASC->GetNumericAttribute(UARAttributeSetCore::GetStrengthAttribute());
		}

		return FMath::Max(0.0f, Strength) * 100.0f;
	}
}

AARPlayerController::AARPlayerController()
{
	// Nothing required here yet.
	// Input bindings can stay in BP; this controller just provides the CommonAbilitySet reference.
}

void AARPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefaultInputMappings(true);
	InitializeCustomCursor();
	RequestHUDInitializationInternal(false);
	EnsureDialogueWidget();
	RefreshDialogueInputStateFromSession();

	if (IsLocalController() && !HasAuthority() && !bRequestedInitialCanonicalSaveSync)
	{
		bRequestedInitialCanonicalSaveSync = true;
		ServerRequestCanonicalSaveSync();
	}
}

void AARPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPauseMenuOpenLocal(false);
	ApplyDialogueInputContexts(false);
	if (bDialogueInputModeApplied)
	{
		ApplyDialogueInputMode(false);
	}
	RemoveDialogueWidget();
	ApplyDefaultInputMappings(false);
	StopHUDInitializationRetry();
	Super::EndPlay(EndPlayReason);
}

void AARPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bPauseMenuOpenLocal && IsPauseMenuBlockedLocal())
	{
		RequestClosePauseMenu();
	}

	TickActiveInteractionRangeValidation(DeltaTime);
}

void AARPlayerController::InitializeCustomCursor()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!bEnableCustomCursorInit)
	{
		return;
	}

	if (!CursorDefaultWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[UI] Custom cursor init skipped on '%s': CursorDefaultWidgetClass is not set."), *GetNameSafe(this));
		return;
	}

	if (!Cursor)
	{
		Cursor = CreateWidget<UUserWidget>(this, CursorDefaultWidgetClass);
	}

	if (!Cursor)
	{
		UE_LOG(ARLog, Warning, TEXT("[UI] Custom cursor init failed on '%s': could not create cursor widget."), *GetNameSafe(this));
		return;
	}

	SetMouseCursorWidget(EMouseCursor::Default, Cursor);
}

void AARPlayerController::RequestHUDInitialization()
{
	RequestHUDInitializationInternal(true);
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

	// Remote client leave requests are executed on the server controller instance for that client.
	// Only return that specific client to menu; do not collapse the host session.
	if (HasAuthority() && !IsLocalController())
	{
		ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Leaving session")));
		return;
	}

	// Host/standalone leave path: best-effort autosave if dirty, except while actively in invader mode.
	if (HasAuthority())
	{
		if (AARGameModeBase* ARGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AARGameModeBase>() : nullptr)
		{
			const FGameplayTag InvaderModeTag = FGameplayTag::RequestGameplayTag(TEXT("Mode.Invader"), false);
			const bool bInInvaderMode = InvaderModeTag.IsValid() && ARGameMode->GetModeTag().MatchesTagExact(InvaderModeTag);
			if (!bInInvaderMode)
			{
				if (UARSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARSaveSubsystem>() : nullptr)
				{
					FARSaveResult SaveResult;
					const bool bSaved = SaveSubsystem->RequestAutosaveIfDirty(true, SaveResult);
					if (bSaved)
					{
						UE_LOG(ARLog, Log, TEXT("[Session] LeaveSession autosave succeeded (Slot=%s Rev=%d)."), *SaveResult.SlotName.ToString(), SaveResult.SlotNumber);
					}
					else if (!SaveResult.Error.IsEmpty())
					{
						UE_LOG(ARLog, Verbose, TEXT("[Session] LeaveSession autosave skipped/failed: %s"), *SaveResult.Error);
					}
				}
			}
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		GI->ReturnToMainMenu();
		return;
	}

	ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Leaving session")));
}

void AARPlayerController::TryStartTravel(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE, EARTravelRoutePolicy RoutePolicy)
{
	if (HasAuthority())
	{
		TryStartTravelInternal(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, RoutePolicy);
		return;
	}

	ServerTryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, RoutePolicy);
}

void AARPlayerController::ServerTryStartTravel_Implementation(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE, EARTravelRoutePolicy RoutePolicy)
{
	TryStartTravelInternal(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, RoutePolicy);
}

void AARPlayerController::TryStartTravelInternal(const FString& URL, const FString& Options, bool bSkipReadyChecks, bool bAbsolute, bool bSkipGameNotify, bool bUseOpenLevelInPIE, EARTravelRoutePolicy RoutePolicy)
{
	if (AARGameModeBase* ARGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AARGameModeBase>() : nullptr)
	{
		if (!ARGameMode->TryStartTravel(URL, Options, bSkipReadyChecks, bAbsolute, bSkipGameNotify, bUseOpenLevelInPIE, RoutePolicy))
		{
			UE_LOG(ARLog, Warning, TEXT("[Travel] Controller '%s' TryStartTravel failed. URL='%s' Options='%s' RoutePolicy=%s"), *GetNameSafe(this), *URL, *Options, *ARTransition::LexToString(RoutePolicy));
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Travel] Controller '%s' TryStartTravel ignored: no authoritative AARGameModeBase."), *GetNameSafe(this));
}

void AARPlayerController::RequestAddUnlock(const FGameplayTag& UnlockTag)
{
	if (HasAuthority())
	{
		RequestAddUnlockInternal(UnlockTag);
		return;
	}

	ServerRequestAddUnlock(UnlockTag);
}

void AARPlayerController::RequestRemoveUnlock(const FGameplayTag& UnlockTag)
{
	if (HasAuthority())
	{
		RequestRemoveUnlockInternal(UnlockTag);
		return;
	}

	ServerRequestRemoveUnlock(UnlockTag);
}

void AARPlayerController::ServerRequestAddUnlock_Implementation(const FGameplayTag& UnlockTag)
{
	RequestAddUnlockInternal(UnlockTag);
}

void AARPlayerController::ServerRequestRemoveUnlock_Implementation(const FGameplayTag& UnlockTag)
{
	RequestRemoveUnlockInternal(UnlockTag);
}

void AARPlayerController::RequestAddUnlockInternal(const FGameplayTag& UnlockTag)
{
	if (AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr)
	{
		if (!ARGameState->AddUnlockTag(UnlockTag))
		{
			UE_LOG(ARLog, Verbose, TEXT("[Save] RequestAddUnlock ignored for '%s' tag '%s'."), *GetNameSafe(this), *UnlockTag.ToString());
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Save] RequestAddUnlock ignored: no AARGameStateBase for '%s'."), *GetNameSafe(this));
}

void AARPlayerController::RequestRemoveUnlockInternal(const FGameplayTag& UnlockTag)
{
	if (AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr)
	{
		if (!ARGameState->RemoveUnlockTag(UnlockTag))
		{
			UE_LOG(ARLog, Verbose, TEXT("[Save] RequestRemoveUnlock ignored for '%s' tag '%s'."), *GetNameSafe(this), *UnlockTag.ToString());
		}
		return;
	}

	UE_LOG(ARLog, Warning, TEXT("[Save] RequestRemoveUnlock ignored: no AARGameStateBase for '%s'."), *GetNameSafe(this));
}

void AARPlayerController::RequestStartDialogue(FGameplayTag SpeakerTag)
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->TryStartDialogueWithSpeaker(this, SpeakerTag);
		}
		return;
	}

	ServerRequestStartDialogue(SpeakerTag);
}

void AARPlayerController::ServerRequestStartDialogue_Implementation(FGameplayTag SpeakerTag)
{
	RequestStartDialogue(SpeakerTag);
}

bool AARPlayerController::IsServerInteractionTargetReachable(const AActor* TargetActor, const TCHAR* ContextLabel) const
{
	return IsServerInteractionTargetReachableInternal(TargetActor, ContextLabel, true);
}

bool AARPlayerController::IsServerInteractionTargetReachableInternal(const AActor* TargetActor, const TCHAR* ContextLabel, const bool bLogFailures) const
{
	const TCHAR* SafeContextLabel = ContextLabel ? ContextLabel : TEXT("Interaction");
	if (!HasAuthority())
	{
		if (bLogFailures)
		{
			UE_LOG(ARLog, Warning, TEXT("[%s] Validation called without authority on controller '%s'."), SafeContextLabel, *GetNameSafe(this));
		}
		return false;
	}

	if (!IsValid(TargetActor))
	{
		if (bLogFailures)
		{
			UE_LOG(ARLog, Verbose, TEXT("[%s] Validation failed on '%s': target is invalid."), SafeContextLabel, *GetNameSafe(this));
		}
		return false;
	}

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		if (bLogFailures)
		{
			UE_LOG(ARLog, Verbose, TEXT("[%s] Validation failed on '%s': missing controlled pawn."), SafeContextLabel, *GetNameSafe(this));
		}
		return false;
	}

	if (TargetActor->GetWorld() != GetWorld())
	{
		if (bLogFailures)
		{
			UE_LOG(
				ARLog,
				Verbose,
				TEXT("[%s] Validation failed on '%s': target '%s' is in different world."),
				SafeContextLabel,
				*GetNameSafe(this),
				*GetNameSafe(TargetActor));
		}
		return false;
	}

	const float MaxDistance = FMath::Max(50.0f, ServerInteractionMaxDistance);
	const float DistanceSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (DistanceSq > FMath::Square(MaxDistance))
	{
		if (bLogFailures)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[%s] Rejected out-of-range interaction from '%s' to '%s' (distance=%.1f max=%.1f)."),
				SafeContextLabel,
				*GetNameSafe(this),
				*GetNameSafe(TargetActor),
				FMath::Sqrt(DistanceSq),
				MaxDistance);
		}
		return false;
	}

	return true;
}

void AARPlayerController::SetActiveInteractable(AActor* InteractableActor)
{
	ActiveInteractable = InteractableActor;
}

void AARPlayerController::SetActiveSecondaryInteractable(AActor* InteractableActor)
{
	ActiveSecondaryInteractable = InteractableActor;
}

void AARPlayerController::ClearActiveInteractable(const bool bNotifyOutOfRange)
{
	if (bNotifyOutOfRange)
	{
		NotifyInteractableOutOfRange(ActiveInteractable, false);
	}
	ActiveInteractable = nullptr;
	RefreshInteractionGateFromActiveTargets();
}

void AARPlayerController::ClearActiveSecondaryInteractable(const bool bNotifyOutOfRange)
{
	if (bNotifyOutOfRange)
	{
		NotifyInteractableOutOfRange(ActiveSecondaryInteractable, true);
	}
	ActiveSecondaryInteractable = nullptr;
	RefreshInteractionGateFromActiveTargets();
}

void AARPlayerController::SetIsInteracting(const bool bInIsInteracting)
{
	bIsInteracting = bInIsInteracting;
}

void AARPlayerController::TickActiveInteractionRangeValidation(const float DeltaTime)
{
	if (!HasAuthority())
	{
		return;
	}

	ActiveInteractionRangeCheckAccumulator += FMath::Max(0.0f, DeltaTime);
	const float CheckInterval = FMath::Max(0.02f, ActiveInteractionRangeCheckInterval);
	if (ActiveInteractionRangeCheckAccumulator < CheckInterval)
	{
		return;
	}
	ActiveInteractionRangeCheckAccumulator = 0.0f;

	if (ActiveInteractable && !IsServerInteractionTargetReachableInternal(ActiveInteractable, TEXT("Interact|Active"), false))
	{
		NotifyInteractableOutOfRange(ActiveInteractable, false);
		ActiveInteractable = nullptr;
	}

	if (ActiveSecondaryInteractable && !IsServerInteractionTargetReachableInternal(ActiveSecondaryInteractable, TEXT("Interact|SecondaryActive"), false))
	{
		NotifyInteractableOutOfRange(ActiveSecondaryInteractable, true);
		ActiveSecondaryInteractable = nullptr;
	}

	RefreshInteractionGateFromActiveTargets();
}

void AARPlayerController::NotifyInteractableOutOfRange(AActor* InteractableActor, const bool bWasSecondaryInteraction)
{
	if (!IsValid(InteractableActor))
	{
		return;
	}

	if (!InteractableActor->GetClass()->ImplementsInterface(UARInteractableRangeListener::StaticClass()))
	{
		return;
	}

	IARInteractableRangeListener::Execute_OnPlayerOutOfRange(InteractableActor, this, bWasSecondaryInteraction);
}

void AARPlayerController::RefreshInteractionGateFromActiveTargets()
{
	if (ActiveInteractable || ActiveSecondaryInteractable)
	{
		return;
	}

	bIsInteracting = false;
}

void AARPlayerController::RequestInteractWithCharacter(AARNPCCharacterBase* CharacterActor)
{
	if (!CharacterActor)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Interact] RequestInteractWithCharacter ignored on '%s': target is null."), *GetNameSafe(this));
		return;
	}

	UE_LOG(
		ARLog,
		Verbose,
		TEXT("[Interact] RequestInteractWithCharacter controller='%s' target='%s' authority=%s"),
		*GetNameSafe(this),
		*GetNameSafe(CharacterActor),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(CharacterActor, TEXT("Interact")))
		{
			return;
		}

		CharacterActor->InteractByController(this);
		return;
	}

	ServerRequestInteractWithCharacter(CharacterActor);
}

void AARPlayerController::ServerRequestInteractWithCharacter_Implementation(AARNPCCharacterBase* CharacterActor)
{
	RequestInteractWithCharacter(CharacterActor);
}

void AARPlayerController::RequestKickActor(AActor* TargetActor)
{
	if (!TargetActor)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Interact|Kick] Request ignored on '%s': TargetActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(TargetActor, TEXT("Interact|Kick")))
		{
			return;
		}

		UPrimitiveComponent* PhysicsPrimitive = ResolveInteractionPhysicsPrimitive(TargetActor);
		if (!PhysicsPrimitive)
		{
			UE_LOG(
				ARLog,
				Warning,
				TEXT("[Interact|Kick] Request failed on '%s': target '%s' has no physics primitive."),
				*GetNameSafe(this),
				*GetNameSafe(TargetActor));
			return;
		}

		if (!PhysicsPrimitive->IsSimulatingPhysics())
		{
			PhysicsPrimitive->SetSimulatePhysics(true);
		}
		PhysicsPrimitive->SetEnableGravity(true);
		PhysicsPrimitive->WakeAllRigidBodies();

		const float KickStrength = FMath::Max(50.0f, ResolveKickStrengthForController(this));
		const FVector KickDirection = GetControlRotation().Vector().GetSafeNormal();
		PhysicsPrimitive->AddImpulse(KickDirection * KickStrength, NAME_None, true);

		const APawn* ControlledPawn = GetPawn();
		const float PawnZ = ControlledPawn ? ControlledPawn->GetActorLocation().Z : 0.0f;
		FVector TargetOrigin = TargetActor->GetActorLocation();
		FVector TargetExtent = FVector::ZeroVector;
		TargetActor->GetActorBounds(true, TargetOrigin, TargetExtent);
		const float HeightDelta = TargetOrigin.Z - PawnZ;
		const EARInteractionActionCue KickOrSlapCue =
			HeightDelta >= FMath::Max(0.0f, SlapCueMinHeightDeltaCm)
			? EARInteractionActionCue::Slap
			: EARInteractionActionCue::Kick;

		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Interact|Kick] Applied impulse controller='%s' target='%s' strength=%.1f heightDelta=%.1f cue=%s."),
			*GetNameSafe(this),
			*GetNameSafe(TargetActor),
			KickStrength,
			HeightDelta,
			*StaticEnum<EARInteractionActionCue>()->GetValueAsString(KickOrSlapCue));
		NotifyInteractionActionCue(KickOrSlapCue, TargetActor);
		return;
	}

	ServerRequestKickActor(TargetActor);
}

void AARPlayerController::ServerRequestKickActor_Implementation(AActor* TargetActor)
{
	RequestKickActor(TargetActor);
}

void AARPlayerController::RequestShopDispenseMeat(AARMeatStorageBoxActor* StorageActor)
{
	if (!StorageActor)
	{
		UE_LOG(ARLog, VeryVerbose, TEXT("[Shop|Storage] RequestShopDispenseMeat ignored on '%s': StorageActor is null."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		if (!IsServerInteractionTargetReachable(StorageActor, TEXT("Shop|Storage")))
		{
			return;
		}

		const bool bInteracted = StorageActor->TryHandleStorageInteraction(this);
		UE_LOG(
			ARLog,
			Verbose,
			TEXT("[Shop|Storage] RequestShopDispenseMeat controller='%s' storage='%s' success=%d."),
			*GetNameSafe(this),
			*GetNameSafe(StorageActor),
			bInteracted ? 1 : 0);
		return;
	}

	ServerRequestShopDispenseMeat(StorageActor);
}

void AARPlayerController::ServerRequestShopDispenseMeat_Implementation(AARMeatStorageBoxActor* StorageActor)
{
	RequestShopDispenseMeat(StorageActor);
}

void AARPlayerController::RequestAdvanceDialogue()
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->AdvanceConversation(this);
		}
		return;
	}

	ServerRequestAdvanceDialogue();
}

void AARPlayerController::ServerRequestAdvanceDialogue_Implementation()
{
	RequestAdvanceDialogue();
}

void AARPlayerController::RequestSubmitDialogueChoice(FGuid ChoiceBranchId)
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->SubmitDialogueChoice(this, ChoiceBranchId);
		}
		return;
	}

	ServerRequestSubmitDialogueChoice(ChoiceBranchId);
}

void AARPlayerController::ServerRequestSubmitDialogueChoice_Implementation(FGuid ChoiceBranchId)
{
	RequestSubmitDialogueChoice(ChoiceBranchId);
}

void AARPlayerController::RequestSetDialogueEavesdrop(bool bEnable, EARPlayerSlot TargetSlot)
{
	if (HasAuthority())
	{
		if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
		{
			DialogueSubsystem->SetShopEavesdropTarget(this, TargetSlot, bEnable);
		}
		return;
	}

	ServerRequestSetDialogueEavesdrop(bEnable, TargetSlot);
}

void AARPlayerController::ServerRequestSetDialogueEavesdrop_Implementation(bool bEnable, EARPlayerSlot TargetSlot)
{
	RequestSetDialogueEavesdrop(bEnable, TargetSlot);
}

void AARPlayerController::RequestSetDialogueEavesdropOtherPlayer(bool bEnable)
{
	const AARPlayerStateBase* ARPS = GetPlayerState<AARPlayerStateBase>();
	if (!ARPS)
	{
		return;
	}

	EARPlayerSlot TargetSlot = EARPlayerSlot::Unknown;
	switch (ARPS->GetPlayerSlot())
	{
	case EARPlayerSlot::P1:
		TargetSlot = EARPlayerSlot::P2;
		break;
	case EARPlayerSlot::P2:
		TargetSlot = EARPlayerSlot::P1;
		break;
	default:
		break;
	}

	if (TargetSlot == EARPlayerSlot::Unknown)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue] RequestSetDialogueEavesdropOtherPlayer ignored: controller '%s' has unknown player slot."), *GetNameSafe(this));
		return;
	}

	RequestSetDialogueEavesdrop(bEnable, TargetSlot);
}

void AARPlayerController::ClientDialogueSessionUpdated_Implementation(const FDialogueClientView& View)
{
	const int32 OldChoiceIndex = SelectedDialogueChoiceIndex;
	CachedDialogueView = View;
	bHasCachedDialogueView = true;
	if (View.bWaitingForChoice && View.Choices.Num() > 0)
	{
		SelectedDialogueChoiceIndex = FMath::Clamp(
			(OldChoiceIndex == INDEX_NONE) ? 0 : OldChoiceIndex,
			0,
			View.Choices.Num() - 1);
	}
	else
	{
		SelectedDialogueChoiceIndex = INDEX_NONE;
	}
	if (SelectedDialogueChoiceIndex != OldChoiceIndex)
	{
		OnDialogueChoiceSelectionChanged.Broadcast(SelectedDialogueChoiceIndex, OldChoiceIndex);
	}
	OnDialogueViewUpdated.Broadcast(View);
	EnsureDialogueWidget();
	RefreshDialogueInputStateFromSession();
	BP_OnDialogueSessionUpdated(View);
}

void AARPlayerController::ClientDialogueSessionEnded_Implementation(const FString& SessionId)
{
	if (bHasCachedDialogueView && CachedDialogueView.SessionId == SessionId)
	{
		CachedDialogueView = FDialogueClientView();
		bHasCachedDialogueView = false;
	}
	SetSelectedDialogueChoiceIndex(INDEX_NONE);
	RefreshDialogueInputStateFromSession();
	OnDialogueSessionEndedSignal.Broadcast(SessionId);
	BP_OnDialogueSessionEnded(SessionId);
}

bool AARPlayerController::GetCachedDialogueView(FDialogueClientView& OutView) const
{
	OutView = bHasCachedDialogueView ? CachedDialogueView : FDialogueClientView();
	return bHasCachedDialogueView;
}

bool AARPlayerController::QueryLocalDialogueView(FDialogueClientView& OutView) const
{
	OutView = FDialogueClientView();
	if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
	{
		return DialogueSubsystem->GetLocalViewForController(this, OutView);
	}
	return false;
}

void AARPlayerController::RequestToggleDialogueAutoAdvance()
{
	AARPlayerStateBase* ARPS = GetPlayerState<AARPlayerStateBase>();
	if (!ARPS)
	{
		return;
	}

	ARPS->SetDialogueAutoAdvanceEnabled(!ARPS->IsDialogueAutoAdvanceEnabled());
}

bool AARPlayerController::GetSelectedDialogueChoiceBranchId(FGuid& OutChoiceBranchId) const
{
	OutChoiceBranchId.Invalidate();
	if (!bHasCachedDialogueView || !CachedDialogueView.bWaitingForChoice)
	{
		return false;
	}

	const int32 ChoiceCount = CachedDialogueView.Choices.Num();
	if (ChoiceCount <= 0)
	{
		return false;
	}

	const int32 ResolvedIndex = (SelectedDialogueChoiceIndex == INDEX_NONE)
		? 0
		: FMath::Clamp(SelectedDialogueChoiceIndex, 0, ChoiceCount - 1);
	const FDialogueChoiceView& Choice = CachedDialogueView.Choices[ResolvedIndex];
	if (!Choice.bCanChoose || !Choice.ChoiceBranchId.IsValid())
	{
		return false;
	}

	OutChoiceBranchId = Choice.ChoiceBranchId;
	return true;
}

void AARPlayerController::RequestAdvanceOrSubmitDialogue()
{
	if (bHasCachedDialogueView && CachedDialogueView.bWaitingForChoice)
	{
		FGuid SelectedBranchId;
		if (GetSelectedDialogueChoiceBranchId(SelectedBranchId))
		{
			RequestSubmitDialogueChoice(SelectedBranchId);
			return;
		}
	}

	RequestAdvanceDialogue();
}

void AARPlayerController::RequestDialogueChoiceDelta(const int32 Delta)
{
	if (!bHasCachedDialogueView || !CachedDialogueView.bWaitingForChoice || CachedDialogueView.Choices.Num() <= 0 || Delta == 0)
	{
		return;
	}

	const int32 ChoiceCount = CachedDialogueView.Choices.Num();
	const int32 CurrentIndex = (SelectedDialogueChoiceIndex == INDEX_NONE)
		? 0
		: FMath::Clamp(SelectedDialogueChoiceIndex, 0, ChoiceCount - 1);
	const int32 NewIndex = (CurrentIndex + Delta % ChoiceCount + ChoiceCount) % ChoiceCount;
	SetSelectedDialogueChoiceIndex(NewIndex);
}

void AARPlayerController::EnsureDialogueWidget()
{
	if (!IsLocalController() || !bAutoCreateDialogueWidget)
	{
		return;
	}

	if (DialogueWidget)
	{
		return;
	}

	if (!DialogueWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[Dialogue|UI] Auto dialogue widget skipped on '%s': DialogueWidgetClass is not set."), *GetNameSafe(this));
		return;
	}

	DialogueWidget = CreateWidget<UARDialogueWidgetBase>(this, DialogueWidgetClass);
	if (!DialogueWidget)
	{
		UE_LOG(ARLog, Warning, TEXT("[Dialogue|UI] Failed to create auto dialogue widget for '%s'."), *GetNameSafe(this));
		return;
	}

	DialogueWidget->InitializeDialogueWidget(this);
	DialogueWidget->AddToViewport(DialogueWidgetZOrder);
}

void AARPlayerController::RemoveDialogueWidget()
{
	if (!DialogueWidget)
	{
		return;
	}

	DialogueWidget->RemoveFromParent();
	DialogueWidget = nullptr;
}

void AARPlayerController::RequestHUDInitializationInternal(const bool bForceBroadcast)
{
	if (!IsLocalController())
	{
		StopHUDInitializationRetry();
		return;
	}

	AARHUDBase* ARHUD = Cast<AARHUDBase>(GetHUD());
	if (!ARHUD)
	{
		StartHUDInitializationRetry();
		if (!bForceBroadcast)
		{
			return;
		}
	}

	APlayerState* CurrentPlayerState = PlayerState;
	AGameStateBase* CurrentGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const bool bContextChanged = LastHUDInitPlayerState.Get() != CurrentPlayerState
		|| LastHUDInitGameState.Get() != CurrentGameState;
	if (!bForceBroadcast && bHasBroadcastHUDInitialization && !bContextChanged)
	{
		return;
	}

	bHasBroadcastHUDInitialization = true;
	LastHUDInitPlayerState = CurrentPlayerState;
	LastHUDInitGameState = CurrentGameState;
	StopHUDInitializationRetry();

	if (ARHUD)
	{
		ARHUD->RequestHUDInitialization(this, CurrentPlayerState, CurrentGameState);
	}

	BP_OnHUDInitializationRequested(this, CurrentPlayerState, CurrentGameState);
}

void AARPlayerController::StartHUDInitializationRetry()
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(HUDInitializationRetryTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		HUDInitializationRetryTimer,
		this,
		&AARPlayerController::HandleHUDInitializationRetry,
		0.1f,
		true);
}

void AARPlayerController::StopHUDInitializationRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HUDInitializationRetryTimer);
	}
}

void AARPlayerController::HandleHUDInitializationRetry()
{
	RequestHUDInitializationInternal(false);
}

void AARPlayerController::RequestOpenPauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldShareLocalPauseAcrossControllers())
	{
		if (IsPauseMenuBlockedLocal())
		{
			return;
		}

		SetPauseMenuOpenLocal(true);
		return;
	}

	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(GetWorld(), LocalControllers);
	if (LocalControllers.IsEmpty())
	{
		return;
	}

	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (!LocalController || LocalController->IsPauseMenuBlockedLocal())
		{
			return;
		}
	}

	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (LocalController)
		{
			LocalController->SetPauseMenuOpenLocal(true);
		}
	}
}

void AARPlayerController::RequestClosePauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldShareLocalPauseAcrossControllers())
	{
		SetPauseMenuOpenLocal(false);
		return;
	}

	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(GetWorld(), LocalControllers);
	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (LocalController)
		{
			LocalController->SetPauseMenuOpenLocal(false);
		}
	}
}

void AARPlayerController::ClosePause()
{
	RequestClosePauseMenu();
}

void AARPlayerController::RequestTogglePauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldShareLocalPauseAcrossControllers())
	{
		if (IsPauseMenuOpenLocal())
		{
			RequestClosePauseMenu();
		}
		else
		{
			RequestOpenPauseMenu();
		}
		return;
	}

	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(GetWorld(), LocalControllers);
	if (LocalControllers.IsEmpty())
	{
		return;
	}

	const bool bAnyOpen = LocalControllers.ContainsByPredicate(
		[](const AARPlayerController* LocalController)
		{
			return LocalController && LocalController->IsPauseMenuOpenLocal();
		});

	if (bAnyOpen)
	{
		RequestClosePauseMenu();
		return;
	}

	RequestOpenPauseMenu();
}

bool AARPlayerController::IsPauseMenuBlockedLocal() const
{
	if (!IsLocalPlayerController())
	{
		return true;
	}

	if (!PauseMenuBlockerReasons.IsEmpty())
	{
		return true;
	}

	return IsLobbyControllerMode() || IsDialogueSessionActiveLocal() || IsInvaderFullBlastSessionActiveLocal();
}

void AARPlayerController::SetPauseMenuBlocked(const bool bBlocked, FName Reason)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (Reason.IsNone())
	{
		Reason = TEXT("Generic");
	}

	if (bBlocked)
	{
		PauseMenuBlockerReasons.Add(Reason);
		if (bPauseMenuOpenLocal)
		{
			RequestClosePauseMenu();
		}
		return;
	}

	PauseMenuBlockerReasons.Remove(Reason);
}

void AARPlayerController::ServerSetPauseMenuVote_Implementation(const bool bPaused)
{
	SubmitPauseMenuVote(bPaused);
}

void AARPlayerController::SetPauseMenuOpenLocal(const bool bOpen)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bOpen && IsPauseMenuBlockedLocal())
	{
		return;
	}

	if (bPauseMenuOpenLocal == bOpen)
	{
		return;
	}

	bPauseMenuOpenLocal = bOpen;

	if (bOpen)
	{
		ApplyPauseInputContexts(true);
		const bool bShouldDisplayOverlay = ShouldDisplayPauseOverlayForLocalController();
		bPauseMenuOverlayVisibleLocal = bShouldDisplayOverlay ? ShowPauseOverlayWidget() : false;
		if (!bShouldDisplayOverlay)
		{
			HidePauseOverlayWidget();
		}

		if (bPauseMenuOverlayVisibleLocal)
		{
			ApplyPauseInputMode(true);
		}

		SubmitPauseMenuVote(true);
		OnPauseMenuOverlayVisibilityChanged.Broadcast(bPauseMenuOverlayVisibleLocal);
		OnPauseMenuStateChanged.Broadcast(true);
		BP_OnPauseMenuOpened(bPauseMenuOverlayVisibleLocal);
		return;
	}

	SubmitPauseMenuVote(false);
	HidePauseOverlayWidget();

	const bool bWasShowingOverlay = bPauseMenuOverlayVisibleLocal;
	bPauseMenuOverlayVisibleLocal = false;
	if (bWasShowingOverlay)
	{
		OnPauseMenuOverlayVisibilityChanged.Broadcast(false);
	}

	ApplyPauseInputContexts(false);
	if (bPauseInputModeApplied)
	{
		ApplyPauseInputMode(false);
	}
	OnPauseMenuStateChanged.Broadcast(false);
	BP_OnPauseMenuClosed();
}

void AARPlayerController::ApplyDefaultInputMappings(const bool bEnable)
{
	if (!bAutoApplyDefaultInputMappings || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	if (bEnable)
	{
		if (bDefaultInputMappingsApplied)
		{
			return;
		}

		for (const FARControllerInputMapping& Mapping : DefaultInputMappings)
		{
			if (Mapping.MappingContext)
			{
				InputSubsystem->AddMappingContext(Mapping.MappingContext, Mapping.Priority);
			}
		}

		bDefaultInputMappingsApplied = true;
		return;
	}

	if (!bDefaultInputMappingsApplied)
	{
		return;
	}

	for (const FARControllerInputMapping& Mapping : DefaultInputMappings)
	{
		if (Mapping.MappingContext)
		{
			InputSubsystem->RemoveMappingContext(Mapping.MappingContext);
		}
	}

	bDefaultInputMappingsApplied = false;
}

void AARPlayerController::ApplyPauseInputContexts(const bool bEnable)
{
	if (!bAutoManagePauseInputContexts || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	if (bEnable)
	{
		if (bPauseInputContextsApplied)
		{
			return;
		}
		ApplyDefaultInputMappings(false);

		if (PauseMenuInputMappingContext)
		{
			InputSubsystem->AddMappingContext(PauseMenuInputMappingContext, PauseMenuInputPriority);
		}

		bPauseInputContextsApplied = true;
		return;
	}

	if (!bPauseInputContextsApplied)
	{
		return;
	}

	if (PauseMenuInputMappingContext)
	{
		InputSubsystem->RemoveMappingContext(PauseMenuInputMappingContext);
	}
	ApplyDefaultInputMappings(true);

	bPauseInputContextsApplied = false;
}

void AARPlayerController::ApplyPauseInputMode(const bool bEnable)
{
	if (!bAutoManagePauseInputMode || !IsLocalPlayerController())
	{
		return;
	}

	if (bEnable)
	{
		bCachedShowMouseCursorForPause = bShowMouseCursor;
		bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;
		if (PauseOverlayWidget)
		{
			InputMode.SetWidgetToFocus(PauseOverlayWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bPauseInputModeApplied = true;
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = bCachedShowMouseCursorForPause;
	bPauseInputModeApplied = false;
}

void AARPlayerController::ApplyDialogueInputContexts(const bool bEnable)
{
	if (!bAutoManageDialogueInputContexts || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	if (bEnable)
	{
		if (bDialogueInputContextsApplied)
		{
			return;
		}

		ApplyDefaultInputMappings(false);
		if (DialogueInputMappingContext)
		{
			InputSubsystem->AddMappingContext(DialogueInputMappingContext, DialogueInputPriority);
		}
		bDialogueInputContextsApplied = true;
		return;
	}

	if (!bDialogueInputContextsApplied)
	{
		return;
	}

	if (DialogueInputMappingContext)
	{
		InputSubsystem->RemoveMappingContext(DialogueInputMappingContext);
	}
	ApplyDefaultInputMappings(true);
	bDialogueInputContextsApplied = false;
}

void AARPlayerController::ApplyDialogueInputMode(const bool bEnable)
{
	if (!bAutoManageDialogueInputMode || !IsLocalPlayerController())
	{
		return;
	}

	if (bEnable)
	{
		if (bDialogueInputModeApplied)
		{
			return;
		}

		bCachedShowMouseCursorForDialogue = bShowMouseCursor;
		bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;
		if (DialogueWidget)
		{
			InputMode.SetWidgetToFocus(DialogueWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bDialogueInputModeApplied = true;
		return;
	}

	if (!bDialogueInputModeApplied)
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = bCachedShowMouseCursorForDialogue;
	bDialogueInputModeApplied = false;
}

void AARPlayerController::RefreshDialogueInputStateFromSession()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	FDialogueClientView LocalView;
	const bool bDialogueActive = QueryLocalDialogueView(LocalView) || bHasCachedDialogueView;
	ApplyDialogueInputContexts(bDialogueActive);
	if (bDialogueActive)
	{
		ApplyDialogueInputMode(true);
		return;
	}

	if (bDialogueInputModeApplied)
	{
		ApplyDialogueInputMode(false);
	}
}

void AARPlayerController::SetSelectedDialogueChoiceIndex(const int32 NewIndex)
{
	if (SelectedDialogueChoiceIndex == NewIndex)
	{
		return;
	}

	const int32 OldIndex = SelectedDialogueChoiceIndex;
	SelectedDialogueChoiceIndex = NewIndex;
	OnDialogueChoiceSelectionChanged.Broadcast(SelectedDialogueChoiceIndex, OldIndex);
}

void AARPlayerController::NotifyInteractionActionCue(const EARInteractionActionCue ActionCue, AActor* ActionTarget)
{
	OnInteractionActionCue.Broadcast(ActionCue, ActionTarget);
}

bool AARPlayerController::ShowPauseOverlayWidget()
{
	if (!PauseOverlayWidgetClass)
	{
		UE_LOG(ARLog, Verbose, TEXT("[UI|Pause] PauseOverlayWidgetClass is not configured for '%s'."), *GetNameSafe(this));
		return false;
	}

	if (!PauseOverlayWidget || PauseOverlayWidget->GetClass() != PauseOverlayWidgetClass)
	{
		HidePauseOverlayWidget();
		PauseOverlayWidget = CreateWidget<UUserWidget>(this, PauseOverlayWidgetClass);
	}

	if (!PauseOverlayWidget)
	{
		UE_LOG(ARLog, Warning, TEXT("[UI|Pause] Failed to create pause overlay widget for '%s'."), *GetNameSafe(this));
		return false;
	}

	if (!PauseOverlayWidget->IsInViewport())
	{
		PauseOverlayWidget->AddToViewport(PauseOverlayWidgetZOrder);
	}

	return PauseOverlayWidget->IsInViewport();
}

void AARPlayerController::HidePauseOverlayWidget()
{
	if (PauseOverlayWidget && PauseOverlayWidget->IsInViewport())
	{
		PauseOverlayWidget->RemoveFromParent();
	}
}

void AARPlayerController::SubmitPauseMenuVote(const bool bPaused)
{
	AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	AARPlayerStateBase* ARPlayerState = GetPlayerState<AARPlayerStateBase>();
	if (!ARGameState || !ARPlayerState)
	{
		return;
	}

	const EARPlayerSlot PlayerSlot = ARPlayerState->GetPlayerSlot();
	if (PlayerSlot == EARPlayerSlot::Unknown)
	{
		return;
	}

	if (HasAuthority())
	{
		ARGameState->SetPlayerPauseMenuVote(PlayerSlot, bPaused);
		return;
	}

	ServerSetPauseMenuVote(bPaused);
}

bool AARPlayerController::ShouldDisplayPauseOverlayForLocalController() const
{
	if (!IsLocalPlayerController())
	{
		return false;
	}

	if (!ShouldShareLocalPauseAcrossControllers())
	{
		return true;
	}

	return ResolveSharedPauseOverlayOwner(GetWorld()) == this;
}

bool AARPlayerController::ShouldShareLocalPauseAcrossControllers() const
{
	const AARGameStateBase* ARGameState = GetWorld() ? GetWorld()->GetGameState<AARGameStateBase>() : nullptr;
	if (ARGameState)
	{
		return ARGameState->ShouldShareLocalPauseAcrossControllers();
	}

	// Safe default before GameState initialization: do not fan out.
	return false;
}

bool AARPlayerController::IsLobbyControllerMode() const
{
	return IsA<AARLobbyPlayerController>();
}

bool AARPlayerController::IsDialogueSessionActiveLocal() const
{
	if (UARDialogueSubsystem* DialogueSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARDialogueSubsystem>() : nullptr)
	{
		FDialogueClientView View;
		return DialogueSubsystem->GetLocalViewForController(this, View);
	}

	return false;
}

bool AARPlayerController::IsInvaderFullBlastSessionActiveLocal() const
{
	if (const AARInvaderGameState* InvaderGameState = GetWorld() ? GetWorld()->GetGameState<AARInvaderGameState>() : nullptr)
	{
		return InvaderGameState->GetFullBlastSession().bIsActive;
	}

	return false;
}

void AARPlayerController::GatherLocalARPlayerControllers(UWorld* World, TArray<AARPlayerController*>& OutControllers)
{
	OutControllers.Reset();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AARPlayerController* ARController = Cast<AARPlayerController>(It->Get()))
		{
			if (ARController->IsLocalPlayerController())
			{
				OutControllers.Add(ARController);
			}
		}
	}
}

AARPlayerController* AARPlayerController::ResolveSharedPauseOverlayOwner(UWorld* World)
{
	TArray<AARPlayerController*> LocalControllers;
	GatherLocalARPlayerControllers(World, LocalControllers);
	if (LocalControllers.IsEmpty())
	{
		return nullptr;
	}

	for (AARPlayerController* LocalController : LocalControllers)
	{
		if (const AARPlayerStateBase* ARPlayerState = LocalController->GetPlayerState<AARPlayerStateBase>())
		{
			if (ARPlayerState->GetPlayerSlot() == EARPlayerSlot::P1)
			{
				return LocalController;
			}
		}
	}

	return LocalControllers[0];
}
